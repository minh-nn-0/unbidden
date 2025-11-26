# SDL

include(FetchContent)

FetchContent_Declare(
	SDL3
	GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
	GIT_TAG badbf8da4ee72b3ef599c721ffc9899e8d7c8d90 # Version 3.2.26
)
FetchContent_Declare(
	SDL_image
	GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
	GIT_TAG 11154afb7855293159588b245b446a4ef09e574f # Version 3.2.4
)
FetchContent_Declare(
	SDL_ttf
	GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
	GIT_TAG a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b # Version 3.2.2
)
FetchContent_Declare(
	SDL_mixer
	GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
	GIT_TAG 171eb2d420d5643e4ee11514a06e04a41a463bbd # Version 2.8.1
)

set(SDL2MIXER_VORBIS OFF)
set(SDL2MIXER_OPUS OFF)
set(SDL2MIXER_WAVPACK OFF)
set(SDL2MIXER_MOD OFF)
set(SDL2MIXER_MIDI_FLUIDSYNTH OFF)
set(SDL2TTF_VENDORED ON)

FetchContent_MakeAvailable(SDL3 SDL_image SDL_ttf SDL_mixer)
