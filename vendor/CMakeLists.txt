# SDL

include(FetchContent)

FetchContent_Declare(
	SDL3
	GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
	GIT_TAG main
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
FetchContent_Declare(
	SDL_image
	GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
	GIT_TAG 11154afb7855293159588b245b446a4ef09e574f # Version 3.2.4
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
FetchContent_Declare(
	SDL_ttf
	GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
	GIT_TAG a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b # Version 3.2.2
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)
FetchContent_Declare(
	SDL_mixer
	GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
	GIT_TAG main
	GIT_SHALLOW TRUE
	GIT_PROGRESS TRUE
)

set(SDLMIXER_VORBIS OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_WAVPACK OFF)
set(SDLMIXER_MOD OFF)
set(SDLMIXER_MIDI_FLUIDSYNTH OFF)
set(SDLTTF_VENDORED ON)

FetchContent_MakeAvailable(SDL3 SDL_image SDL_ttf SDL_mixer)
