#include "app.h"
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_log.h"
#include "vk.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <stdlib.h>

static constexpr bool ENABLE_VALIDATION_LAYERS =
#ifndef NDEBUG
	true;
#else
	false;
#endif

static void show_available_instance_extensions()
{
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    VkExtensionProperties extensions[count];
    vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions);
    
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "# Available instance extensions\n");
    for (int i = 0; i < count; i++)
    {
        SDL_Log("%s\n", extensions[i].extensionName);
    };
    
};

static VkBool32 VKAPI_PTR debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData)
{
	SDL_Log("Validation layer: %s\n", pCallbackData->pMessage);

	return VK_FALSE;
};

static VkResult create_debug_messenger_util(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT *create_info,
		const VkAllocationCallbacks *allocator,
		VkDebugUtilsMessengerEXT *debug_messenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
			"vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, create_info, allocator, debug_messenger);
	}
	else return VK_ERROR_EXTENSION_NOT_PRESENT;
};

static void destroy_debug_messenter_util(VkInstance instance,
		VkDebugUtilsMessengerEXT debug_messenger,
		const VkAllocationCallbacks *allocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance,
		"vkDestroyDebugUtilsMessengerEXT");
	
	if (func != nullptr)
	{
		func(instance, debug_messenger, allocator);
	};
};

static Result pick_physical_device(VulkanState *vk)
{
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(vk->_instance, &count, nullptr);

	if (count == 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to find GPU support Vulkan\n");
		return FAILURE;
	};

	VkPhysicalDevice physical_devices[count];
	vkEnumeratePhysicalDevices(vk->_instance, &count, physical_devices);
	
	vk->_physical_device = VK_NULL_HANDLE;

	// Choose whatever possible, but prioritise discrete_gpu
	for (int i = 0; i < count; i++)
	{
		if (vk->_physical_device == VK_NULL_HANDLE) vk->_physical_device = physical_devices[i];
		else
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physical_devices[i], &properties);
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				vk->_physical_device = physical_devices[i];
		};
	};
	
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(vk->_physical_device, &properties);
	SDL_Log("Choosen Physical Device: %s\n", properties.deviceName);

	return SUCCESS;
};

static bool find_graphic_queue_family(VkPhysicalDevice physical_device, uint32_t* index)
{
	uint32_t count;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);

	VkQueueFamilyProperties properties[count];
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, properties);

	for (int i = 0; i < count; i++)
	{
		if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			*index = i;
			SDL_Log("Graphics Queue family: %d\n", i);
			return true;
		};
	};
	return false;
};

