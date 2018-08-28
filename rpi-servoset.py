#!/usr/bin/python2

import pygame, serial, sys, os
from pygame.locals import *
from time import sleep

WHITE       = ( 255, 255, 255 )
BLACK       = (   0,   0,   0 )
RED         = ( 255,   0,   0 )
BLUE        = (   0,   0, 128 )
GREY        = ( 128, 128, 128 )

BGCOLOUR = BLACK
PANELCOLOUR = GREY

# button configuration for RPi
TASK_SWITCH_PIN = int(11) #blue
EXIT_PIN = int(15) #red
DOWN_PIN = int(16) #white
UP_PIN = int(13) #yellow
SELECT_PIN = int(22) #green

LeverTypes = {'S': RED,
              'P': BLACK,
              'F': BLUE,
              '-': WHITE}

def drawText(surface, font, colour, left, top, text ):
  block = font.render( text, True, colour )
  blockRect = block.get_rect()
  blockRect.topleft = ( left, top )
  surface.blit( block, blockRect )
  return blockRect.width, blockRect.height

class Button:
  def __init__(self, GPIO, pin, bounceTime):
    self.pin = pin
    GPIO.setup(pin, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)

    def postButtonEvent(pin):
      evt = pygame.event.Event(pygame.USEREVENT, {"pin": pin})
      pygame.fastevent.post(evt)

    GPIO.add_event_detect(
      pin,
      GPIO.RISING,
      callback=postButtonEvent,
      bouncetime=bounceTime)


class ServoController:
  # direction
  NORMAL = 0
  REVERSED = 1

  #function
  POSITION = 0
  SPEED = 1

  SERVO_A = 0
  SERVO_B = 1
  SERVO_C = 2
  SERVO_D = 3

  COMMAND_BASE = 0x41

  def __init__(self, portName):
    try:
      self.port = serial.Serial(portName)
    except serial.SerialException,e:
      self.port = None
      print(
        "Failed to open serial port {}: {}".format(
          portName,
          e))

  def set(self):
    print('\0%c000' % '\x40')
    if self.port is not None:
      self.port.write(b'\0%c000' % '\x40')

  def reset(self):
    print('\0%c000' % '\x23')
    if self.port is not None:
      self.port.write(b'\0%c000' % '\x23')

  def sendCommand(self, servo, direction, function, value):
    cmd = (self.COMMAND_BASE +
           servo * 4 +
           function * 2 +
           direction)
    print('\0%c%03d' % (cmd, value))
    if self.port is not None:
      self.port.write(b'\0%c%03d' % (cmd, value))


class Lever:
  # state
  IDLE = 0
  NORMAL = 1
  REVERSED = 2
  RETURN = 3
  PULL = 4
  NUM_STATES = 5

  def __init__(
      self,
      sc,
      board,
      idx,
      number,
      leverType,
      normal,
      reverse,
      pullSpeed,
      returnSpeed):
    self.sc = sc
    self.board = board
    self.idx = idx
    self.number = number
    self.leverType = leverType
    self.colour = LeverTypes[leverType]
    self.vals = [0, normal, reverse, pullSpeed, returnSpeed]
    self.state = self.IDLE
    self.val = 0
    self.changed = False

  def sendCommand(self):
    self.sc.sendCommand(
      self.idx,
      1 - self.state % 2,
      self.state / 3,
      self.vals[self.state])

  def update(self, step):
    if self.state == self.IDLE:
      return False
    else:
      if (step > 0 and self.vals[self.state] < 255 or
          step < 0 and self.vals[self.state] > 0):
        self.vals[self.state] = self.vals[self.state] + step
        self.sendCommand()
        self.changed = True
      return True

  def handleYellow(self):
    return self.update(-1)

  def handleWhite(self):
    return self.update(1)

  def handleGreen(self):
    if self.state != self.IDLE and self.changed:
      self.sc.set()
      self.changed = False
    self.state = (self.state + 1) % self.NUM_STATES

  def draw(self, surface, font, leverWidth, leverHeight, current):
    (plateWidth,plateHeight) = font.size("00")
    selectRect = Rect(
      leverWidth - 2 + 2 * leverWidth * (self.number -1),
      2,
      leverWidth + 2,
      leverHeight - 2)
    handleRect = Rect(
      (leverWidth * 5 / 4) + 2 * leverWidth * (self.number - 1),
      4,
      leverWidth / 2,
      leverHeight / 4)
    leverRect = Rect(
      leverWidth + 2 * leverWidth * (self.number -1),
      leverHeight / 4,
      leverWidth,
      leverHeight * 3 / 4 - 8)
    plateRect = Rect(
      leverRect.left + leverWidth / 2 - plateWidth / 2,
      leverHeight / 4,
      plateWidth,
      plateHeight)

    if current:
      pygame.draw.rect(
        surface,
        self.colour,
        selectRect,
        1)

      detailsLeft = 4
      detailsTop = leverHeight + 4

      items = [
        (self.NUM_STATES + 1, "Lever {}".format(self.number)),
        (self.NUM_STATES + 1, ""),
        (self.NUM_STATES + 1, "Board {}".format(self.board)),
        (self.NUM_STATES + 1, "Index {}".format(self.idx)),
        (self.NORMAL, "Normal Pos: {}".format(self.vals[self.NORMAL])),
        (self.REVERSED, "Reversed Pos: {}".format(self.vals[self.REVERSED])),
        (self.RETURN, "Return Speed: {}".format(self.vals[self.RETURN])),
        (self.PULL, "Pull Speed: {}".format(self.vals[self.PULL]))]

      for (state, string) in items:
        (w,h) = drawText(
          surface,
          font,
          WHITE,
          detailsLeft,
          detailsTop,
          string)
        if state == self.state:
          pygame.draw.line(
            surface,
            WHITE,
            (detailsLeft, detailsTop + font.get_ascent()),
            (detailsLeft + w, detailsTop + font.get_ascent()))
        detailsTop = detailsTop + h * 2

    pygame.draw.rect(
      surface,
      self.colour,
      handleRect,
      0)
    pygame.draw.rect(
      surface,
      self.colour,
      leverRect,
      0)

    pygame.draw.ellipse(
      surface,
      WHITE,
      plateRect,
      0)
    tBlock = font.render(
      "%d" % self.number,
      True,
      BLACK)
    tBlockRect = tBlock.get_rect()
    tBlockRect.left = plateRect.left + (plateRect.width - tBlockRect.width) / 2
    tBlockRect.top = plateRect.top + (plateRect.height - tBlockRect.height) / 2
    surface.blit(tBlock, tBlockRect)


