// Copyright Ian Wakeling 2021
// License MIT

#if !defined TOKENISER_H
#define TOKENISER_H

#include <string>
#include <optional>

class Tokeniser
{
public:
  Tokeniser(std::string input)
    : input_(std::move(input))
    , pos_(0)
  {
  }

  std::optional<std::string> next()
  {
    std::optional<std::string> result;
    if( pos_ != std::string::npos )
    {
      auto start = pos_;
      pos_ = input_.find(',', start);
      result = std::optional<std::string>(input_.substr(start, pos_ - start));
      if( pos_ != std::string::npos )
      {
        pos_++;
      }
    }
    return result;
  }

  std::string remainder()
  {
    return input_.substr(pos_);
  }

private:
  std::string input_;
  std::string::size_type pos_;
};

#endif // !defined TOKENISER_H