static bool find_present_queue_family(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t* index)
{
	uint32_t count;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);

	VkQueueFamilyProperties properties[count];
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, properties);

	for (int i = 0; i < count; i++)
	{
		VkBool32 present_support;
		vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
		if (present_support == VK_TRUE)
		{
			*index = i;
			SDL_Log("Present Queue family: %d\n", i);
			return true;
		};
	};
	return false;
};
static bool check_device_extension_support(VkPhysicalDevice device, const char **required_extensions, uint32_t required_count)
{
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

	VkExtensionProperties ext_properties[extension_count];
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, ext_properties);

	uint32_t found = 0;
	for (int i = 0; i < required_count; i++)
	{
		for (int j = 0; j < extension_count; j++)
		{
			if (strcmp(required_extensions[i], ext_properties[j].extensionName))
			{
				found++;
				break;
			};
		};
	};
	if (found != required_count)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Missing required device extensions\n");
		return false;
	};
	return true;
};
static Result create_logical_device(VulkanState *vk)
{
	uint32_t graphic_queue_family;
	uint32_t present_queue_family;
	if (!find_graphic_queue_family(vk->_physical_device, &graphic_queue_family))
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "No queue family with graphic support\n");
		return FAILURE;
	};
	if (!find_present_queue_family(vk->_physical_device, vk->_surface, &present_queue_family))
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "No present family with graphic support\n");
		return FAILURE;
	};

	float priority = 1.0f;
	VkDeviceQueueCreateInfo queue_create_infos[2];
	uint32_t queue_count = 0;
	queue_create_infos[queue_count++] = (VkDeviceQueueCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pQueuePriorities = &priority,
		.queueCount = 1,
		.queueFamilyIndex = graphic_queue_family,
		.pNext = nullptr,
		.flags = 0
	};
	
	if (graphic_queue_family != present_queue_family)
	{
		queue_create_infos[queue_count++] = (VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pQueuePriorities = &priority,
			.queueCount = 1,
			.queueFamilyIndex = graphic_queue_family,
			.pNext = nullptr,
			.flags = 0
		};
	};

	vk->_queue_indicies._graphics = graphic_queue_family;
	vk->_queue_indicies._present = present_queue_family;

	VkDeviceCreateInfo device_create_info = {};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.queueCreateInfoCount = queue_count;
	device_create_info.pQueueCreateInfos = queue_create_infos;

	
	
	const char * required_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	uint32_t required_extensions_count = sizeof(required_extensions) / sizeof(required_extensions[0]);
	
	check_device_extension_support(vk->_physical_device, required_extensions, required_extensions_count);
	device_create_info.ppEnabledExtensionNames = required_extensions;
	device_create_info.enabledExtensionCount = required_extensions_count;

	// Use dynamic rendering instead of renderpass
	// https://docs.vulkan.org/samples/latest/samples/extensions/dynamic_rendering/README.html
	VkPhysicalDeviceVulkan13Features v13_features = {};
	v13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	v13_features.dynamicRendering = VK_TRUE;
	// https://docs.vulkan.org/guide/latest/extensions/VK_KHR_synchronization2.html
	v13_features.synchronization2 = VK_TRUE;

	// Use DrawParameters feature of spirv 1.5
	
	VkPhysicalDeviceVulkan11Features v11_features = {};
	v11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	v11_features.shaderDrawParameters = VK_TRUE;
	v11_features.pNext = &v13_features;

	// VkPhysicalDeviceFeatures2 provide a pNext chain to enable features on the device.
	// The features member of this structs is 1.0 features. No features needed for now
	VkPhysicalDeviceFeatures2 device_features = {};
	device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	device_features.pNext = &v11_features;
	
	// pEnabledFeatures is legacy. Use pNext chain to enable features
	//device_create_info.pEnabledFeatures = &device_features;

	device_create_info.pNext = &device_features;

	if (vkCreateDevice(vk->_physical_device, &device_create_info, nullptr, &vk->_device) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create vulkan logical device\n");
		return FAILURE;
	};

	vkGetDeviceQueue(vk->_device, graphic_queue_family, 0, &vk->_graphics_queue);
	vkGetDeviceQueue(vk->_device, present_queue_family, 0, &vk->_present_queue);

	return SUCCESS;
};

// Swapchain creation

#define CLAMP(x,lo,hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
static VkExtent2D get_swap_extent(const VkSurfaceCapabilitiesKHR *capabilities, SDL_Window *window)
{
	if (capabilities->currentExtent.width != UINT32_MAX)
	{
		return capabilities->currentExtent;
	};

	int winw, winh;
	SDL_GetWindowSizeInPixels(window, &winw, &winh);

	VkExtent2D extent = {(uint32_t)winw, (uint32_t)winh};

	extent.width = CLAMP(extent.width, capabilities->minImageExtent.width, capabilities->maxImageExtent.width);
	extent.height = CLAMP(extent.height, capabilities->minImageExtent.height, capabilities->maxImageExtent.height);

	return extent;
};

// Get the format that support normal rgba and srgb color space, or fallback to whatever available
VkSurfaceFormatKHR get_swap_surface_format(VulkanState *vk)
{
	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk->_physical_device, vk->_surface, &format_count, nullptr);

	VkSurfaceFormatKHR formats[format_count];
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk->_physical_device, vk->_surface, &format_count, formats);

	for (int i = 0; i < format_count; i++)
	{
		if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB
				&& formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return formats[i];
	};

	return formats[0];
};

