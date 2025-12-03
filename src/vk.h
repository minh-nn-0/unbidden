#pragma once
#include <vulkan/vulkan.h>
typedef struct
{
	// Some gpu have queue that support graphic but not present, and vice versa.
	// Though it's rare. Try to select device that support both
	uint32_t _graphics;
	uint32_t _present;
} QueueFamilyIndices;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
typedef struct
{
	uint32_t _swapchain_images_count, _current_frame;
	QueueFamilyIndices _queue_indicies;
	VkImageView *_swapchain_imageviews;
	VkImage *_swapchain_images;
	VkInstance _instance;
#ifndef NDEBUG
	VkDebugUtilsMessengerEXT _debug_messenger;
#endif
	VkSurfaceKHR _surface;
	VkShaderModule _shader_module;
	VkPhysicalDevice _physical_device;
	VkDevice _device;
	VkQueue _graphics_queue, _present_queue;
	VkPipelineLayout _pipeline_layout;
	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_format;
	VkExtent2D _swapchain_extent;
	VkPipeline _graphics_pipeline;
    VkCommandPool _commandpool;
    VkCommandBuffer _commandbuffers[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore _smps_present_complete[MAX_FRAMES_IN_FLIGHT],
				_smps_render_complete[MAX_FRAMES_IN_FLIGHT];
	VkFence _fences_draw[MAX_FRAMES_IN_FLIGHT];
} VulkanState;
