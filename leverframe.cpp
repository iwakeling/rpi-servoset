// Copyright Ian Wakeling 2021
// License MIT

#include "leverframe.h"

#include <iostream>
#include <fstream>
#include <map>
#include <string>

#include <SDL2/SDL2_gfxPrimitives.h>

class FieldEditor
{
public:
  FieldEditor(
    int curr,
    int min,
    int max,
    std::function<void()> enterField,
    std::function<void(int newValue)> changeValue,
    std::function<void(bool changed, int finalValue)> exitField)
    : curr_(curr)
    , min_(min)
    , max_(max)
    , active_(false)
    , changed_(false)
    , enterField_(std::move(enterField))
    , changeValue_(std::move(changeValue))
    , exitField_(std::move(exitField))
  {
  }

  ~FieldEditor()
  {
    if( active_ )
    {
      exit();
    }
  }

  void enter()
  {
    if( enterField_ )
    {
      enterField_();
    }
    active_ = true;
  }

  void exit()
  {
    active_ = false;
    if( exitField_ )
    {
      exitField_(changed_, curr_);
    }
  }

  void left()
  {
    if( curr_ > min_ )
    {
      curr_--;
      changed_ = true;
      if( changeValue_ )
      {
        changeValue_(curr_);
      }
    }
  }

  void right()
  {
    if( curr_ < max_ )
    {
      curr_++;
      changed_ = true;
      if( changeValue_ )
      {
        changeValue_(curr_);
      }
    }
  }

  int current()
  {
    return curr_;
  }

private:
  int curr_;
  int min_;
  int max_;
  bool active_;
  bool changed_;
  std::function<void()> enterField_;
  std::function<void(int newValue)> changeValue_;
  std::function<void(bool changed, int finalValue)> exitField_;
};

LeverFrame::LeverFrame(
  std::string const& framePath,
  std::string const& fontFile,
  SDL_Rect const& pos,
  ServoController& servoController)
  : framePath_(framePath)
  , pos_(pos)
  , leverFont_(sdl::ttf::open_font(fontFile.c_str(), 18))
{
  if( !leverFont_ )
  {
    std::string msg = "Failed to open font " + fontFile + + ": " + TTF_GetError();
    throw std::runtime_error(msg);
  }

  loadFrame(servoController);

  leverSelector_ = std::make_shared<FieldEditor>(
    0,
    0,
    levers_.size() - 1,
    std::function<void()>(),
    std::function<void(int)>(),
    std::function<void(bool,int)>());
  currentField_ = leverSelector_;
}

LeverFrame::~LeverFrame()
{
  saveFrame();
}

void LeverFrame::render(sdl::renderer const& renderer)
{
  SDL_Rect framePos = pos_;
  framePos.h = Lever::height() * 3 / 2;
  sdl::render_set_colour(renderer, sdl::grey);
  SDL_RenderFillRect(renderer.get(), &framePos);

  for(int i = 0; i < levers_.size(); i++)
  {
    levers_[i].render(renderer, i == leverSelector_->current());
  }
}

void LeverFrame::handleLeft()
{
  currentField_->left();
}

void LeverFrame::handleRight()
{
  currentField_->right();
}

void LeverFrame::handleUp()
{
  if( currentField_ != leverSelector_ )
  {
    changeField(levers_[leverSelector_->current()].prevField());
  }

  if( !currentField_ )
  {
    changeField(leverSelector_);
  }
}

void LeverFrame::handleDown()
{
  auto next = levers_[leverSelector_->current()].nextField();

  if( next )
  {
    changeField(next);
  }
}

void LeverFrame::loadFrame(ServoController& servoController)
{
  std::ifstream is(framePath_);
  while( is )
  {
    std::string line;
    std::getline(is, line);
    if( !line.empty() && line[0] != '#' )
    {
      Tokeniser fields(line);
      try
      {
        levers_.emplace_back(
          servoController,
          fields,
          leverFont_,
          pos_.x + levers_.size() * Lever::spacing(),
          pos_.y + Lever::height() / 4);
      }
      catch(...)
      {
        std::cerr << "Lever "
                  << levers_.size()
                  << " incorrectly formatted: "
                  << line
                  << std::endl;
      }
    }
  }
}

void LeverFrame::saveFrame()
{
  std::ofstream os(framePath_);
  os << "# Name,Board,servo,Type(S|P|F|-),Normal-pos,Reversed-pos,Pull-speed,"
     << "Return-speed,Description" << std::endl;
  for( auto&& lever : levers_)
  {
    lever.write(os);
  }
}