static const char* get_present_mode_string(VkPresentModeKHR presentMode) {
    switch (presentMode) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
            return "VK_PRESENT_MODE_IMMEDIATE_KHR";
        case VK_PRESENT_MODE_MAILBOX_KHR:
            return "VK_PRESENT_MODE_MAILBOX_KHR";
        case VK_PRESENT_MODE_FIFO_KHR:
            return "VK_PRESENT_MODE_FIFO_KHR";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
            return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
            return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";
        default:
            return "UNKNOWN_PRESENT_MODE";
    }
}
bool is_swap_present_mode_supported(VulkanState *vk, VkPresentModeKHR desired_mode)
{
	uint32_t pmode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(vk->_physical_device, vk->_surface, &pmode_count, nullptr);

	VkPresentModeKHR pmodes[pmode_count];
	vkGetPhysicalDeviceSurfacePresentModesKHR(vk->_physical_device, vk->_surface, &pmode_count, pmodes);

	SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Present mode support:\n");
	for (int i = 0; i < pmode_count; i++)
	{
		SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "\t%s\n", get_present_mode_string(pmodes[i]));
		if (pmodes[i] == desired_mode) return true;
	};
	return false;
};

static Result create_swapchain(VulkanState *vk, SDL_Window *window)
{
	VkSurfaceFormatKHR surface_format = get_swap_surface_format(vk);

	VkSurfaceCapabilitiesKHR surface_capabilities = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->_physical_device, vk->_surface, &surface_capabilities);
	VkExtent2D extent = get_swap_extent(&surface_capabilities, window);
	
	// Will experiment with different mode later
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

	uint32_t image_count = surface_capabilities.minImageCount + 1;
	if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount)
		image_count = surface_capabilities.maxImageCount;
	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = vk->_surface;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	create_info.presentMode = present_mode;
	create_info.imageExtent = extent;
	create_info.imageFormat = surface_format.format;
	create_info.minImageCount = image_count;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageArrayLayers = 1;
	create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	if (vk->_queue_indicies._graphics != vk->_queue_indicies._present)
	{
		uint32_t queue_indices[2] = {vk->_queue_indicies._graphics, vk->_queue_indicies._present};
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_indices;
	};

	create_info.preTransform = surface_capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.clipped = VK_TRUE;
	//Providing a valid oldSwapchain may aid in the resource reuse, and also allows the
	//application to still present any images that are already acquired from it.
	//Set to Null_handle on the first creation.
	create_info.oldSwapchain = VK_NULL_HANDLE;


	if(vkCreateSwapchainKHR(vk->_device, &create_info, nullptr, &vk->_swapchain) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create swapchain\n");
		return FAILURE;
	};

	SDL_LogInfo(SDL_LOG_CATEGORY_GPU,"Created Swapchain\n");
	SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Use %d images in swapchain\n", image_count);

	vkGetSwapchainImagesKHR(vk->_device, vk->_swapchain, &vk->_swapchain_images_count, nullptr);


	vk->_swapchain_extent = extent;
	vk->_swapchain_format = surface_format.format;
	return SUCCESS;
};

