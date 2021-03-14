// Copyright Ian Wakeling 2021
// License MIT

#if !defined TOKENISER_H
#define TOKENISER_H

#include <string>

class Tokeniser
{
public:
  Tokeniser(std::string input)
    : input_(std::move(input))
    , pos_(0)
  {
  }

  std::pair<bool,std::string> next()
  {
    std::pair<bool,std::string> result{false,""};
    if( pos_ != std::string::npos )
    {
      auto start = pos_;
      pos_ = input_.find(',', start);
      result.first = true;
      result.second = input_.substr(start, pos_ - start);
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