void LeverFrame::changeField(std::shared_ptr<FieldEditor> newField)
{
  if( currentField_ )
  {
    currentField_->exit();
  }
  currentField_ = std::move(newField);
  if( currentField_ )
  {
    currentField_->enter();
  }
}

LeverFrame::Lever::Lever(
  ServoController& servoController,
  Tokeniser& values,
  sdl::ttf::font const& font,
  int x,
  int y)
  : servoController_(servoController)
  , font_(font)
  , currField_(nullptr)
{
  auto fieldSpace = TTF_FontLineSkip(font.get());
  auto hposBase = y
  + height() * 3 / 2 // below levers + margin
  + fieldSpace; // skip space for description

  fields_.emplace_back(
    0, // name appears on the plate on the lever
    Field::Simple,
    0,
    0,
    0,
    values.next().value());
  fields_[0].adjustPos_ = [x,y](Field* field)
                          {
                              field->pos_.x = x + (spacing() - field->pos_.w) / 2;
                              field->pos_.y = y + height() / 3;
                          };

  fields_.emplace_back(
    hposBase += fieldSpace,
    Field::Integer,
    0,
    0,
    std::stoi(values.next().value()),
    "Board: ");
  connector_ = std::stoi(values.next().value());
  fields_.emplace_back(
    hposBase += fieldSpace,
    Field::Integer,
    0,
    0,
    connector_,
    "Connector: ");
  auto type_str = values.next().value();
  type_ = Lever::to_type(type_str);
  fields_.emplace_back(0, Field::Hidden, 0, 0, 0, type_str);
  fields_.emplace_back(
    hposBase += fieldSpace,
    Field::Editable | Field::Integer,
    0,
    255,
    std::stoi(values.next().value()),
    "Normal Position: ",
    ServoController::Normal,
    ServoController::Position);
  fields_.emplace_back(
    hposBase += fieldSpace,
    Field::Editable | Field::Integer,
    0,
    255,
    std::stoi(values.next().value()),
    "Reversed Position: ",
    ServoController::Reversed,
    ServoController::Position);
  fields_.emplace_back(
    hposBase += fieldSpace,
    Field::Editable | Field::Integer,
    0,
    6,
    std::stoi(values.next().value()),
    "Pull Speed: ",
    ServoController::Normal,
    ServoController::Speed);
  fields_.emplace_back(
    hposBase += fieldSpace,
    Field::Editable | Field::Integer,
    0,
    6,
    std::stoi(values.next().value()),
    "Return Speed: ",
    ServoController::Reversed,
    ServoController::Speed);
  fields_.emplace_back(
    y + height() * 3 / 2, // description appears at top
    Field::Simple,
    0,
    0,
    0,
    values.remainder());

  handlePos_ =
  {
    x + (spacing() / 4),
    y,
    spacing() / 2,
    height() / 3
  };
  leverPos_ =
  {
    x + (spacing() / 8),
    y + (height() / 3),
    spacing() * 3 / 4,
    height() * 2 / 3
  };
  auto selectHeight = height() / 12; // height()/4 is the bottom margin
  selectPos_ =
  {
    x,
    y + height() + selectHeight,
    spacing(),
    selectHeight
  };
}

namespace
{
  struct FieldSep
  {
    bool active_;

    FieldSep() : active_(false) {}
  };

  std::ostream& operator <<(std::ostream& os, FieldSep& sep)
  {
    if( sep.active_ )
    {
      os << ',';
    }
    else
    {
      sep.active_ = true;
    }
    return os;
  }
}

void LeverFrame::Lever::write(std::ostream& os)
{
  FieldSep sep;
  for(auto&& field : fields_)
  {
    os << sep;
    if( (field.flags_ & Field::Integer) != 0 )
    {
      os << field.cur_;
    }
    else
    {
      os << field.str_;
    }
  }
  os << std::endl;
}

