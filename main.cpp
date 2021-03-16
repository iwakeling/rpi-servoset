// Utility for setting up and adjusting MERG Servo4 boards
//
// Copyright Ian Wakeling 2021
// License MIT

#include <fontconfig/fontconfig.h>
#include <opt-parse/opt-parse.h>
#include <gpiosysfs/buttons.h>
#include "sdl2-cpp/sdl2.h"
#include "sdl2-cpp/ttf.h"

#include <fstream>
#include <stdexcept>

#include "leverframe.h"
#include "servocontroller.h"

std::string GetFontFile(std::string const& fontName)
{
  std::string fontFile;
  auto config = FcInitLoadConfigAndFonts();
  auto pat = FcNameParse(reinterpret_cast<FcChar8 const*>(fontName.c_str()));
  FcConfigSubstitute(config, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);

  auto result = FcResultNoMatch;
  auto font = FcFontMatch(config, pat, &result);
  if( result == FcResultMatch && font != nullptr )
  {
    FcChar8* fileName = NULL;
    if( FcPatternGetString(font, FC_FILE, 0, &fileName) == FcResultMatch )
    {
      fontFile = reinterpret_cast<char const*>(fileName);
    }
    FcPatternDestroy(font);
  }
  FcPatternDestroy(pat);
  return fontFile;
}

int main(int argc, char** argv)
{
  std::string frameFileName;
  std::string buttonFileName;
  std::string serialPort;
  bool fullScreen = false;

  if( !Opt::parseCmdLine(argc, argv, {
        Opt(
          "--frameFile=(.+)",
          " lever frame file to read",
          [&frameFileName](std::cmatch const& m)
          {
            frameFileName = m[1];
          },
          true),
        Opt(
          "--buttonFile=(.+)",
          "Name of file containing input button parameters",
          [&buttonFileName](std::cmatch const& m)
          {
            buttonFileName = m[1];
          }),
        Opt(
          "--serialPort=(.+)",
          "Name of serial port to write to",
          [&serialPort](std::cmatch const& m)
          {
            serialPort = m[1];
          }),
        Opt(
          "--fullScreen",
          "Use full screen window",
          [&fullScreen](std::cmatch const& m)
          {
            fullScreen = true;
          })}) )
  {
    return 1;
  }

  try
  {
    auto sdlLib = sdl::init();
    auto ttfLib = sdl::ttf::init();

    auto buttonPressEventType = SDL_RegisterEvents(1);
    if( buttonPressEventType == static_cast<Uint32>(-1) )
    {
      sdl::throw_error("Failed to register event types with SDL: ");
    }
    gpiosysfs::Buttons buttons(
      [&buttonPressEventType](std::string const& function)
      {
        static std::map<std::string, SDL_Keycode> const buttonMap{
          {"blue", SDLK_LEFT},
          {"green", SDLK_RIGHT},
          {"white", SDLK_UP},
          {"yellow", SDLK_DOWN},
          {"red", SDLK_q}
        };
        auto a = buttonMap.find(function);
        if( a != buttonMap.end() )
        {
          SDL_Event event;
          event.type = buttonPressEventType;
          event.key.keysym.sym = static_cast<Sint32>(a->second);
          SDL_PushEvent(&event);
        }
      });
    if( !buttonFileName.empty() )
    {
      std::ifstream buttonFile(buttonFileName);
      if( buttonFile.is_open() )
      {
        buttons.loadConfig(buttonFile);
        buttonFile.close();
      }
    }

    int windowFlags = SDL_WINDOW_SHOWN;
    if( fullScreen )
    {
      windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    auto window = sdl::create_window(
      "RPI ServoSet",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      640,
      480,
      windowFlags);
    if( !window )
    {
      sdl::throw_error("Failed to create SDL window: ");
    }
    SDL_RaiseWindow(window.get());
    SDL_Rect displayBounds{0, 0, 0, 0};
    SDL_GetWindowSize(window.get(), &displayBounds.w, &displayBounds.h);

    auto renderer = sdl::create_renderer(window, -1, SDL_RENDERER_ACCELERATED);
    if( !renderer )
    {
      sdl::throw_error("Failed to create SDL renderer: ");
    }

    std::string fontFileName = GetFontFile("DejaVuSans");

    ServoController servoController(serialPort);
    LeverFrame leverFrame(
      frameFileName,
      fontFileName,
      displayBounds,
      servoController);

    bool quit = false;
    std::map<SDL_Keycode, std::function<void()>> keys{
      {SDLK_LEFT, [&leverFrame](){leverFrame.handleLeft();}},
      {SDLK_RIGHT, [&leverFrame](){leverFrame.handleRight();}},
      {SDLK_UP, [&leverFrame](){leverFrame.handleUp();}},
      {SDLK_DOWN, [&leverFrame](){leverFrame.handleDown();}},
      {SDLK_q, [&quit](){quit = true;}}
    };

    SDL_Event e;
    while( !quit && SDL_WaitEvent( &e ) != 0 )
    {
      SDL_SetRenderDrawColor(renderer.get(), 0x00, 0x00, 0x00, 0xFF);
      SDL_RenderClear(renderer.get());

      if( sdl::is_debounced_key(e) ||
          e.type == buttonPressEventType)
      {
        auto k = keys.find(e.key.keysym.sym);
        if( k != keys.end() )
        {
          k->second();
        }
      }
      else if( e.type == SDL_QUIT )
      {
        std::cout << "got SDL_QUIT" << std::endl;
        quit = true;
      }

      leverFrame.render(renderer);
      SDL_RenderPresent(renderer.get());
    }
  }
  catch(std::exception const& e)
  {
    std::cerr << "An exception occurred: " << e.what() << std::endl;
    return 2;
  }

  return 0;
}