static Result create_image_view(VulkanState *vk)
{

	for (int i = 0; i < vk->_swapchain_images_count; i++)
	{
		VkImageViewCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = vk->_swapchain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = vk->_swapchain_format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.layerCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;

		if (vkCreateImageView(vk->_device, &create_info, nullptr, &vk->_swapchain_imageviews[i]) != VK_SUCCESS)
		{
			SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create image view\n");
			return FAILURE;
		};
	};

	SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Created %d image views\n", vk->_swapchain_images_count);
	return SUCCESS;
};
static Result create_vulkan_instance(VulkanState *vk)
{
	uint32_t instanceVersion = VK_API_VERSION_1_0;
	vkEnumerateInstanceVersion(&instanceVersion);
	SDL_LogInfo(SDL_LOG_CATEGORY_GPU,"Instance supported version: %u.%u.%u\n",
		   VK_VERSION_MAJOR(instanceVersion),
		   VK_VERSION_MINOR(instanceVersion),
		   VK_VERSION_PATCH(instanceVersion));
    show_available_instance_extensions();
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Home Invasion";
    app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
    app_info.apiVersion = VK_API_VERSION_1_3;
    
    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    
	// SDL provide the needed extensions for creating the instance (like VK_KHR_win32/wayland..._surface)
    uint32_t sdl_extensions_count = 0;
    const char * const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count);

	uint32_t total_exts_count = sdl_extensions_count + (ENABLE_VALIDATION_LAYERS ? 1 : 0);
    const char **extensions = (const char **)malloc(total_exts_count * sizeof(char*));
	for (int i = 0; i < sdl_extensions_count; i++)
	{
		extensions[i] = sdl_extensions[i];
	};
	
	if (ENABLE_VALIDATION_LAYERS) extensions[total_exts_count - 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "# Instance Extensions count: %d\n", total_exts_count);
    for (int i = 0; i < total_exts_count; i++)
    {
        SDL_Log("%s\n", extensions[i]);
    };
    instance_create_info.enabledExtensionCount = total_exts_count;
    instance_create_info.ppEnabledExtensionNames = extensions;
	if (ENABLE_VALIDATION_LAYERS)
	{

		const char *validation_layers[1] = {"VK_LAYER_KHRONOS_validation"};
		uint32_t layer_count;
		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
		VkLayerProperties available_layers[layer_count];
		vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

		bool layer_found = false;
		for (int i = 0; i < layer_count; i++)
		{
			if (strcmp(validation_layers[0], available_layers[i].layerName))
				layer_found = true;
		};

		if (!layer_found)
		{
			SDL_LogError(SDL_LOG_CATEGORY_GPU, "Validation layer requested but not found\n");
			return FAILURE;
		};

		SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Enabled validation layers\n");

	 instance_create_info.enabledLayerCount = 1;
	 instance_create_info.ppEnabledLayerNames = validation_layers;
	};

	// DEBUG_MESSENGER
#ifndef NDEBUG
	VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
	debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; 
	debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_messenger_create_info.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

	debug_messenger_create_info.pfnUserCallback = debug_callback;

	instance_create_info.pNext = &debug_messenger_create_info;

#endif


	// CREATING THE INSTANCE

    VkResult result = vkCreateInstance(&instance_create_info, nullptr, &vk->_instance);
    
    if (result != VK_SUCCESS)
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create vulkan instance\n");
		return FAILURE;
    };


    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "# Created Vulkan Instance, version\n");
	

#ifndef NDEBUG
	if (create_debug_messenger_util(vk->_instance, &debug_messenger_create_info, nullptr, &vk->_debug_messenger) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "VULKAN: Failed to create debug messenger util\n");
		return FAILURE;
	};
#endif

	return SUCCESS;
};
static Result init_sdl(AppState *app)
{
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_Log("Couldn't init SDL\n");
        return FAILURE;
	};
	
	app->_window = SDL_CreateWindow("Test", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	if (app->_window == nullptr)
	{
		SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Failed to create SDL_Window\n");
		return FAILURE;

	};
    
    return SUCCESS;
};

static Result create_vulkan_surface(AppState *app)
{
	if (!SDL_Vulkan_CreateSurface(app->_window, app->_vk._instance, nullptr, &app->_vk._surface))
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create Vulkan Surface\n");
		return FAILURE;
	};

	SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "# Created Vulkan surface\n");

	return SUCCESS;
};

#include <stdio.h>
static char *read_shader_file(const char *path, uint32_t *out_size)
{
	FILE *file = fopen(path, "rb");
	if (!file) {SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "fopen\n");exit(0);};

	size_t size = 0;
	fseek(file, 0, SEEK_END);
	size = ftell(file);

	char *content = malloc(size);
	fseek(file, 0, SEEK_SET);
	fread(content, sizeof(char), size, file);

	fclose(file);
	*out_size = size;
	return content;
};

VkShaderModule create_shader_module(VkDevice device, const char *src, size_t size)
{
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pCode = (const uint32_t *)src;
	create_info.codeSize = size;

	VkShaderModule shader_module;

	if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create shader module\n");
		exit(0);
	};

	return shader_module;
};

// TODO: Make this as a fallback
//If not using Dynamic Rendering, draw commands must be recorded within a render pass instance.
//Each render pass instance defines a set of image resources,
//referred to as attachments, used during rendering.
//static Result create_render_pass(VulkanState *vk)
//{
//};

