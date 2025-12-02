#pragma once
#include <vulkan/vulkan.h>
typedef struct
{
	// Some gpu have queue that support graphic but not present, and vice versa.
	// Though it's rare. Try to select device that support both
	uint32_t _graphics;
	uint32_t _present;
} QueueFamilyIndices;

typedef struct
{
	uint32_t _swapchain_images_count;
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
    VkCommandPool _commandpool;
    // Use single one for first time experimenting
    VkCommandBuffer _commandbuffer;
	QueueFamilyIndices _queue_indicies;
} VulkanState;