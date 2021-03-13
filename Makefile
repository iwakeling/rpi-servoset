include makelib/cpp-rules.mk

SOURCES := main.cpp leverframe.cpp servocontroller.cpp
LOCAL_LIB_FLAGS := -I ..
LOCAL_LIBS := -L../gpiosysfs/$(FLAVOUR) -lgpiosysfs
SDL_FLAGS := `pkg-config --cflags SDL2_ttf`
SDL_LIBS := `pkg-config --libs SDL2_ttf`
SDLGFX_FLAGS := `pkg-config --cflags SDL2_gfx`
SDLGFX_LIBS := `pkg-config --libs SDL2_gfx`
FONTCONFIG_FLAGS := `pkg-config --cflags fontconfig`
FONTCONFIG_LIBS := `pkg-config --libs fontconfig`

CPPFLAGS := $(LOCAL_LIB_FLAGS) $(SDL_FLAGS) $(SDLGFX_FLAGS) $(FONTCONFIG_FLAGS)
CXXFLAGS += --std=c++17
LIBS := $(SDL_LIBS) $(SDLGFX_LIBS) $(FONTCONFIG_LIBS) $(LOCAL_LIBS)

$(call build-executable,rpi-servoset,$(SOURCES),$(LIBS))