static Result create_graphics_pipeline(VulkanState *vk, const char *shader_path)
{
	uint32_t shader_size;
	const char* shader_src = read_shader_file(shader_path, &shader_size);
	vk->_shader_module = create_shader_module(vk->_device, shader_src, shader_size);

	VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {0};
	shader_stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader_stage_create_info[0].module = vk->_shader_module;
	shader_stage_create_info[0].pName = "vert_main" ;

	shader_stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader_stage_create_info[1].module = vk->_shader_module;
	shader_stage_create_info[1].pName = "frag_main";

	VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
	vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info = {};

	input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_create_info.primitiveRestartEnable = VK_FALSE;



	VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};
	uint32_t dynamic_states_size = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
	VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
	dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_create_info.pDynamicStates = dynamic_states;
	dynamic_state_create_info.dynamicStateCount = dynamic_states_size;
	
	// With dynamic state, the actual viewport and scissor will be later set at drawing time
	// Without dynamic state, they need to be set here, which makes them immutable - any changes needed require
	// creating a new pipeline
	// Can create multiple viewport and scissor on some GPU, need to enable in GPU features when creating
	// logical device
	VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
	viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_create_info.scissorCount = 1;
	viewport_state_create_info.viewportCount = 1;

	// # Rasterizer
	// The rasterizer takes the geometry shaped by the vertices from the vertex shader
	// and turns it into fragments to be colored by the fragment shader. 
	// It also performs depth testing, face culling and the scissor test, and it can be
	// configured to output fragments that fill entire polygons or just the edges (wireframe rendering)

	VkPipelineRasterizationStateCreateInfo rasterizer_create_info = {};
	rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_create_info.depthClampEnable = VK_FALSE;
	rasterizer_create_info.depthBiasSlopeFactor = 1.0f;
	rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_create_info.lineWidth = 1.0f;
	rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer_create_info.depthBiasEnable = VK_FALSE;

	// # Multisampling
	// Disable for now
	VkPipelineMultisampleStateCreateInfo multisample_create_info = {};
	multisample_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_create_info.sampleShadingEnable = VK_FALSE;
	multisample_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// # Color blending
	
	// config per attched framebuffer
	VkPipelineColorBlendAttachmentState colorblend_attachment = {};
	colorblend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	// if false, new color from fragment shader is passed through unmodified.
	// below is alpha-blending
	// `	finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
	// `	finalColor.a = newAlpha.a
	colorblend_attachment.blendEnable = VK_TRUE;
	colorblend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorblend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorblend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	VkPipelineColorBlendStateCreateInfo colorblend_state_create_info = {};
	colorblend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	// If true, use bitwise combination of color, ignore the colorblend attachment above
	colorblend_state_create_info.logicOpEnable = VK_FALSE;
	colorblend_state_create_info.attachmentCount = 1;
	colorblend_state_create_info.pAttachments = &colorblend_attachment;

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(vk->_device, &pipeline_layout_create_info, nullptr, &vk->_pipeline_layout) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create pipeline layout\n");
		return FAILURE;
	};

	// If use dynamic rendering, pass this to pNext of VkGraphicsPipelineCreateInfo and renderpass set to nullptr. This will specify the viewmask and
	// color attachment info. If use a valid RenderPass, value of this structure is ignored
	VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {};
	pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipeline_rendering_create_info.colorAttachmentCount = 1;
	pipeline_rendering_create_info.pColorAttachmentFormats = &vk->_swapchain_format;

	VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &pipeline_rendering_create_info,
		.stageCount = 2,
		.pStages = shader_stage_create_info,
		.pVertexInputState = &vertex_input_info,
		.pInputAssemblyState = &input_assembly_create_info,
		.pRasterizationState = &rasterizer_create_info,
		.pColorBlendState = &colorblend_state_create_info,
		.pMultisampleState = &multisample_create_info,
		.pViewportState = &viewport_state_create_info,
		.pDynamicState = &dynamic_state_create_info,
		.renderPass = nullptr,
		.layout = vk->_pipeline_layout,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1,
	};


	if (vkCreateGraphicsPipelines(vk->_device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &vk->_graphics_pipeline) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create graphics pipeline\n");
		return FAILURE;
	};

	SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Created Graphics Pipeline\n");
	return SUCCESS;
};

