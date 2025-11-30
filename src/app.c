#include "app.h"
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

#ifndef NDEBUG
#endif
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

	// No features needed for now
	VkPhysicalDeviceFeatures device_features = {};

	device_create_info.pEnabledFeatures = &device_features;

	const char *required_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	uint32_t required_count = sizeof(required_extensions) / sizeof(required_extensions[0]);

	if (!check_device_extension_support(vk->_physical_device, required_extensions, required_count)) return FAILURE;
	
	device_create_info.enabledExtensionCount = required_count;
	device_create_info.ppEnabledExtensionNames = required_extensions;

	if (vkCreateDevice(vk->_physical_device, &device_create_info, nullptr, &vk->_device) != VK_SUCCESS)
	{
		SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create vulkan logical device\n");
		return FAILURE;
	};

	vkGetDeviceQueue(vk->_device, graphic_queue_family, 0, &vk->_present_queue);

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

	SDL_Log("Created Swapchain\n");

	vkGetSwapchainImagesKHR(vk->_device, vk->_swapchain, &vk->_swapchain_images_count, nullptr);

	vk->_swapchain_images = calloc(vk->_swapchain_images_count, sizeof(VkImage));
	vkGetSwapchainImagesKHR(vk->_device, vk->_swapchain, &vk->_swapchain_images_count, vk->_swapchain_images);

	vk->_swapchain_extent = extent;
	vk->_swapchain_format = surface_format.format;
	return SUCCESS;
};

static Result create_image_view(VulkanState *vk)
{
	vk->_swapchain_imageviews_count = vk->_swapchain_images_count;
	vk->_swapchain_imageviews = calloc(vk->_swapchain_imageviews_count, sizeof(VkImageView));

	for (int i = 0; i < vk->_swapchain_imageviews_count; i++)
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

	return SUCCESS;
};
static Result create_vulkan_instance(VulkanState *vk)
{
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Home Invasion";
    app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
    app_info.apiVersion = VK_API_VERSION_1_4;
    
    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    
    uint32_t count = 0;
    const char * const *sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&count);

	uint32_t total_exts_count = count + (ENABLE_VALIDATION_LAYERS ? 1 : 0);
    const char **extensions = (const char **)malloc(total_exts_count * sizeof(char*));
	for (int i = 0; i < count; i++)
	{
		extensions[i] = sdl_extensions[i];
	};

	if (ENABLE_VALIDATION_LAYERS) extensions[count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    
    fprintf(stderr, "Instance Extensions count: %d\n", count);
    for (int i = 0; i < total_exts_count; i++)
    {
        fprintf(stderr, "%s\n", extensions[i]);
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
			fprintf(stderr, "Validation layer requested but not found\n");
			return FAILURE;
		};

		fprintf(stderr, "Enabled validation layers\n");

	 instance_create_info.enabledLayerCount = 1;
	 instance_create_info.ppEnabledLayerNames = validation_layers;
	};
    
    show_available_extensions();

	// DEBUG_MESSENGER
#ifndef NDEBUG
	VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
	debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; 
	debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

	debug_messenger_create_info.pfnUserCallback = debug_callback;

	instance_create_info.pNext = &debug_messenger_create_info;

#endif


	// CREATING THE INSTANCE

    VkResult result = vkCreateInstance(&instance_create_info, nullptr, &vk->_instance);
    
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "Failed to create vulkan instance\n");
		return FAILURE;
    };

    fprintf(stderr, "Created Vulkan Instance\n");

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
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_Log("Couldn't init SDL\n");
        return FAILURE;
	};
	
	app->_window = SDL_CreateWindow("home invasion", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

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

	SDL_Log("Created Vulkan surface\n");
	VkBool32 presentation_support;

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

static Result create_graphics_pipeline(VulkanState *vk, const char *vert_shader_path, const char *frag_shader_path)
{
	uint32_t vert_src_size, frag_src_size;
	char *vert_shader_src = read_shader_file(vert_shader_path, &vert_src_size);
	char *frag_shader_src = read_shader_file(frag_shader_path, &frag_src_size);

	VkShaderModule vert_shader_module = create_shader_module(vk->_device, vert_shader_src, vert_src_size);
	VkShaderModule frag_shader_module = create_shader_module(vk->_device, frag_shader_src, frag_src_size);

	VkPipelineShaderStageCreateInfo vert_shader_create_info = {};
	vert_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_create_info.module = vert_shader_module;
	vert_shader_create_info.pName = "main" ;

	VkPipelineShaderStageCreateInfo frag_shader_create_info = {};
	frag_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_create_info.module = frag_shader_module;
	frag_shader_create_info.pName = "main" ;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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

	//VkViewport viewport = {};
	//viewport.x = 0;
	//viewport.y = 0;
	//viewport.width = (float)vk->_swapchain_extent.width;
	//viewport.height = (float)vk->_swapchain_extent.height;
	//viewport.minDepth = 0.0f;
	//viewport.maxDepth = 1.0f;

	//VkRect2D scissor = {};
	//scissor.offset = (VkOffset2D){0, 0};
	//scissor.extent = vk->_swapchain_extent;

	
	VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
	viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	// With dynamic state, the actual viewport and scissor will be later set at drawing time
	// Without dynamic state, they need to be set here, which makes them immutable - any changes needed require
	// creating a new pipeline
	// Can create multiple viewport and scissor on some GPU, need to enable in GPU features when creating
	// logical device
	viewport_state_create_info.scissorCount = 1;
	viewport_state_create_info.viewportCount = 1;

	// # Rasterizer

	VkPipelineRasterizationStateCreateInfo rasterizer_create_info = {};
	rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_create_info.depthClampEnable = VK_FALSE;
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
	// if false, new color from fragment shader is passed through unmodified
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

	CONTINUE_HERE

	return SUCCESS;

};

Result app_init(AppState *app)
{
	if (init_sdl(app) != SUCCESS) return FAILURE;
	if (create_vulkan_instance(&app->_vk) != SUCCESS) return FAILURE;
	if (create_vulkan_surface(app) != SUCCESS) return FAILURE;
	if (pick_physical_device(&app->_vk) != SUCCESS) return FAILURE;
	if (create_logical_device(&app->_vk) != SUCCESS) return FAILURE;
	if (create_swapchain(&app->_vk, app->_window) != SUCCESS) return FAILURE;
	if (create_image_view(&app->_vk) != SUCCESS) return FAILURE;

	return SUCCESS;
};
Result app_mainloop(AppState *app)
{
    return SUCCESS;
};
void app_quit(AppState *app)
{
#ifndef NDEBUG
	destroy_debug_messenter_util(app->_vk._instance, app->_vk._debug_messenger, nullptr);
#endif
	for (uint32_t i = 0; i < app->_vk._swapchain_imageviews_count; i++)
	{
		vkDestroyImageView(app->_vk._device, app->_vk._swapchain_imageviews[i], nullptr);
	};
	free(app->_vk._swapchain_imageviews);
	free(app->_vk._swapchain_images);
	vkDestroyPipelineLayout(app->_vk._device, app->_vk._pipeline_layout, nullptr);
	vkDestroySwapchainKHR(app->_vk._device, app->_vk._swapchain, nullptr);
	vkDestroyDevice(app->_vk._device, nullptr);
	vkDestroySurfaceKHR(app->_vk._instance, app->_vk._surface, nullptr);
    vkDestroyInstance(app->_vk._instance, nullptr);
    SDL_DestroyWindow(app->_window);
    free(app);
};
