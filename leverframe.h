// Copyright Ian Wakeling 2021
// License MIT

#include "sdl2-cpp/sdl2.h"
#include "sdl2-cpp/ttf.h"

#include <functional>
#include <vector>

#include "servocontroller.h"
#include "tokeniser.h"

class FieldEditor;

class LeverFrame
{
public:
  LeverFrame(std::string const& framePath,
             std::string const& fontFile,
             SDL_Rect const& pos,
             ServoController& servoController);
  ~LeverFrame();

  void render(sdl::renderer const& renderer);

  void handleLeft();
  void handleRight();
  void handleUp();
  void handleDown();

private:
  void loadFrame(ServoController& servoController);
  void saveFrame();

  void changeField(std::shared_ptr<FieldEditor> newField);

  class Lever
  {
  public:
    enum class Type
    {
      Signal,
      Point,
      FPL,
      Spare
    };

  public:
    Lever(
      ServoController& servoController,
      Tokeniser& values,
      sdl::ttf::font const& font,
      int x,
      int y);

    void write(std::ostream& os);

    void render(sdl::renderer const& renderer, bool selected);

    std::shared_ptr<FieldEditor> nextField();
    std::shared_ptr<FieldEditor> prevField();

    static Type to_type(std::string const& str);
    static int spacing();
    static int height();

  private:
    struct Field
    {
      enum Flags
      {
        Simple = 0x00,
        Integer = 0x01,
        Editable = 0x02,
        Hidden = 0x04
      };

      Field(
        int hpos,
        int flags,
        int min,
        int max,
        int cur,
        std::string str,
        ServoController::Direction direction = ServoController::Normal,
        ServoController::Function function = ServoController::Position)
        : direction_(direction)
        , function_(function)
        , flags_(flags)
        , min_(min)
        , max_(max)
        , cur_(cur)
        , str_(std::move(str))
        , texture_(nullptr, nullptr)
      {
        pos_.x = 40;
        pos_.y = hpos;
      }

      ServoController::Direction direction_;
      ServoController::Function function_;
      int flags_;
      int min_;
      int max_;
      int cur_;
      std::string str_;
      sdl::texture texture_;
      SDL_Rect pos_;
      std::function<void(Field*)> adjustPos_;
    };
    std::shared_ptr<FieldEditor> makeFieldEditor(Field* field);

    ServoController& servoController_;
    sdl::ttf::font const& font_;
    SDL_Rect handlePos_;
    SDL_Rect leverPos_;
    SDL_Rect selectPos_;
    Type type_;
    int connector_;
    std::vector<Field> fields_;
    Field* currField_;
  };

  std::string framePath_;
  SDL_Rect pos_;
  sdl::ttf::font leverFont_;
  std::vector<Lever> levers_;
  std::shared_ptr<FieldEditor> leverSelector_;
  std::shared_ptr<FieldEditor> currentField_;
};