static Result create_command_pool(VulkanState *vk)
{
	VkCommandPoolCreateInfo cmdpool_create_info = {};
	cmdpool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;	
	cmdpool_create_info.queueFamilyIndex = vk->_queue_indicies._graphics;
	// This allow command buffers to be reset individually via vkResetCommandBuffer
	cmdpool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	
	if (vkCreateCommandPool(vk->_device, &cmdpool_create_info, nullptr, &vk->_commandpool) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create command pool\n");
		return FAILURE;
	};
	
	SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Created Command pool\n");
	return SUCCESS;
};

static Result create_command_buffer(VulkanState *vk)
{
	VkCommandBufferAllocateInfo cmdbuffer_create_info = {};
	cmdbuffer_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdbuffer_create_info.commandBufferCount = 1;
	cmdbuffer_create_info.commandPool = vk->_commandpool;
	cmdbuffer_create_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	
	if (vkAllocateCommandBuffers(vk->_device, &cmdbuffer_create_info, &vk->_commandbuffer) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to allocate command buffer\n");
		return FAILURE;
	};
	SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Allocated command buffer\n");
	
	return SUCCESS;
};

static void transition_image_layout(VulkanState *vk,
		uint32_t image_idx,
		VkImageLayout old_layout,
		VkImageLayout new_layout,
		VkAccessFlags2 src_access_mask,
		VkAccessFlags2 dst_access_mask,
		VkPipelineStageFlags2 src_stage_mask,
		VkPipelineStageFlags2 dst_stage_mask)
{
	VkImageMemoryBarrier2 img_memory_barrier = {};
	img_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	img_memory_barrier.srcStageMask = src_stage_mask;
	img_memory_barrier.dstStageMask = dst_stage_mask;
	img_memory_barrier.srcAccessMask = src_access_mask;
	img_memory_barrier.dstAccessMask = dst_access_mask;
	img_memory_barrier.oldLayout = old_layout;
	img_memory_barrier.newLayout = new_layout;
	img_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	img_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	img_memory_barrier.image = vk->_swapchain_images[image_idx];
	img_memory_barrier.subresourceRange = (VkImageSubresourceRange){
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.layerCount = 1,
		.baseArrayLayer = 0
	};

	VkDependencyInfo dependency_info = {};
	dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency_info.imageMemoryBarrierCount = 1;
	dependency_info.pImageMemoryBarriers = &img_memory_barrier;

	vkCmdPipelineBarrier2(vk->_commandbuffer, &dependency_info);
};
static void record_command_buffer(VulkanState *vk, uint32_t image_idx)
{
	VkCommandBufferBeginInfo cmdbuffer_begin_info = {};
	cmdbuffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(vk->_commandbuffer, &cmdbuffer_begin_info);

	//Before rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
	
	transition_image_layout(vk, image_idx,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

	VkClearValue clear_color = {};
	clear_color.color = (VkClearColorValue){0.0f, 0.0f, 0.0f, 1.0f};
	VkRenderingAttachmentInfo rendering_attachment_info = {};
	rendering_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	rendering_attachment_info.imageView = vk->_swapchain_imageviews[image_idx];
	rendering_attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	rendering_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	rendering_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	rendering_attachment_info.clearValue = clear_color;

	VkRenderingInfo rendering_info = {};
	rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering_info.renderArea = (VkRect2D){.offset = {0, 0}, .extent = vk->_swapchain_extent};
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &rendering_attachment_info;

	vkCmdBeginRendering(vk->_commandbuffer, &rendering_info);

	vkCmdBindPipeline(vk->_commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->_graphics_pipeline);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)vk->_swapchain_extent.width;
	viewport.height = (float)vk->_swapchain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = (VkOffset2D){0, 0};
	scissor.extent = vk->_swapchain_extent;
	vkCmdSetViewport(vk->_commandbuffer, 0, 1, &viewport);
	vkCmdSetScissor(vk->_commandbuffer, 0, 1, &scissor);

	vkCmdDraw(vk->_commandbuffer, 3, 1, 0, 0);


	vkCmdEndRendering(vk->_commandbuffer);
	//After drawing, transition the image back to PRESENT_SRC
	
	transition_image_layout(vk, image_idx,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

	vkEndCommandBuffer(vk->_commandbuffer);

};

static Result create_sync_objects(VulkanState *vk)
{
	VkSemaphoreCreateInfo smp_create_info = {};
	smp_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(vk->_device, &smp_create_info,
		nullptr, &vk->_smp_present_complete) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create present semaphore\n");
		return FAILURE;
	};
	if (vkCreateSemaphore(vk->_device, &smp_create_info,
		nullptr, &vk->_smp_render_complete) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create render semaphore\n");
		return FAILURE;
	};
	
	VkFenceCreateInfo fence_create_info = {};
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (vkCreateFence(vk->_device, &fence_create_info, nullptr, &vk->_fence_draw) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create fence for drawing\n");
		return FAILURE;
	};
	
	SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Created sync objects\n");
	return SUCCESS;
};

