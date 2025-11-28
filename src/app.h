#pragma once
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
typedef struct
{
    SDL_Window* _window;
    VkInstance _vkinstance;
} AppState;
void app_init(AppState *app);
int app_mainloop(AppState *app);
void app_quit(AppState *app);
