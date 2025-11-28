#include "app.h"
#include "SDL3/SDL_Vulkan.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <stdlib.h>

static void show_available_extensions()
{
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    VkExtensionProperties extensions[count];
    vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions);
    
    fprintf(stderr, "Available extensions\n");
    for (int i = 0; i < count; i++)
    {
        fprintf(stderr, "%s\t", extensions[i].extensionName);
    };
    
    fprintf(stderr, "\n");
    
};
static void init_vulkan(VkInstance *instance)
{
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Home Invasion";
    app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
    app_info.apiVersion = VK_API_VERSION_1_4;
    
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    
    uint32_t count = 0;
    const char * const *extensions = nullptr;
    
    extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    
    fprintf(stderr, "Instance Extensions count: %d\n", count);
    for (int i = 0; i < count; i++)
    {
        fprintf(stderr, "%s\n", extensions[i]);
    };
    create_info.enabledExtensionCount = count;
    create_info.ppEnabledExtensionNames = extensions;
    
    show_available_extensions();
    VkResult result = vkCreateInstance(&create_info, nullptr, instance);
    
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "Failed to create vulkan instance\n");
        exit(0);
    };
    
    fprintf(stderr, "Created Vulkan Instance\n");
};
static int init_sdl(AppState *app)
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_Log("Couldn't init SDL\n");
        return 0;
	};
	
	app->_window = SDL_CreateWindow("home invasion", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    
    return 1;
};

void app_init(AppState *app)
{
    init_sdl(app);
    init_vulkan(&app->_vkinstance);
};
int app_mainloop(AppState *app)
{
    return 1;
};
void app_quit(AppState *app)
{
    SDL_DestroyWindow(app->_window);
    vkDestroyInstance(app->_vkinstance, nullptr);
    free(app);
};
