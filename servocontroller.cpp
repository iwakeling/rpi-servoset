// Copyright Ian Wakeling 2021
// License MIT

#include "servocontroller.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

using namespace std::chrono_literals;

ServoController::ServoController(std::string const& port)
  : fd_(-1)
  , stop_(false)
{
  if( !port.empty() )
  {
    fd_ = open(port.c_str(), O_WRONLY | O_NOCTTY);
    if( fd_ < 0 )
    {
      throw std::runtime_error(std::strerror(errno));
    }

    struct termios term_options;
    if( tcgetattr(fd_, &term_options) < 0 )
    {
      std::cerr << std::strerror(errno) << std::endl;
    }

    cfsetospeed(&term_options, B9600);
    term_options.c_cflag |= CLOCAL | CREAD;
    term_options.c_cflag &= ~(PARENB | PARODD);
    term_options.c_cflag &= ~CSTOPB;
    term_options.c_cflag &= ~CRTSCTS;
    term_options.c_cflag &= ~CSIZE;
    term_options.c_cflag |= CS8;
    term_options.c_oflag = 0;
    if( tcsetattr(fd_, TCSANOW, &term_options) < 0)
    {
      std::cerr << std::strerror(errno) << std::endl;
    }
  }
}

ServoController::~ServoController()
{
  if( fd_ > 0 )
  {
    close(fd_);
  };
}

void ServoController::start(
  unsigned int connection,
  Direction direction,
  Function function,
  unsigned int value)
{
  curValue_ = value;
  stopped_ = std::async(
    std::launch::async,
    [this, connection, direction, function]()
    {
      std::unique_lock<std::mutex> lock(guard_);
      while( !cv_.wait_for(lock, 100ms, [this]{ return stop_; }) )
      {
        send((0x41 + (connection * 4) + (function * 2) + direction), curValue_);
      }
      send(0x40, 0);
      stop_ = false;
    });
}

void ServoController::update(unsigned int newValue)
{
  std::lock_guard<std::mutex> lock(guard_);
  curValue_ = newValue;
}

void ServoController::finish()
{
  {
    std::lock_guard<std::mutex> lock(guard_);
    stop_ = true;
  }
  stopped_.get();
}

void ServoController::send(unsigned int cmd, unsigned int value) const
{
  // pkt format is:
  // nul cmd value
  char buf[6];
  buf[0] = 0;
  snprintf(buf + 1, 5, "%c%03d", cmd, value);
  if( fd_ > 0 )
  {
    write(fd_, buf, 5);
  }
}
