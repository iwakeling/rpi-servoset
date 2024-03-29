#!/usr/bin/python2

import pygame, serial, sys, os
from pygame.locals import *
from time import sleep

WHITE       = ( 255, 255, 255 )
BLACK       = (   0,   0,   0 )
RED         = ( 255,   0,   0 )
BLUE        = (   0,   0, 128 )
GREY        = ( 128, 128, 128 )
YELLOW      = (   0,  64,  64 )

BGCOLOUR = BLACK
PANELCOLOUR = GREY

# button configuration for RPi
EXIT_PIN = int(15) #red
UP_PIN = int(16) #white
DOWN_PIN = int(13) #yellow
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


class Debug:
  MAX_ITEMS = 5
  window = None

  def __init__(self):
    self.trace = []

  def setSurface(self, surface, font, left, top):
    self.surface = surface
    self.font = font
    self.left = left
    self.top = top

  def addTrace(self, marker):
    if self is not None:
      if len(self.trace) == self.MAX_ITEMS:
        self.trace = self.trace[1:-1]
      self.trace.append(marker)
      self. surface.fill(
        BGCOLOUR,
        Rect(
          self.left,
          self.top,
          self.surface.get_width() - self.left,
          self.surface.get_height() - self.top))
      self.draw()
      pygame.display.update()


  def draw(self):
    top = self.top
    for marker in self.trace:
      (w,h) = drawText(self.surface, self.font, YELLOW, self.left, top, marker)
      top = top + h


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
      idx,
      board,
      conn,
      number,
      leverType,
      normal,
      reverse,
      pullSpeed,
      returnSpeed,
      description):
    self.sc = sc
    self.idx = idx
    self.board = board
    self.conn = conn
    self.number = number
    self.leverType = leverType
    self.colour = LeverTypes[leverType]
    self.vals = [0, normal, reverse, returnSpeed, pullSpeed]
    self.description = description
    self.state = self.IDLE
    self.val = 0
    self.changed = False

  def sendCommand(self):
    self.sc.sendCommand(
      self.conn,
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
      leverWidth - 2 + 2 * leverWidth * (self.idx),
      2,
      leverWidth + 2,
      leverHeight - 2)
    handleRect = Rect(
      (leverWidth * 5 / 4) + 2 * leverWidth * (self.idx),
      4,
      leverWidth / 2,
      leverHeight / 4)
    leverRect = Rect(
      leverWidth + 2 * leverWidth * (self.idx),
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
        (self.NUM_STATES + 1, "Lever {} {}".format(self.number, self.description)),
        (self.NUM_STATES + 1, ""),
        (self.NUM_STATES + 1, "Board {}".format(self.board)),
        (self.NUM_STATES + 1, "Connector {}".format(self.conn)),
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
      "{}".format(self.number),
      True,
      BLACK)
    tBlockRect = tBlock.get_rect()
    tBlockRect.left = plateRect.left + (plateRect.width - tBlockRect.width) / 2
    tBlockRect.top = plateRect.top + (plateRect.height - tBlockRect.height) / 2
    surface.blit(tBlock, tBlockRect)


def main():
  args = sys.argv[1:]

  if len(args) > 0 and args[0] == "-d":
    print("Enabling debug window\n")
    Debug.window = Debug()
    args = args[1:]

  if len(args) != 2:
    print("Usage\n{} [-d] <port> <frame-file>\n".format(sys.argv[0]))
    print("got {} args: {}\n".format(len(args), args))
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
  except ImportError, e:
    print("No RPi module, proceeding without")


  pygame.init()
  if haveRPi:
    pygame.fastevent.init()
  pygame.mouse.set_visible(False)
  pygame.display.set_caption('Servo4 Setup')
  surface = pygame.display.set_mode((600,480))#, pygame.FULLSCREEN)
  font = pygame.font.Font( 'freesansbold.ttf', 18 )

  if Debug.window is not None:
    Debug.window.setSurface(surface, font, 300, 100)

  sc = ServoController(args[0])
  levers = []

  Debug.window.addTrace("Reading frame file")
  frame = open(args[1] + ".frame")
  for line in frame:
    line = line.strip()
    if line != "" and line[0] != '#':
      fields = line.split(',')
      if len(fields) == 9:
        levers.append(
          Lever(
            sc,
            len(levers),
            int(fields[1]),
            int(fields[2]),
            fields[0],
            fields[3],
            int(fields[4]),
            int(fields[5]),
            int(fields[6]),
            int(fields[7]),
            fields[8]))
      else:
        print("Lever {} incorrectly formatted".format(len(levers) + 1))
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
    if Debug.window != None:
      Debug.window.draw()
    pygame.display.update()

    # for event in pygame.event.get():
    event = pygame.event.wait()
    Debug.window.addTrace("got event")
    print("got event\n")
    if event.type == pygame.KEYDOWN:
      if event.key == pygame.K_ESCAPE:
        Debug.window.addTrace("escape pressed")
        done = True
      elif event.key == pygame.K_w:
        Debug.window.addTrace("w pressed")
        if not curLever.handleWhite() and current < (len(levers) - 1):
          current = current + 1
      elif event.key == pygame.K_y:
        Debug.window.addTrace("y pressed")
        if not curLever.handleYellow() and current > 0:
          current = current - 1
      elif event.key == pygame.K_g:
        Debug.window.addTrace("g pressed")
        curLever.handleGreen()
    elif event.type == pygame.USEREVENT:
      if event.pin == EXIT_PIN:
        Debug.window.addTrace("exit btn pressed")
        done = True
      elif event.pin == UP_PIN:
        Debug.window.addTrace("up btn pressed")
        if not curLever.handleWhite() and current < (len(levers) - 1):
          current = current + 1
      elif event.pin == DOWN_PIN:
        Debug.window.addTrace("down btn pressed")
        if not curLever.handleYellow() and current > 0:
          current = current - 1
      elif event.pin == SELECT_PIN:
        Debug.window.addTrace("select btn pressed")
        curLever.handleGreen()
      else:
        Debug.window.addTrace("unknown btn {} pressed".format(event.pin))

    pygame.time.Clock().tick(5)

  Debug.window.addTrace("Writing frame with {} levers".format(len(levers)))
  frame = open(args[1] + ".frame", "w")
  frame.write("# Board,Servo,Type(S|P|F|-),Normal,Reversed,Pull,Return\n")
  for lever in levers:
    frame.write(
      "{},{},{},{},{},{},{},{},{}\n".format(
        lever.number,
        lever.board,
        lever.conn,
        lever.leverType,
        lever.vals[lever.NORMAL],
        lever.vals[lever.REVERSED],
        lever.vals[lever.PULL],
        lever.vals[lever.RETURN],
        lever.description))
  frame.close()

  pygame.quit()

if __name__ == '__main__':
  main()