static void cleanup_swapchain(VkDevice device, VkSwapchainKHR swapchain, uint32_t imageview_count, VkImageView* imageviews)
{
	for (uint32_t i = 0; i < imageview_count; i++)
	{
		vkDestroyImageView(device, imageviews[i], nullptr);
	};
	
	vkDestroySwapchainKHR(device, swapchain,nullptr);
};

static void recreate_swapchain(VulkanState* vk, SDL_Window* window)
{
	vkDeviceWaitIdle(vk->_device);
	
	cleanup_swapchain(vk->_device, vk->_swapchain, vk->_swapchain_images_count, vk->_swapchain_imageviews);
	create_swapchain(vk, window);
	vkGetSwapchainImagesKHR(vk->_device, vk->_swapchain, &vk->_swapchain_images_count, vk->_swapchain_images);
	create_image_view(vk);
};
Result app_init(AppState *app)
{
	if (init_sdl(app) != SUCCESS) return FAILURE;
	if (create_vulkan_instance(&app->_vk) != SUCCESS) return FAILURE;
	if (create_vulkan_surface(app) != SUCCESS) return FAILURE;
	if (pick_physical_device(&app->_vk) != SUCCESS) return FAILURE;
	if (create_logical_device(&app->_vk) != SUCCESS) return FAILURE;


	if (create_swapchain(&app->_vk, app->_window) != SUCCESS) return FAILURE;
	app->_vk._swapchain_images = calloc(app->_vk._swapchain_images_count, sizeof(VkImage));
	vkGetSwapchainImagesKHR(app->_vk._device, app->_vk._swapchain, &app->_vk._swapchain_images_count, app->_vk._swapchain_images);
	app->_vk._swapchain_imageviews = calloc(app->_vk._swapchain_images_count, sizeof(VkImageView));
	if (create_image_view(&app->_vk) != SUCCESS) return FAILURE;
	if (create_graphics_pipeline(&app->_vk, "shader/slang_compiled.spv") != SUCCESS) return FAILURE;
	if (create_command_pool(&app->_vk) != SUCCESS) return FAILURE;
	if (create_command_buffer(&app->_vk) != SUCCESS) return FAILURE;
	if (create_sync_objects(&app->_vk) != SUCCESS) return FAILURE;

	return SUCCESS;
};

