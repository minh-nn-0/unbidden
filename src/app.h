#pragma once
#include <SDL3/SDL.h>
#include "vk.h"
typedef struct
{
    SDL_Window* _window;
    VulkanState _vk;
} AppState;

typedef enum
{
	SUCCESS,
	FAILURE
} Result;
Result app_init(AppState *app);
Result app_mainloop(AppState *app);
void app_quit(AppState *app);