def main():
  if len(sys.argv) < 3:
    print("Usage\n{} <port> <frame-file>\n".format(sys.argv[0]))
    exit(1)

  haveRPi = False
  try:
    import RPi.GPIO as GPIO
    haveRPi = True
    GPIO.setmode(GPIO.BOARD)
    upButton = Button(GPIO, UP_PIN, 1000)
    downButton = Button(GPIO, DOWN_PIN, 1000)
    selectButton = Button(GPIO, SELECT_PIN, 1000)
    exitButton = Button(GPIO, EXIT_PIN, 1000)
    taskSwitchButton = Button(GPIO, TASK_SWITCH_PIN, 1000)
  except ImportError, e:
    print("No RPi module, proceeding without")


  pygame.init()
  if haveRPi:
    pygame.fastevent.init()
  pygame.mouse.set_visible(False)
  pygame.display.set_caption('Servo4 Setup')
  surface = pygame.display.set_mode((600,480))#, pygame.FULLSCREEN)
  font = pygame.font.Font( 'freesansbold.ttf', 18 )

  sc = ServoController(sys.argv[1])
  levers = []

  frame = open(sys.argv[2] + ".frame")
  for line in frame:
    line = line.strip()
    if line != "" and line[0] != '#':
      fields = line.split(',')
      if len(fields) == 7:
        levers.append(
          Lever(
            sc,
            int(fields[0]),
            int(fields[1]),
            len(levers) + 1,
            fields[2],
            int(fields[3]),
            int(fields[4]),
            int(fields[5]),
            int(fields[6])))
      else:
        print("Lever %d incorrectly formatted" % len(levers) + 1)
  frame.close()

  maxLeverWidth = surface.get_width() / (1 + len(levers) * 2)
  maxLeverHeight = surface.get_height() / 4
  leverWidth = 16
  leverHeight = 100
  if leverWidth > maxLeverWidth:
    leverWidth = maxLeverWidth
  if leverHeight > maxLeverHeight:
    leverHeight = maxLeverHeight

  current = 0

  done = False
  taskSwitch = False
  while not done:
    surface.fill(BGCOLOUR)
    surface.fill(
      PANELCOLOUR,
      Rect(
        0,
        0,
        surface.get_width(),
        leverHeight))

    curLever = levers[current]

    for lever in levers:
      lever.draw(surface, font, leverWidth, leverHeight, lever == curLever)
    pygame.display.update()

    # for event in pygame.event.get():
    event = pygame.event.wait()
    if event.type == pygame.KEYDOWN:
      if event.key == pygame.K_ESCAPE:
        done = True
      elif event.key == pygame.K_w:
        if not curLever.handleYellow() and current < (len(levers) - 1):
          current = current + 1
      elif event.key == pygame.K_y:
        if not curLever.handleWhite() and current > 0:
          current = current - 1
      elif event.key == pygame.K_g:
        curLever.handleGreen()
    elif event.type == pygame.USEREVENT:
      if event.pin == EXIT_PIN:
        done = True
      elif event.pin == TASK_SWITCH_PIN:
        done = True
        taskSwitch = True
      elif event.pin == UP_PIN:
        if not curLever.handleYellow() and current < (len(levers) - 1):
          current = current + 1
      elif event.pin == DOWN_PIN:
        if not curLever.handleWhite() and current > 0:
          current = current - 1
      elif event.pin == SELECT_PIN:
        curLever.handleGreen()

    pygame.time.Clock().tick(5)

  print("Writing frame with {} levers".format(len(levers)))
  frame = open(sys.argv[2] + ".frame", "w")
  frame.write("# Board,Servo,Type(S|P|F|-),Normal,Reversed,Pull,Return\n")
  for lever in levers:
    frame.write(
      "{},{},{},{},{},{},{}\n".format(
        lever.board,
        lever.idx,
        lever.leverType,
        lever.vals[lever.NORMAL],
        lever.vals[lever.REVERSED],
        lever.vals[lever.PULL],
        lever.vals[lever.RETURN]))
  frame.close()

  pygame.quit()

  if taskSwitch:
    os.execl("stationmaster/stationmaster.py", "stationmaster.py", sys.argv[2])


if __name__ == '__main__':
  main()