static void draw(AppState *app)
{
	VulkanState *vk = &app->_vk;

	vkQueueWaitIdle(vk->_graphics_queue);
	
	VkAcquireNextImageInfoKHR acquire_next_image_info = {};
	acquire_next_image_info.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
	acquire_next_image_info.semaphore = vk->_smp_present_complete;
	acquire_next_image_info.timeout = UINT64_MAX;
	acquire_next_image_info.swapchain = vk->_swapchain;
	// If use multiple gpus, need to mask which one we are using
	acquire_next_image_info.deviceMask = 1;

	uint32_t img_idx = 0;
	VkResult acquire_image_result = vkAcquireNextImage2KHR(vk->_device, &acquire_next_image_info, &img_idx);
	if (acquire_image_result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreate_swapchain(&app->_vk, app->_window);
		return;
	}
	else if (acquire_image_result != VK_SUCCESS && acquire_image_result != VK_SUBOPTIMAL_KHR)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to acquire next image\n");
		exit(0);
	};

	// Put fence in unsignal state to pass to queue_summit, then we can use
	// vkWaitForFences() to know when gpu is done
	vkResetFences(vk->_device, 1, &vk->_fence_draw);
	
	record_command_buffer(vk, img_idx);
	VkPipelineStageFlagBits2 pipeline_stage_flag = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	
	// Wait semamphore submit info
	VkSemaphoreSubmitInfo wait_smp_submit_info = {};
	wait_smp_submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	wait_smp_submit_info.semaphore = vk->_smp_present_complete;
	// For binary semaphore, this is ignore. Otherwise (timeline semaphore),
	// value is either the value used to signal semaphore
	// or the value waited on by semaphore
	wait_smp_submit_info.value = 0;
	wait_smp_submit_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	// Wait semamphore submit info
	VkSemaphoreSubmitInfo signal_smp_submit_info = {};
	signal_smp_submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signal_smp_submit_info.semaphore = vk->_smp_render_complete;
	signal_smp_submit_info.value = 0;
	signal_smp_submit_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	
	//Command buffer submit info

	VkCommandBufferSubmitInfo cmd_submit_info = {};
	cmd_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd_submit_info.commandBuffer = vk->_commandbuffer;

	VkSubmitInfo2 submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.waitSemaphoreInfoCount = 1;
	submit_info.pWaitSemaphoreInfos = &wait_smp_submit_info;
	submit_info.signalSemaphoreInfoCount = 1;
	submit_info.pSignalSemaphoreInfos = &signal_smp_submit_info;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &cmd_submit_info;
	
	// Submit the queue.
	vkQueueSubmit2(vk->_graphics_queue, 1, &submit_info, vk->_fence_draw);
	
	// Wait for the gpu to done work
	vkWaitForFences(vk->_device, 1, &vk->_fence_draw, VK_TRUE, UINT64_MAX);
	
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk->_smp_render_complete;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk->_swapchain;
	present_info.pImageIndices = &img_idx;
	
	VkResult present_result = vkQueuePresentKHR(vk->_graphics_queue, &present_info);
	if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
	{
		recreate_swapchain(&app->_vk, app->_window);
	};
};
Result app_mainloop(AppState *app)
{
	draw(app);
    return SUCCESS;
};
void app_quit(AppState *app)
{
	vkDeviceWaitIdle(app->_vk._device);
#ifndef NDEBUG
	destroy_debug_messenter_util(app->_vk._instance, app->_vk._debug_messenger, nullptr);
#endif
	cleanup_swapchain(app->_vk._device, app->_vk._swapchain, app->_vk._swapchain_images_count, app->_vk._swapchain_imageviews);

	free(app->_vk._swapchain_imageviews);
	free(app->_vk._swapchain_images);
	vkDestroySemaphore(app->_vk._device, app->_vk._smp_present_complete, nullptr);
	vkDestroySemaphore(app->_vk._device, app->_vk._smp_render_complete, nullptr);
	vkDestroyFence(app->_vk._device, app->_vk._fence_draw, nullptr);
	vkFreeCommandBuffers(app->_vk._device, app->_vk._commandpool, 1, &app->_vk._commandbuffer);
	vkDestroyCommandPool(app->_vk._device, app->_vk._commandpool, nullptr);
	vkDestroyPipelineLayout(app->_vk._device, app->_vk._pipeline_layout, nullptr);
	vkDestroyShaderModule(app->_vk._device, app->_vk._shader_module, nullptr);
	vkDestroyPipeline(app->_vk._device, app->_vk._graphics_pipeline, nullptr);
	vkDestroyDevice(app->_vk._device, nullptr);
	vkDestroySurfaceKHR(app->_vk._instance, app->_vk._surface, nullptr);
    vkDestroyInstance(app->_vk._instance, nullptr);
    SDL_DestroyWindow(app->_window);
    free(app);
};
