// Copyright Ian Wakeling 2021
// License MIT

#if !defined SERVOCONTROLLER_H
#define SERVOCONTROLLER_H

#include <condition_variable>
#include <iostream>
#include <future>
#include <mutex>

class ServoController
{
public:
  enum Direction
  {
    Normal,
    Reversed
  };

  enum Function
  {
    Position,
    Speed
  };

  ServoController(std::string const& port);
  ~ServoController();

  void start(
    unsigned int connection,
    Direction direction,
    Function function,
    unsigned int value);
  void update(unsigned int newValue);
  void finish();

private:
  void send(unsigned int cmd, unsigned int value) const;

private:
  int fd_;
  unsigned int curValue_;
  bool stop_;
  std::condition_variable cv_;
  std::mutex guard_;
  std::future<void> stopped_;
};

#endif // !defined SERVOCONTROLLER_H
