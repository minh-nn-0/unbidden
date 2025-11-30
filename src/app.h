#pragma once
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
typedef struct
{
	// Some gpu have queue that support graphic but not present, and vice versa.
	// Though it's rare. Try to select device that support both
	uint32_t _graphics;
	uint32_t _present;
} QueueFamilyIndices;

typedef struct
{
	uint32_t _swapchain_images_count, _swapchain_imageviews_count;
	VkImageView *_swapchain_imageviews;
	VkImage *_swapchain_images;
	VkInstance _instance;
#ifndef NDEBUG
	VkDebugUtilsMessengerEXT _debug_messenger;
#endif
	VkSurfaceKHR _surface;
	VkPhysicalDevice _physical_device;
	VkDevice _device;
	VkQueue _graphics_queue, _present_queue;
	VkPipelineLayout _pipeline_layout;
	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_format;
	VkExtent2D _swapchain_extent;
	QueueFamilyIndices _queue_indicies;
} VulkanState;
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