void LeverFrame::Lever::render(sdl::renderer const& renderer, bool selected)
{
  static SDL_Color const colours[] =
  {
    { 0xAA, 0x00, 0x00 },
    sdl::black,
    { 0x00, 0x00, 0xAA },
    sdl::white
  };

  for( auto&& field: fields_ )
  {
    if( (field.flags_ & Field::Hidden) == 0 && !field.texture_ )
    {
      std::string valStr;
      auto& str = (field.flags_ & Field::Integer) != 0
        ? valStr = field.str_ + std::to_string(field.cur_)
        : field.str_;
      auto surface = sdl::ttf::render_blended(font_, str, sdl::grey);
      field.texture_ = sdl::create_texture_from_surface(renderer, surface);
      sdl::ttf::size(font_, str, &field.pos_.w, &field.pos_.h);
      if( field.adjustPos_ )
      {
        field.adjustPos_(&field);
      }
    }
  }

  sdl::render_set_colour(renderer, colours[static_cast<int>(type_)]);
  SDL_RenderFillRect(renderer.get(), &leverPos_);
  SDL_RenderFillRect(renderer.get(), &handlePos_);

  auto first = &fields_[0];
  auto last = &fields_[1];

  if( selected )
  {
    SDL_RenderFillRect(renderer.get(), &selectPos_);
    last = &fields_[fields_.size()];
  }

  auto& plate_pos = fields_[0].pos_;
  filledEllipseRGBA(
    renderer.get(),
    plate_pos.x + plate_pos.w / 2,
    plate_pos.y + plate_pos.h / 2,
    plate_pos.w / 2,
    plate_pos.h / 2,
    sdl::white.r,
    sdl::white.g,
    sdl::white.b,
    SDL_ALPHA_OPAQUE);
  ellipseRGBA(
    renderer.get(),
    plate_pos.x + plate_pos.w / 2,
    plate_pos.y + plate_pos.h / 2,
    plate_pos.w / 2,
    plate_pos.h / 2,
    sdl::black.r,
    sdl::black.g,
    sdl::black.b,
    SDL_ALPHA_OPAQUE);

  for( auto field = first; field < last; field++ )
  {
    if( field == currField_ )
    {
      filledTrigonRGBA(
        renderer.get(),
        field->pos_.x + field->pos_.w + 10,
        field->pos_.y + field->pos_.h / 2,
        field->pos_.x + field->pos_.w + 20,
        field->pos_.y,
        field->pos_.x + field->pos_.w + 20,
        field->pos_.y + field->pos_.h,
        sdl::white.r,
        sdl::white.g,
        sdl::white.b,
        SDL_ALPHA_OPAQUE);
      filledTrigonRGBA(
        renderer.get(),
        field->pos_.x + field->pos_.w + 30,
        field->pos_.y,
        field->pos_.x + field->pos_.w + 40,
        field->pos_.y + field->pos_.h / 2,
        field->pos_.x + field->pos_.w + 30,
        field->pos_.y + field->pos_.h,
        sdl::white.r,
        sdl::white.g,
        sdl::white.b,
        SDL_ALPHA_OPAQUE);
    }
    sdl::render_copy(renderer, field->texture_, nullptr, &field->pos_);
  }
}

std::shared_ptr<FieldEditor> LeverFrame::Lever::nextField()
{
  auto field = currField_ == nullptr ? &fields_[0] : currField_;
  while( field++ < &fields_[fields_.size() - 1] )
  {
    if( field->flags_ & Field::Editable )
    {
      currField_ = field;
      return makeFieldEditor(field);
    }
  }
  return {};
}

std::shared_ptr<FieldEditor> LeverFrame::Lever::prevField()
{
  auto field = currField_;
  while( field-- > &fields_[0] )
  {
    if( field->flags_ & Field::Editable )
    {
      currField_ = field;
      return makeFieldEditor(field);
    }
  }
  currField_ = nullptr;
  return {};
}

LeverFrame::Lever::Type LeverFrame::Lever::to_type(std::string const& str)
{
  static std::map<std::string,Type> const types =
  {
    { "S", Type::Signal },
    { "P", Type::Point },
    { "F", Type::FPL },
    { "-", Type::Spare }
  };

  return types.at(str);
}

int LeverFrame::Lever::spacing()
{
  return 24;
}

int LeverFrame::Lever::height()
{
  return 100;
}

std::shared_ptr<FieldEditor> LeverFrame::Lever::makeFieldEditor(Field* field)
{
  return std::make_shared<FieldEditor>(
    field->cur_,
    field->min_,
    field->max_,
    [this,field]()
    {
      servoController_.start(connector_,
                             field->direction_,
                             field->function_,
                             field->cur_);
    },
    [this,field](int newValue)
    {
      field->cur_ = newValue;
      field->texture_.reset();
      servoController_.update(newValue);
    },
    [this](bool, int)
    {
      servoController_.finish();
    });
}
