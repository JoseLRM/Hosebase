#include "Hosebase/defines.h"

#if SV_GRAPHICS

#define VMA_IMPLEMENTATION
#define SV_VULKAN_IMPLEMENTATION
// #define VOLK_IMPLEMENTATION
#include "graphics_vulkan.h"
#include "Hosebase/external/volk.c"

namespace sv {

    static std::unique_ptr<Graphics_vk> g_API;

	Graphics_vk& graphics_vulkan_device_get()
	{
		return *g_API;
	}
	
    static VkDescriptorSet update_descriptors(const ShaderDescriptorSetLayout& layout, ShaderType shader_type, bool constant_buffers, bool shader_resources, bool unordered_access_views, bool samplers, CommandList cmd_);
    static void update_graphics_state(CommandList cmd_);
	static void update_compute_state(CommandList cmd_);

    ////////////////////////////////////////////DEBUG/////////////////////////////////////////////////
    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
			VkDebugUtilsMessageSeverityFlagsEXT flags,
			const VkDebugUtilsMessengerCallbackDataEXT* data,
			void* userData
		)
    {
		switch (msgSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			SV_LOG_WARNING("[VULKAN] %s\n", data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			SV_LOG_WARNING("[VULKAN] %s\n", data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			SV_LOG_ERROR("[VULKAN] %s\n", data->pMessage);
			break;
		}
		return VK_FALSE;
    }

    VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerCreateInfoEXT* create_info, const VkAllocationCallbacks* alloc, VkDebugUtilsMessengerEXT* debug)
    {
		auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		
		if (fn) return fn(instance, create_info, alloc, debug);
		return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    void __stdcall vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug)
    {
		auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (fn) fn(instance, debug, nullptr);
    }
    void __stdcall vkCmdInsertDebugUtilsLabelEXT(VkInstance instance, VkCommandBuffer cmd, const VkDebugUtilsLabelEXT* info)
    {
		auto fn = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
		if (fn) fn(cmd, info);
    }
    void __stdcall vkCmdBeginDebugUtilsLabelEXT(VkInstance instance, VkCommandBuffer cmd, const VkDebugUtilsLabelEXT* info)
    {
		auto fn = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
		if (fn) fn(cmd, info);
    }
    void __stdcall vkCmdEndDebugUtilsLabelEXT(VkInstance instance, VkCommandBuffer cmd)
    {
		auto fn = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
		if (fn) fn(cmd);
    }

    //////////////////////////////////////////// MEMORY //////////////////////////////////////////////////

    inline static VkResult create_stagingbuffer(StagingBuffer& buffer, VkDeviceSize size)
    {
		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = size;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer_info.queueFamilyIndexCount = 0u;
		buffer_info.pQueueFamilyIndices = nullptr;

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
		alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

		vkExt(vmaCreateBuffer(g_API->allocator, &buffer_info, &alloc_info, &buffer.buffer, &buffer.allocation, nullptr));

		buffer.data = buffer.allocation->GetMappedData();

		return VK_SUCCESS;
    }

    inline static VkResult destroy_stagingbuffer(StagingBuffer& buffer)
    {
		vmaDestroyBuffer(g_API->allocator, buffer.buffer, buffer.allocation);
		return VK_SUCCESS;
    }

    constexpr u32 ALLOCATOR_BLOCK_SIZE = 500000u;

    inline static VulkanGPUAllocator::Buffer create_allocator_buffer(u32 size)
    {
		VulkanGPUAllocator::Buffer buffer;

		create_stagingbuffer(buffer.staging_buffer, size);
		buffer.current = (u8*)buffer.staging_buffer.data;
		// TODO: handle error

		return buffer;
    }

    inline static bool _alloc_buffer(DynamicAllocation& a, VulkanGPUAllocator::Buffer& b, size_t size, size_t alignment)
    {
		u8* data = (u8*)((reinterpret_cast<size_t>(b.current) + (alignment - 1)) & ~(alignment - 1));

		u8* end = data + size;

		if (end <= b.end) {

			a.buffer = b.staging_buffer.buffer;
			a.data = data;
			a.offset = u32(data - (u8*)b.staging_buffer.data);

			b.current = end;
			return true;
		}
		return false;
    }

    inline static DynamicAllocation allocate_gpu(u32 size, size_t alignment, CommandList cmd)
    {
		assert((0 != alignment) && (alignment | (alignment - 1)));

		VulkanGPUAllocator& allocator = g_API->GetFrame().allocator[cmd];
		DynamicAllocation a;

		if (size >= ALLOCATOR_BLOCK_SIZE) {

			VulkanGPUAllocator::Buffer& buffer = allocator.buffers.emplace_back();
			buffer = create_allocator_buffer(size);

			// TODO: Should align this??
			a.buffer = buffer.staging_buffer.buffer;
			a.data = (u8*)buffer.staging_buffer.data;
			a.offset = 0u;

			buffer.current = (u8*)a.data + size;
			buffer.end = buffer.current;
		}
		else {

			// Using active buffers

			for (VulkanGPUAllocator::Buffer& b : allocator.buffers) {

				if (_alloc_buffer(a, b, size, alignment)) break;
			}

			if (!a.isValid()) {

				// Create new buffer
				VulkanGPUAllocator::Buffer& buffer = allocator.buffers.emplace_back();
				buffer = create_allocator_buffer(ALLOCATOR_BLOCK_SIZE + u32(alignment));
				buffer.current = (u8*)buffer.staging_buffer.data;
				buffer.end = buffer.current + ALLOCATOR_BLOCK_SIZE + alignment;

				_alloc_buffer(a, buffer, size, alignment);
			}
		}

		return a;
    }

    //////////////////////////////////////////// DESCRIPTORS ///////////////////////////////////////////////

    VkDescriptorSet allocate_descriptors_sets(DescriptorPool& descPool, const ShaderDescriptorSetLayout& layout)
    {
// Try to use allocated set
		auto it = descPool.sets.find(layout.setLayout);
		if (it != descPool.sets.end()) {
			VulkanDescriptorSet& sets = it->second;
			if (sets.used < sets.sets.size()) {
				return sets.sets[sets.used++];
			}
		}

		if (it == descPool.sets.end()) descPool.sets[layout.setLayout] = {};
		VulkanDescriptorSet& sets = descPool.sets[layout.setLayout];
		VulkanDescriptorPool* pool = nullptr;
		Graphics_vk& gfx = graphics_vulkan_device_get();

		// Try to find existing pool
		if (!descPool.pools.empty()) {
			for (auto it = descPool.pools.begin(); it != descPool.pools.end(); ++it) {

				bool si = true;

				if (!(it->sets)) {
					continue;
				}

				foreach(i, VulkanDescriptorType_MaxEnum) {
					if (it->count[i] < layout.count[i] * VULKAN_DESCRIPTOR_ALLOC_COUNT) {
						si = false;
						break;
					}
				}
				
				if (si) {
					pool = &(*it);
					break;
				}
			}
		}

		// Create new pool if necessary
		if (pool == nullptr) {

			descPool.pools.emplace_back();
			pool = &descPool.pools.back();

			VkDescriptorPoolSize sizes[VulkanDescriptorType_MaxEnum];

			foreach(i, VulkanDescriptorType_MaxEnum) {
				sizes[i].descriptorCount = VULKAN_MAX_DESCRIPTOR_TYPES * VULKAN_MAX_DESCRIPTOR_SETS;

				switch (i) {
					
				case VulkanDescriptorType_UniformBuffer:
					sizes[i].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					break;

				case VulkanDescriptorType_UniformTexelBuffer:
					sizes[i].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
					break;
					
				case VulkanDescriptorType_SampledImage:
					sizes[i].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
					break;
					
				case VulkanDescriptorType_StorageBuffer:
					sizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					break;

				case VulkanDescriptorType_StorageImage:
					sizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
					break;
					
				case VulkanDescriptorType_StorageTexelBuffer:
					sizes[i].type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
					break;
					
				case VulkanDescriptorType_Sampler:
					sizes[i].type = VK_DESCRIPTOR_TYPE_SAMPLER;
					break;
					
				}
			}

			VkDescriptorPoolCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			create_info.maxSets = VULKAN_MAX_DESCRIPTOR_SETS;
			create_info.poolSizeCount = VulkanDescriptorType_MaxEnum;
			create_info.pPoolSizes = sizes;

			vkAssert(vkCreateDescriptorPool(gfx.device, &create_info, nullptr, &pool->pool));

			pool->sets = VULKAN_MAX_DESCRIPTOR_SETS;
			for (u32 i = 0; i < VulkanDescriptorType_MaxEnum; ++i)
				pool->count[i] = VULKAN_MAX_DESCRIPTOR_TYPES;
		}
	
		// Allocate sets
		{
			foreach(i, VulkanDescriptorType_MaxEnum) {
				pool->count[i] -= layout.count[i] * VULKAN_DESCRIPTOR_ALLOC_COUNT;
			}
			
			pool->sets -= VULKAN_DESCRIPTOR_ALLOC_COUNT;
	    
			size_t index = sets.sets.size();
			sets.sets.resize(index + VULKAN_DESCRIPTOR_ALLOC_COUNT);

			VkDescriptorSetLayout setLayouts[VULKAN_DESCRIPTOR_ALLOC_COUNT];
			for (u32 i = 0; i < VULKAN_DESCRIPTOR_ALLOC_COUNT; ++i)
				setLayouts[i] = layout.setLayout;

			VkDescriptorSetAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			alloc_info.descriptorPool = pool->pool;
			alloc_info.descriptorSetCount = VULKAN_DESCRIPTOR_ALLOC_COUNT;
			alloc_info.pSetLayouts = setLayouts;

			vkAssert(vkAllocateDescriptorSets(gfx.device, &alloc_info, sets.sets.data() + index));

			return sets.sets[sets.used++];
		}
    }

    void graphics_vulkan_descriptors_reset(DescriptorPool& descPool)
    {
		for (auto& set : descPool.sets) {
			set.second.used = 0u;
		}
    }

    void graphics_vulkan_descriptors_clear(DescriptorPool& descPool)
    {
		Graphics_vk& gfx = graphics_vulkan_device_get();

		for (auto it = descPool.pools.begin(); it != descPool.pools.end(); ++it) {
			vkDestroyDescriptorPool(gfx.device, it->pool, nullptr);
		}

		descPool.sets.clear();
		descPool.pools.clear();
    }
	
    //////////////////////////////////////////// DEVICE /////////////////////////////////////////////////

}

void graphics_vulkan_device_prepare(GraphicsDevice* device_)
{
	GraphicsDevice& device = *device_;

	if (volkInitialize() != VK_SUCCESS)
	{
		SV_LOG_ERROR("Can't initialize volk\n");
	}
			
	device.initialize			= sv::graphics_vulkan_initialize;
	device.close				= sv::graphics_vulkan_close;
	device.get				    = sv::graphics_vulkan_get;
	device.create				= sv::graphics_vulkan_create;
	device.destroy				= sv::graphics_vulkan_destroy;
	device.commandlist_begin	= sv::graphics_vulkan_commandlist_begin;
	device.commandlist_last		= sv::graphics_vulkan_commandlist_last;
	device.commandlist_count	= sv::graphics_vulkan_commandlist_count;
	device.renderpass_begin		= sv::graphics_vulkan_renderpass_begin;
	device.renderpass_end		= sv::graphics_vulkan_renderpass_end;
	device.swapchain_resize		= sv::graphics_vulkan_swapchain_resize;
	device.gpu_wait				= sv::graphics_vulkan_gpu_wait;
	device.frame_begin			= sv::graphics_vulkan_frame_begin;
	device.frame_end			= sv::graphics_vulkan_frame_end;
	device.draw				    = sv::graphics_vulkan_draw;
	device.draw_indexed			= sv::graphics_vulkan_draw_indexed;
	device.dispatch             = sv::graphics_vulkan_dispatch;
	device.image_clear			= sv::graphics_vulkan_image_clear;
	device.image_blit			= sv::graphics_vulkan_image_blit;
	device.buffer_update		= sv::graphics_vulkan_buffer_update;
	device.barrier				= sv::graphics_vulkan_barrier;
	device.event_begin			= sv::graphics_vulkan_event_begin;
	device.event_mark			= sv::graphics_vulkan_event_mark;
	device.event_end			= sv::graphics_vulkan_event_end;

	device.buffer_allocator              = instance_allocator_init(sizeof(sv::Buffer_vk), 200u);
	device.image_allocator			     = instance_allocator_init(sizeof(sv::Image_vk), 200u);
	device.sampler_allocator			 = instance_allocator_init(sizeof(sv::Sampler_vk), 200u);
	device.shader_allocator			     = instance_allocator_init(sizeof(sv::Shader_vk), 200u);
	device.render_pass_allocator		 = instance_allocator_init(sizeof(sv::RenderPass_vk), 200u);
	device.blend_state_allocator		 = instance_allocator_init(sizeof(sv::BlendState_vk), 200u);
	device.depth_stencil_state_allocator = instance_allocator_init(sizeof(sv::DepthStencilState_vk), 200u);
		
	device.api = GraphicsAPI_Vulkan;
}

namespace sv {

    b8 graphics_vulkan_initialize(b8 validation)
    {
		g_API = std::make_unique<Graphics_vk>();
		
		g_API->mutexCMD = mutex_create();
		g_API->pipeline_mutex = mutex_create();
		g_API->IDMutex = mutex_create();

		// Instance extensions and validation layers

		if (validation) {
			g_API->validationLayers.push_back("VK_LAYER_KHRONOS_validation");
			g_API->extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		
		g_API->extensions.push_back("VK_KHR_surface");

#if SV_PLATFORM_WINDOWS
		g_API->extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif SV_PLATFORM_ANDROID
		g_API->extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

		// Device extensions and validation layers
		if (validation) {
			g_API->deviceValidationLayers.push_back("VK_LAYER_KHRONOS_validation");
		}
		g_API->deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		// Create instance
		{
			// Check Extensions
			{
				u32 count = 0u;
				vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
				std::vector<VkExtensionProperties> props(count);
				vkEnumerateInstanceExtensionProperties(nullptr, &count, props.data());

				for (u32 i = 0; i < g_API->extensions.size(); ++i) {
					bool found = false;

					for (u32 j = 0; j < props.size(); ++j) {
						if (strcmp(props[j].extensionName, g_API->extensions[i]) == 0) {
							found = true;
							break;
						}
					}

					if (!found) {
						SV_LOG_ERROR("InstanceExtension '%s' not found\n", g_API->extensions[i]);
						return false;
					}
				}
			}

			// Check Validation Layers
			{
				u32 count = 0u;
				vkEnumerateInstanceLayerProperties(&count, nullptr);
				std::vector<VkLayerProperties> props(count);
				vkEnumerateInstanceLayerProperties(&count, props.data());

				for (u32 i = 0; i < g_API->validationLayers.size(); ++i) {
					bool found = false;

					for (u32 j = 0; j < props.size(); ++j) {
						if (strcmp(props[j].layerName, g_API->validationLayers[i]) == 0) {
							found = true;
							break;
						}
					}

					if (!found) {
						SV_LOG_ERROR("InstanceValidationLayer '%s' not found\n", g_API->validationLayers[i]);
						return false;
					}
				}
			}

			// Create Instance
			VkApplicationInfo app_info{};
			app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			app_info.pEngineName = "SilverEngine";
			app_info.pApplicationName = "";
			//Version engineVersion = GetEngine().GetVersion();
			//app_info.engineVersion = VK_MAKE_VERSION(engineVersion.major, engineVersion.minor, engineVersion.revision);
			app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
			app_info.apiVersion = VK_VERSION_1_2;

			VkInstanceCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			create_info.pApplicationInfo = &app_info;

			create_info.enabledExtensionCount = u32(g_API->extensions.size());
			create_info.ppEnabledExtensionNames = g_API->extensions.data();
			create_info.enabledLayerCount = u32(g_API->validationLayers.size());
			create_info.ppEnabledLayerNames = g_API->validationLayers.data();

			create_info.flags = 0u;

			vkCheck(vkCreateInstance(&create_info, nullptr, &g_API->instance));

			volkLoadInstance(g_API->instance);
		}

		// Initialize validation layers
		if (validation) {
			VkDebugUtilsMessengerCreateInfoEXT create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			create_info.flags = 0u;
			create_info.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
			create_info.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			create_info.pfnUserCallback = DebugCallback;
			create_info.pUserData = nullptr;
			
			vkCheck(vkCreateDebugUtilsMessengerEXT(g_API->instance, &create_info, nullptr, &g_API->debug));

			SV_LOG_INFO("Vulkan validation enabled\n");
		}

		// Create adapter
		{
			// Enumerate Vulkan Adapters
			u32 count = 0u;
			vkCheck(vkEnumeratePhysicalDevices(g_API->instance, &count, nullptr));

			std::vector<VkPhysicalDevice> devices(count);
			vkCheck(vkEnumeratePhysicalDevices(g_API->instance, &count, devices.data()));

			u32 deviceIndex = u32_max;
			u32 indexSuitability = 0u;

			// Choose graphics card
			for (u32 i = 0; i < devices.size(); ++i) {
				
				VkPhysicalDevice device = devices[i];

				VkPhysicalDeviceProperties			props;
				VkPhysicalDeviceFeatures			features;
				VkPhysicalDeviceMemoryProperties	mem_props;
				VkFormatProperties	format_props;

				u32 suitability = 0u;

				u32 familyindex_graphics = u32_max;

				vkGetPhysicalDeviceProperties(device, &props);
				vkGetPhysicalDeviceFeatures(device, &features);
				vkGetPhysicalDeviceMemoryProperties(device, &mem_props);
				vkGetPhysicalDeviceFormatProperties(device, VK_FORMAT_R32_UINT, &format_props);

				if (format_props.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT) {
					suitability++;
				}

				// Choose FamilyQueueIndices
				{
					count = 0u;
					vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
					std::vector<VkQueueFamilyProperties> props(count);
					vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());

					for (u32 i = 0; i < props.size(); ++i) {
						const VkQueueFamilyProperties& prop = props[i];

						bool hasGraphics = prop.queueFlags & VK_QUEUE_GRAPHICS_BIT;

						if (hasGraphics) familyindex_graphics = i;
					}
				}

				// Suitability
				bool valid = true;
				suitability++;

				switch (props.deviceType)
				{
				case VK_PHYSICAL_DEVICE_TYPE_OTHER:
					suitability += 0;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
					suitability += 5;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
					suitability += 20;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
					suitability += 2;
					break;
				case VK_PHYSICAL_DEVICE_TYPE_CPU:
					suitability += 2;
					break;
				}

				if (!valid) suitability = 0u;
				
				if (suitability > indexSuitability) {
					indexSuitability = suitability;
					deviceIndex = i;

					g_API->card.familyIndex.graphics = familyindex_graphics;
					g_API->card.properties = props;
					g_API->card.memoryProps = mem_props;
					g_API->card.features = features;
					g_API->card.physicalDevice = device;
				}
			}

			// Adapter not found
			if (deviceIndex == u32_max) {
				SV_LOG_ERROR("Can't find valid graphics card for Vulkan\n");
				return false;
			}

			SV_LOG_INFO("Using '%s'\n", g_API->card.properties.deviceName);
		}
		
		// Create logical device
		{
			auto& card = g_API->card;

			// Queue Create Info Structs
			constexpr u32 queueCount = 1u;
			u32 queueIndices[] = {
				card.familyIndex.graphics
			};

			const f32 priorities = 1.f;

			VkDeviceQueueCreateInfo queue_create_info[queueCount];
			for (u32 i = 0; i < queueCount; ++i) {
				queue_create_info[i] = {};
				queue_create_info[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queue_create_info[i].flags = 0u;
				queue_create_info[i].queueFamilyIndex = queueIndices[i];
				queue_create_info[i].queueCount = 1u;
				queue_create_info[i].pQueuePriorities = &priorities;
			}

			// Get available extensions and validation layers
			std::vector<VkExtensionProperties> available_extensions;
			std::vector<VkLayerProperties> available_layers;
			{
				u32 available_extension_count;
				u32 available_layer_count;

				vkEnumerateDeviceExtensionProperties(card.physicalDevice, nullptr, &available_extension_count, nullptr);
				vkEnumerateDeviceLayerProperties(card.physicalDevice, &available_layer_count, nullptr);

				if (available_extension_count == 0u) {
					SV_LOG_ERROR("There's no available device extensions\n");
					return false;
				}

				available_extensions.resize(available_extension_count);
				vkEnumerateDeviceExtensionProperties(card.physicalDevice, nullptr, &available_extension_count, available_extensions.data());
				available_layers.resize(available_layer_count);
				vkEnumerateDeviceLayerProperties(card.physicalDevice, &available_layer_count, available_layers.data());
			}

			// Check device extensions
			for (auto it = g_API->deviceExtensions.begin(); it != g_API->deviceExtensions.end(); ) {

				const char* ext = *it;

				bool found = false;

				for (const VkExtensionProperties& e : available_extensions) {

					if (strcmp(e.extensionName, ext) == 0) {
						found = true;
						break;
					}
				}

				if (!found) {

					SV_LOG_ERROR("Device extension '%s' not available\n", ext);
					it = g_API->deviceExtensions.erase(it);
				}
				else ++it;
			}

			// Check device validationLayers
			for (auto it = g_API->deviceValidationLayers.begin(); it != g_API->deviceValidationLayers.end(); ) {

				const char* layer = *it;

				bool found = false;

				for (const VkLayerProperties& e : available_layers) {

					if (strcmp(e.layerName, layer) == 0) {
						found = true;
						break;
					}
				}

				if (!found) {

					SV_LOG_ERROR("Device validation layer '%s' not available\n", layer);
					it = g_API->deviceValidationLayers.erase(it);
				}
				else ++it;
			}

			// Create
			VkDeviceCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			create_info.flags = 0u;
			create_info.queueCreateInfoCount = queueCount;
			create_info.pQueueCreateInfos = queue_create_info;

			create_info.enabledLayerCount = u32(g_API->deviceValidationLayers.size());
			create_info.ppEnabledLayerNames = g_API->deviceValidationLayers.data();
			create_info.enabledExtensionCount = u32(g_API->deviceExtensions.size());
			create_info.ppEnabledExtensionNames = g_API->deviceExtensions.data();
			
			create_info.pEnabledFeatures = &card.features;

			vkCheck(vkCreateDevice(card.physicalDevice, &create_info, nullptr, &g_API->device));

			// Get Queues
			vkGetDeviceQueue(g_API->device, card.familyIndex.graphics, 0u, &g_API->queueGraphics);
		}

		// Create allocator
		{
			VmaAllocatorCreateInfo create_info{};
			create_info.device = g_API->device;
			create_info.instance = g_API->instance;
			create_info.physicalDevice = g_API->card.physicalDevice;

			vkCheck(vmaCreateAllocator(&create_info, &g_API->allocator));
		}
		
		// Create frames
		{
			g_API->frameCount = 3u;
			g_API->frames.resize(g_API->frameCount);

			VkCommandPoolCreateInfo cmdPool_info{};
			cmdPool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			cmdPool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			cmdPool_info.queueFamilyIndex = g_API->card.familyIndex.graphics;

			for (u32 i = 0; i < g_API->frameCount; ++i) {
				Frame& frame = g_API->frames[i];

				frame.fence = graphics_vulkan_fence_create(true);

				vkCheck(vkCreateCommandPool(g_API->device, &cmdPool_info, nullptr, &frame.commandPool));

				cmdPool_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
				vkCheck(vkCreateCommandPool(g_API->device, &cmdPool_info, nullptr, &frame.transientCommandPool));

				frame.transient_command_pool_mutex = mutex_create();

				// AllocateCommandBuffers
				VkCommandBufferAllocateInfo alloc_info{};
				alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				alloc_info.commandPool = frame.commandPool;
				alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				alloc_info.commandBufferCount = GraphicsLimit_CommandList;

				vkCheck(vkAllocateCommandBuffers(g_API->device, &alloc_info, frame.commandBuffers));
			}
		}

		// Create swapchain
		SV_CHECK(graphics_vulkan_swapchain_create());

		return true;
    }

    b8 graphics_vulkan_close()
    {
		vkDeviceWaitIdle(g_API->device);

		mutex_destroy(g_API->mutexCMD);
		mutex_destroy(g_API->pipeline_mutex);
		mutex_destroy(g_API->IDMutex);

		// Destroy swapchain
		graphics_vulkan_swapchain_destroy(false);
	
		// Destroy Pipelines
		for (auto& it : g_API->pipelines) {
			graphics_vulkan_pipeline_destroy(it.second);
		}
		g_API->pipelines.clear();

		// Destroy frames
		for (u32 i = 0; i < g_API->frameCount; ++i) {
			Frame& frame = g_API->frames[i];
			vkDestroyCommandPool(g_API->device, frame.commandPool, nullptr);
			vkDestroyCommandPool(g_API->device, frame.transientCommandPool, nullptr);
			mutex_destroy(frame.transient_command_pool_mutex);
			vkDestroyFence(g_API->device, frame.fence, nullptr);

			foreach (i, GraphicsLimit_CommandList) {
				graphics_vulkan_descriptors_clear(frame.descPool[i]);
			}

			// Destroy dynamic allocators
			foreach(i, GraphicsLimit_CommandList) {
				
				VulkanGPUAllocator& allocator = frame.allocator[i];

				for (VulkanGPUAllocator::Buffer& buffer : allocator.buffers) {
					destroy_stagingbuffer(buffer.staging_buffer);
				}

				allocator.buffers.clear();
			}
		}

		// Destroy VMA Allocator
		vmaDestroyAllocator(g_API->allocator);

		// Destroy device and vulkan instance
		vkDestroyDevice(g_API->device, nullptr);

		if (g_API->debug != VK_NULL_HANDLE)
			vkDestroyDebugUtilsMessengerEXT(g_API->instance, g_API->debug);

		vkDestroyInstance(g_API->instance, nullptr);
		return true;
    }

    void* graphics_vulkan_get()
    {
		return g_API.get();
    }

    b8 graphics_vulkan_create(GraphicsPrimitiveType type, const void* desc, GraphicsPrimitive* ptr)
    {
		switch (type)
		{

		case GraphicsPrimitiveType_Buffer:
		{
			Buffer_vk* b = new(ptr) Buffer_vk();
			return graphics_vulkan_buffer_create(*b, *reinterpret_cast<const GPUBufferDesc*>(desc));
		}
		break;

		case GraphicsPrimitiveType_Shader:
		{
			Shader_vk* s = new(ptr) Shader_vk();
			return graphics_vulkan_shader_create(*s, *reinterpret_cast<const ShaderDesc*>(desc));
		}
		break;

		case GraphicsPrimitiveType_Image:
		{
			Image_vk* i = new(ptr) Image_vk();
			return graphics_vulkan_image_create(*i, *reinterpret_cast<const GPUImageDesc*>(desc));
		}
		break;

		case GraphicsPrimitiveType_Sampler:
		{
			Sampler_vk* i = new(ptr) Sampler_vk();
			return graphics_vulkan_sampler_create(*i, *reinterpret_cast<const SamplerDesc*>(desc));
		}
		break;

		case GraphicsPrimitiveType_RenderPass:
		{
			RenderPass_vk* rp = new(ptr) RenderPass_vk();
			return graphics_vulkan_renderpass_create(*rp, *reinterpret_cast<const RenderPassDesc*>(desc));
		}
		break;

		case GraphicsPrimitiveType_BlendState:
		{
			BlendState_vk* bs = new(ptr) BlendState_vk();
			return graphics_vulkan_blendstate_create(*bs, *reinterpret_cast<const BlendStateDesc*>(desc));
		}
		break;

		case GraphicsPrimitiveType_DepthStencilState:
		{
			DepthStencilState_vk* dss = new(ptr) DepthStencilState_vk();
			return graphics_vulkan_depthstencilstate_create(*dss, *reinterpret_cast<const DepthStencilStateDesc*>(desc));
		}
		break;

		}

		return false;
    }

    b8 graphics_vulkan_destroy(GraphicsPrimitive* primitive)
    {
		vkDeviceWaitIdle(g_API->device);

		bool result = true;

		switch (primitive->type)
		{

		case GraphicsPrimitiveType_Buffer:
		{
			Buffer_vk& buffer = *reinterpret_cast<Buffer_vk*>(primitive);
			result = graphics_vulkan_buffer_destroy(buffer);
			buffer.~Buffer_vk();
			break;
		}

		case GraphicsPrimitiveType_Shader:
		{
			Shader_vk& shader = *reinterpret_cast<Shader_vk*>(primitive);
			result = graphics_vulkan_shader_destroy(shader);
			shader.~Shader_vk();
			break;
		}

		case GraphicsPrimitiveType_Image:
		{
			Image_vk& image = *reinterpret_cast<Image_vk*>(primitive);
			result = graphics_vulkan_image_destroy(image);
			image.~Image_vk();
			break;
		}

		case GraphicsPrimitiveType_Sampler:
		{
			Sampler_vk& sampler = *reinterpret_cast<Sampler_vk*>(primitive);
			result = graphics_vulkan_sampler_destroy(sampler);
			sampler.~Sampler_vk();
			break;
		}

		case GraphicsPrimitiveType_RenderPass:
		{
			RenderPass_vk& renderPass = *reinterpret_cast<RenderPass_vk*>(primitive);
			result = graphics_vulkan_renderpass_destroy(renderPass);
			renderPass.~RenderPass_vk();
			break;
		}

		case GraphicsPrimitiveType_BlendState:
		{
			BlendState_vk& blendState = *reinterpret_cast<BlendState_vk*>(primitive);
			blendState.~BlendState_vk();
			result = true;
			break;
		}

		case GraphicsPrimitiveType_DepthStencilState:
		{
			DepthStencilState_vk& depthStencilState = *reinterpret_cast<DepthStencilState_vk*>(primitive);
			depthStencilState.~DepthStencilState_vk();
			result = true;
			break;
		}

		default:
			return true;

		}

		return result;
    }

    CommandList graphics_vulkan_commandlist_begin()
    {
		mutex_lock(g_API->mutexCMD);
		CommandList index = g_API->activeCMDCount++;
		VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[index];
		mutex_unlock(g_API->mutexCMD);

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkAssert(vkBeginCommandBuffer(cmd, &begin_info));

		return index;
    }

    CommandList graphics_vulkan_commandlist_last()
    {
		SV_LOCK_GUARD(g_API->mutexCMD, lock);
		assert(g_API->activeCMDCount != 0);
		return g_API->activeCMDCount - 1u;
    }

    u32 graphics_vulkan_commandlist_count()
    {
		SV_LOCK_GUARD(g_API->mutexCMD, lock);
		return g_API->activeCMDCount;
    }

    void graphics_vulkan_renderpass_begin(CommandList cmd_)
    {
		GraphicsState& state = graphics_state_get()->graphics[cmd_];
		RenderPass_vk& renderPass = *reinterpret_cast<RenderPass_vk*>(state.render_pass);
		VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[cmd_];

		u32 att_count = renderPass.info.attachment_count;

		// FrameBuffer
		VkFramebuffer fb = VK_NULL_HANDLE;
		{
			// Calculate hash value for attachments
			u64 hash = 0u;
			hash = hash_combine(hash, att_count);
			foreach(i, att_count) {
				Image_vk& att = *reinterpret_cast<Image_vk*>(state.attachments[i]);
				hash = hash_combine(hash, att.ID);
			}

			// Find framebuffer
			SV_LOCK_GUARD(renderPass.mutex, lock);

			for (auto it = renderPass.frameBuffers.begin(); it != renderPass.frameBuffers.end(); ++it) {

				if (it->first == hash) {
					fb = it->second;
					break;
				}
			}
			
			if (fb == VK_NULL_HANDLE) {

				// Create attachments views list
				VkImageView views[GraphicsLimit_Attachment];
				u32 width = 0, height = 0, layers = 0;

				foreach(i, att_count) {
					Image_vk& att = *reinterpret_cast<Image_vk*>(state.attachments[i]);

					if (renderPass.info.depthstencil_attachment_index == i) {
						views[i] = att.depth_stencil_view;
					}
					else {
						views[i] = att.render_target_view;
					}

					if (i == 0) {
						width = att.info.width;
						height = att.info.height;
						layers = att.layers;
					}
				}
				
				VkFramebufferCreateInfo create_info{};
				create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				create_info.renderPass = renderPass.renderPass;
				create_info.attachmentCount = att_count;
				create_info.pAttachments = views;
				create_info.width = width;
				create_info.height = height;
				create_info.layers = layers;

				vkAssert(vkCreateFramebuffer(g_API->device, &create_info, nullptr, &fb));

				renderPass.frameBuffers.push_back({ hash, fb });
			}
		}	

		// Color Attachments clear values
		VkClearValue clearValues[GraphicsLimit_Attachment];
		foreach(i, att_count) {

			if (renderPass.info.depthstencil_attachment_index == i) {
				clearValues[i].depthStencil.depth = state.clear_depth;
				clearValues[i].depthStencil.stencil = state.clear_stencil;

				clearValues[i].color = {};
			}
			else {
				clearValues[i].depthStencil.depth = 1.f;
				clearValues[i].depthStencil.stencil = 0u;

				clearValues[i].color = { state.clear_colors[i].x, state.clear_colors[i].y, state.clear_colors[i].z, state.clear_colors[i].w };
			}
		}

		renderPass.beginInfo.pClearValues = clearValues;
		renderPass.beginInfo.renderArea.extent.width = state.attachments[0]->info.width;
		renderPass.beginInfo.renderArea.extent.height = state.attachments[0]->info.height;
		renderPass.beginInfo.framebuffer = fb;

		vkCmdBeginRenderPass(cmd, &renderPass.beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void graphics_vulkan_renderpass_end(CommandList cmd_)
    {
		//GraphicsState& state = graphics_state_get().graphics[cmd_];
		//RenderPass_vk& renderPass = *reinterpret_cast<RenderPass_vk*>(state.renderPass);
		VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[cmd_];
	    
		vkCmdEndRenderPass(cmd);
    }

    void graphics_vulkan_swapchain_resize()
    {
		SwapChain_vk& sc = g_API->swapchain;

		vkAssert(vkDeviceWaitIdle(g_API->device));
		VkSwapchainKHR old = sc.swapChain;
		graphics_vulkan_swapchain_destroy(true);
		graphics_vulkan_swapchain_create();
		vkDestroySwapchainKHR(g_API->device, old, nullptr);
    }

    void graphics_vulkan_gpu_wait()
    {
		vkAssert(vkDeviceWaitIdle(g_API->device));
    }

    void graphics_vulkan_frame_begin()
    {
		f64 now = timer_now();

		Frame& frame = g_API->frames[g_API->currentFrame];

		// Reset Descriptors
		for (u32 i = 0; i < GraphicsLimit_CommandList; ++i) {
			graphics_vulkan_descriptors_reset(frame.descPool[i]);
		}

		// Reset dynamic allocator
		{
			// TODO: free unused memory

			foreach(i, GraphicsLimit_CommandList) {
			
				VulkanGPUAllocator& allocator = frame.allocator[i];

				for (VulkanGPUAllocator::Buffer& buffer : allocator.buffers) {
					buffer.current = (u8*)buffer.staging_buffer.data;
				}
			}
		}

		// Destroy unused objects
		if (now - g_API->lastTime >= VULKAN_UNUSED_OBJECTS_TIMECHECK) {

			vkAssert(vkDeviceWaitIdle(g_API->device));

			// Pipelines
			auto next_it = g_API->pipelines.begin();
			for (auto it = next_it; it != g_API->pipelines.end(); it = next_it) {
				++next_it;
				//size_t hash = it->first;
				auto& pipeline = it->second;
				if (now - pipeline.lastUsage >= VULKAN_UNUSED_OBJECTS_LIFETIME) {
					graphics_vulkan_pipeline_destroy(pipeline);
					g_API->pipelines.erase(it);
				}
			}

			g_API->lastTime = now;
		}

		vkAssert(vkWaitForFences(g_API->device, 1, &frame.fence, VK_TRUE, UINT64_MAX));

		vkAssert(vkResetCommandPool(g_API->device, frame.commandPool, 0u));
    }

    void graphics_vulkan_frame_end()
    {
		graphics_vulkan_acquire_image();
		graphics_vulkan_submit_commandbuffers();
		graphics_vulkan_present();
    }

    void graphics_vulkan_draw(u32 vertexCount, u32 instanceCount, u32 startVertex, u32 startInstance, CommandList cmd)
    {
		update_graphics_state(cmd);
		vkCmdDraw(g_API->frames[g_API->currentFrame].commandBuffers[cmd], vertexCount, instanceCount, startVertex, startInstance);
    }

    void graphics_vulkan_draw_indexed(u32 indexCount, u32 instanceCount, u32 startIndex, u32 startVertex, u32 startInstance, CommandList cmd)
    {
		update_graphics_state(cmd);
		vkCmdDrawIndexed(g_API->frames[g_API->currentFrame].commandBuffers[cmd], indexCount, instanceCount, startIndex, startVertex, startInstance);
    }

	void graphics_vulkan_dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z, CommandList cmd)
	{
		update_compute_state(cmd);
		vkCmdDispatch(g_API->frames[g_API->currentFrame].commandBuffers[cmd], group_count_x, group_count_y, group_count_z);
	}

    void graphics_vulkan_image_clear(GPUImage* image_, GPUImageLayout oldLayout, GPUImageLayout newLayout, Color clearColor, float depth, u32 stencil, CommandList cmd_)
    {
		Image_vk& image = *reinterpret_cast<Image_vk*>(image_);
		VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[cmd_];

		VkImageSubresourceRange range{};
		range.aspectMask = graphics_vulkan_aspect_from_image_layout(oldLayout, image.info.format);
		range.baseMipLevel = 0u;
		range.levelCount = 1u;
		range.baseArrayLayer = 0u;
		range.layerCount = image.layers;

		// Image Barrier: from oldLayout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		VkPipelineStageFlags srcStage = graphics_vulkan_stage_from_image_layout(oldLayout);
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask = graphics_vulkan_access_from_image_layout(oldLayout);
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = graphics_vulkan_parse_image_layout(oldLayout);
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image.image;
		barrier.subresourceRange = range;

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);

		// Clear
		if (image.info.type & GPUImageType_DepthStencil) {

			VkClearDepthStencilValue clear = { depth, stencil };
			vkCmdClearDepthStencilImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1u, &range);

		}
		else {

			v4 col = color_to_v4(clearColor);
			VkClearColorValue clear = { col.x, col.y, col.z, col.w };
			vkCmdClearColorImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1u, &range);

		}

		// Image Barrier: from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to newLayout
		srcStage = dstStage;
		dstStage = graphics_vulkan_stage_from_image_layout(newLayout);

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = graphics_vulkan_access_from_image_layout(newLayout);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = graphics_vulkan_parse_image_layout(newLayout);
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image.image;

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0u, 0u, nullptr, 0u, nullptr, 1u, &barrier);
    }

	
	
    void graphics_vulkan_image_blit(GPUImage* src, GPUImage* dst, GPUImageLayout srcLayout, GPUImageLayout dstLayout, u32 count, const GPUImageBlit* imageBlit, SamplerFilter filter, CommandList cmd_)
    {
		VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[cmd_];

		assert(count <= 16u);

		Image_vk& srcImage = *reinterpret_cast<Image_vk*>(src);
		Image_vk& dstImage = *reinterpret_cast<Image_vk*>(dst);

		// Barriers

		VkImageMemoryBarrier imgBarrier[2];
		imgBarrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imgBarrier[0].pNext = nullptr;
		imgBarrier[0].srcAccessMask = graphics_vulkan_access_from_image_layout(srcLayout);
		imgBarrier[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		imgBarrier[0].oldLayout = graphics_vulkan_parse_image_layout(srcLayout);
		imgBarrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imgBarrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier[0].image = srcImage.image;
		imgBarrier[0].subresourceRange.aspectMask = graphics_vulkan_aspect_from_image_layout(srcLayout);
		imgBarrier[0].subresourceRange.baseArrayLayer = 0u;
		imgBarrier[0].subresourceRange.baseMipLevel = 0u;
		imgBarrier[0].subresourceRange.layerCount = srcImage.layers;
		imgBarrier[0].subresourceRange.levelCount = 1u;

		imgBarrier[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imgBarrier[1].pNext = nullptr;
		imgBarrier[1].srcAccessMask = graphics_vulkan_access_from_image_layout(dstLayout);
		imgBarrier[1].dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		imgBarrier[1].oldLayout = graphics_vulkan_parse_image_layout(dstLayout);
		imgBarrier[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imgBarrier[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgBarrier[1].image = dstImage.image;
		imgBarrier[1].subresourceRange.aspectMask = graphics_vulkan_aspect_from_image_layout(dstLayout);
		imgBarrier[1].subresourceRange.baseArrayLayer = 0u;
		imgBarrier[1].subresourceRange.baseMipLevel = 0u;
		imgBarrier[1].subresourceRange.layerCount = dstImage.layers;
		imgBarrier[1].subresourceRange.levelCount = 1u;

		VkPipelineStageFlags srcStage = graphics_vulkan_stage_from_image_layout(srcLayout) | graphics_vulkan_stage_from_image_layout(dstLayout);
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0u, 0u, 0u, 0u, 0u, 2u, imgBarrier);

		VkImageBlit blits[16];

		for (u32 i = 0; i < count; ++i) {

			blits[i].srcOffsets[0].x = imageBlit[i].src_region.offset0.x;
			blits[i].srcOffsets[0].y = imageBlit[i].src_region.offset0.y;
			blits[i].srcOffsets[0].z = imageBlit[i].src_region.offset0.z;

			blits[i].srcOffsets[1].x = imageBlit[i].src_region.offset1.x;
			blits[i].srcOffsets[1].y = imageBlit[i].src_region.offset1.y;
			blits[i].srcOffsets[1].z = imageBlit[i].src_region.offset1.z;

			blits[i].dstOffsets[0].x = imageBlit[i].dst_region.offset0.x;
			blits[i].dstOffsets[0].y = imageBlit[i].dst_region.offset0.y;
			blits[i].dstOffsets[0].z = imageBlit[i].dst_region.offset0.z;

			blits[i].dstOffsets[1].x = imageBlit[i].dst_region.offset1.x;
			blits[i].dstOffsets[1].y = imageBlit[i].dst_region.offset1.y;
			blits[i].dstOffsets[1].z = imageBlit[i].dst_region.offset1.z;

			blits[i].srcSubresource.aspectMask = graphics_vulkan_aspect_from_image_layout(srcLayout);
			blits[i].srcSubresource.baseArrayLayer = 0u;
			blits[i].srcSubresource.layerCount = srcImage.layers;
			blits[i].srcSubresource.mipLevel = 0u;

			blits[i].dstSubresource.aspectMask = graphics_vulkan_aspect_from_image_layout(dstLayout);
			blits[i].dstSubresource.baseArrayLayer = 0u;
			blits[i].dstSubresource.layerCount = dstImage.layers;
			blits[i].dstSubresource.mipLevel = 0u;

		}

		vkCmdBlitImage(cmd, srcImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   dstImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, count, blits, graphics_vulkan_parse_filter(filter));

		// Barrier
		std::swap(srcStage, dstStage);
		std::swap(imgBarrier[0].srcAccessMask, imgBarrier[0].dstAccessMask);
		std::swap(imgBarrier[0].oldLayout, imgBarrier[0].newLayout);
		std::swap(imgBarrier[1].srcAccessMask, imgBarrier[1].dstAccessMask);
		std::swap(imgBarrier[1].oldLayout, imgBarrier[1].newLayout);

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0u, 0u, 0u, 0u, 0u, 2u, imgBarrier);
    }

    void graphics_vulkan_buffer_update(GPUBuffer* buffer_, GPUBufferState buffer_state, const void* pData, u32 size, u32 offset, CommandList cmd_)
    {
		Buffer_vk& buffer = *reinterpret_cast<Buffer_vk*>(buffer_);

		VmaAllocationInfo info;
		vmaGetAllocationInfo(g_API->allocator, buffer.allocation, &info);
		
		if (buffer.info.buffer_type & GPUBufferType_Constant && buffer.info.usage == ResourceUsage_Dynamic) {

			DynamicAllocation allocation = allocate_gpu(size, 256u, cmd_);
			memcpy(allocation.data, pData, size_t(size));
			buffer.dynamic_allocation[cmd_] = allocation;

			graphics_state_get()->graphics[cmd_].flags |=
				GraphicsPipelineState_ConstantBuffer |
				GraphicsPipelineState_Resource_VS |
				GraphicsPipelineState_Resource_PS |
				GraphicsPipelineState_Resource_GS |
				GraphicsPipelineState_Resource_HS |
				GraphicsPipelineState_Resource_TS;
		}
		else {

			VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[cmd_];

			// Transfer dst Memory Barrier
			VkBufferMemoryBarrier bufferBarrier{};
			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.buffer = buffer.buffer;
			bufferBarrier.size = VK_WHOLE_SIZE;
			bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.srcAccessMask = 0u;

			VkPipelineStageFlags stages = 0u;

			switch (buffer_state)
			{
			case GPUBufferState_Vertex:
				bufferBarrier.srcAccessMask |= VK_ACCESS_INDEX_READ_BIT;
				stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
				break;
			case GPUBufferState_Index:
				bufferBarrier.srcAccessMask |= VK_ACCESS_INDEX_READ_BIT;
				stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
				break;
			case GPUBufferState_Constant:
				bufferBarrier.srcAccessMask |= VK_ACCESS_UNIFORM_READ_BIT;
				stages |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				break;
			case GPUBufferState_ShaderResource:
				bufferBarrier.srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
				stages |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				break;
			case GPUBufferState_UnorderedAccessView:
				bufferBarrier.srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
				// TODO stages |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				break;
			}

			vkCmdPipelineBarrier(cmd, stages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);

			// copy
			DynamicAllocation allocation = allocate_gpu(size, 1u, cmd_);
			
			if (allocation.data == NULL) {

				SV_LOG_ERROR("Can't allocate %u in the GPU to perform a buffer copy\n", size);
			}
			else {
				memcpy(allocation.data, pData, u64(size));
				graphics_vulkan_buffer_copy(cmd, allocation.buffer, buffer.buffer, VkDeviceSize(allocation.offset), VkDeviceSize(offset), VkDeviceSize(size));
			}

			// Memory Barrier

			std::swap(bufferBarrier.srcAccessMask, bufferBarrier.dstAccessMask);
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, stages, 0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);
		}
    }

    void graphics_vulkan_barrier(const GPUBarrier* barriers, u32 count, CommandList cmd_)
    {
		VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[cmd_];

		VkPipelineStageFlags srcStage = 0u;
		VkPipelineStageFlags dstStage = 0u;

		u32 memoryBarrierCount = 0u;
		u32 bufferBarrierCount = 0u;
		u32 imageBarrierCount = 0u;

		VkMemoryBarrier			memoryBarrier[GraphicsLimit_GPUBarrier];
		VkBufferMemoryBarrier	bufferBarrier[GraphicsLimit_GPUBarrier];
		VkImageMemoryBarrier	imageBarrier[GraphicsLimit_GPUBarrier];

		for (u32 i = 0; i < count; ++i) {

			const GPUBarrier& barrier = barriers[i];

			switch (barrier.type)
			{
			case BarrierType_Image:

				imageBarrier[imageBarrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageBarrier[imageBarrierCount].pNext = nullptr;
				imageBarrier[imageBarrierCount].srcAccessMask = graphics_vulkan_access_from_image_layout(barrier.image.old_layout);
				imageBarrier[imageBarrierCount].dstAccessMask = graphics_vulkan_access_from_image_layout(barrier.image.new_layout);
				imageBarrier[imageBarrierCount].oldLayout = graphics_vulkan_parse_image_layout(barrier.image.old_layout);
				imageBarrier[imageBarrierCount].newLayout = graphics_vulkan_parse_image_layout(barrier.image.new_layout);
				imageBarrier[imageBarrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier[imageBarrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier[imageBarrierCount].subresourceRange.levelCount = 1u;
				imageBarrier[imageBarrierCount].subresourceRange.baseArrayLayer = 0u;
				imageBarrier[imageBarrierCount].subresourceRange.baseMipLevel = 0u;

				Image_vk& image = *reinterpret_cast<Image_vk*>(barrier.image.image);

				imageBarrier[imageBarrierCount].image = image.image;
				imageBarrier[imageBarrierCount].subresourceRange.aspectMask = graphics_vulkan_aspect_from_image_layout(barrier.image.old_layout, image.info.format);
				imageBarrier[imageBarrierCount].subresourceRange.layerCount = image.layers;
		
				srcStage |= graphics_vulkan_stage_from_image_layout(barrier.image.old_layout);
				dstStage |= graphics_vulkan_stage_from_image_layout(barrier.image.new_layout);

				imageBarrierCount++;
				break;
			}

		}

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0u, memoryBarrierCount, memoryBarrier, bufferBarrierCount, bufferBarrier, imageBarrierCount, imageBarrier);
    }

    void graphics_vulkan_event_begin(const char* name, CommandList cmd)
    {
		VkDebugUtilsLabelEXT label = {};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = name;
		label.color[0] = 1.f;
		label.color[1] = 1.f;
		label.color[2] = 1.f;
		label.color[3] = 1.f;

		vkCmdBeginDebugUtilsLabelEXT(g_API->instance, g_API->frames[g_API->currentFrame].commandBuffers[cmd], &label);
    }
    void graphics_vulkan_event_mark(const char* name, CommandList cmd)
    {
		VkDebugUtilsLabelEXT label = {};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = name;
		label.color[0] = 1.f;
		label.color[1] = 1.f;
		label.color[2] = 1.f;
		label.color[3] = 1.f;

		vkCmdInsertDebugUtilsLabelEXT(g_API->instance, g_API->frames[g_API->currentFrame].commandBuffers[cmd], &label);
    }
    void graphics_vulkan_event_end(CommandList cmd)
    {
		vkCmdEndDebugUtilsLabelEXT(g_API->instance, g_API->frames[g_API->currentFrame].commandBuffers[cmd]);
    }

    //////////////////////////////////////////// API /////////////////////////////////////////////////

    bool graphics_vulkan_swapchain_create()
    {
		SwapChain_vk& swapChain = g_API->swapchain;
		auto& card = g_API->card;

		bool recreating = swapChain.swapChain != VK_NULL_HANDLE;

		// Create Surface
		if (!recreating)
		{
#if SV_PLATFORM_WINDOWS
			VkWin32SurfaceCreateInfoKHR create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			create_info.hinstance = GetModuleHandle(nullptr);
			create_info.flags = 0u;
			create_info.hwnd = reinterpret_cast<HWND>(window_handle());

			vkCheck(vkCreateWin32SurfaceKHR(g_API->instance, &create_info, nullptr, &swapChain.surface));
#elif SV_PLATFORM_ANDROID

			VkAndroidSurfaceCreateInfoKHR create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
			create_info.flags = 0u;
			create_info.window = (struct ANativeWindow*)window_handle();

			vkCheck(vkCreateAndroidSurfaceKHR(g_API->instance, &create_info, nullptr, &swapChain.surface));
#endif
		}

		// Check SwapChain Supported by this adapter
		{
			VkBool32 supported;
			vkCheck(vkGetPhysicalDeviceSurfaceSupportKHR(card.physicalDevice, card.familyIndex.graphics, swapChain.surface, &supported));
			if (!supported) {
				SV_LOG_ERROR("This adapter don't support vulkan swapChain\n");
				return false;
			}
		}

		// Get Support Details
		vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(card.physicalDevice, swapChain.surface, &swapChain.capabilities));
		{
			u32 count = 0;
			vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(card.physicalDevice, swapChain.surface, &count, nullptr));
			swapChain.presentModes.resize(count);
			vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(card.physicalDevice, swapChain.surface, &count, swapChain.presentModes.data()));

			VkSurfaceTransformFlagBitsKHR transform = swapChain.capabilities.currentTransform;

			SwapchainRotation rotation = SwapchainRotation_0;

			if (transform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
				rotation = SwapchainRotation_90;
			else if (transform == VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR)
				rotation = SwapchainRotation_180;
			else if (transform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
				rotation = SwapchainRotation_270;

			_graphics_swapchain_rotation_set(rotation);
		}
		{
			u32 count = 0;
			vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(card.physicalDevice, swapChain.surface, &count, nullptr));
			swapChain.formats.resize(count);
			vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(card.physicalDevice, swapChain.surface, &count, swapChain.formats.data()));
		}

		u32 width = SV_MAX(window_size().x, 1);
		u32 height = SV_MAX(window_size().y, 1);

		// Create SwapChain
		{
			// Choose image count
			u32 imageCount = swapChain.capabilities.minImageCount + 1u;
			if (imageCount > swapChain.capabilities.maxImageCount && swapChain.capabilities.maxImageCount > 0)
				imageCount = swapChain.capabilities.maxImageCount;

			// Choose format
			VkSurfaceFormatKHR format = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
			if (!swapChain.formats.empty()) format = swapChain.formats[0];

			for (u32 i = 0; i < swapChain.formats.size(); ++i) {
				VkSurfaceFormatKHR f = swapChain.formats[i];
				if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
					format = f;
					break;
				}
			}

			// Choose Swapchain Extent
			VkExtent2D extent;
			if (swapChain.capabilities.currentExtent.width != UINT32_MAX) extent = swapChain.capabilities.currentExtent;
			else {

				extent.width = width;
				extent.height = height;
				VkExtent2D min = swapChain.capabilities.minImageExtent;
				VkExtent2D max = swapChain.capabilities.maxImageExtent;

				if (extent.width < min.width) extent.width = min.width;
				else if (extent.width > max.width) extent.width = max.width;
				if (extent.height < min.height) extent.height = min.height;
				else if (extent.height > max.height) extent.height = max.height;
			}

			extent.width = SV_MAX(extent.width, 1);
			extent.height = SV_MAX(extent.height, 1);

			switch (graphics_swapchain_rotation())
			{
				case SwapchainRotation_90:
				case SwapchainRotation_270:
				{
					u32 aux = extent.width;
					extent.width = extent.height;
					extent.height = aux;
				}
				break;
			}

			VkSwapchainCreateInfoKHR create_info{};

			// Family Queue Indices
			auto fi = card.familyIndex;

			// TODO: if familyqueue not support that surface, create presentQueue with other index
			////////////// UNFINISHED ///////////////////////
			//u32 indices[] = {
			//	fi.graphics
			//};

			//if (fi.graphics == sc.familyIndexPresent) {
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			create_info.queueFamilyIndexCount = 0u;
			create_info.pQueueFamilyIndices = nullptr;
			//}
			//else {
			//	create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			//	create_info.queueFamilyIndexCount = 2u;
			//	create_info.pQueueFamilyIndices = indices;
			//}
			/////////////////////////

			// Choose Present Mode
			VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

			for (u32 i = 0; i < swapChain.presentModes.size(); ++i) {
				if (swapChain.presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
					//if (g_API->swapChain.presentModes[i] == VK_PRESENT_MODE_FIFO_KHR) {
					//if (g_API->swapChain.presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
					presentMode = swapChain.presentModes[i];
					break;
				}
			}

			create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			create_info.flags = 0u;
			create_info.surface = swapChain.surface;
			create_info.minImageCount = imageCount;
			create_info.imageFormat = format.format;
			create_info.imageColorSpace = format.colorSpace;
			create_info.imageExtent = extent;
			create_info.imageArrayLayers = 1u;
			create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			create_info.preTransform = swapChain.capabilities.currentTransform;
			create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			create_info.presentMode = presentMode;
			create_info.clipped = VK_TRUE;
			create_info.oldSwapchain = swapChain.swapChain;

			vkCheck(vkCreateSwapchainKHR(g_API->device, &create_info, nullptr, &swapChain.swapChain));

			swapChain.currentFormat = format.format;
			swapChain.currentColorSpace = format.colorSpace;
			swapChain.currentPresentMode = presentMode;
			swapChain.currentExtent = extent;
		}

		// Get Images Handles
		{
			std::vector<VkImage> images;
			u32 count = 0u;
			vkCheck(vkGetSwapchainImagesKHR(g_API->device, swapChain.swapChain, &count, nullptr));
			images.resize(count);
			vkCheck(vkGetSwapchainImagesKHR(g_API->device, swapChain.swapChain, &count, images.data()));

			swapChain.images.resize(images.size());
			for (u32 i = 0; i < swapChain.images.size(); ++i) {
				swapChain.images[i].image = images[i];
			}
		}

		// Create Views, IDs and SilverEngine format image
		{
			for (u32 i = 0; i < swapChain.images.size(); ++i) {
				VkImage image = swapChain.images[i].image;

				VkImageViewCreateInfo create_info{};
				create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				create_info.flags = 0u;
				create_info.image = image;
				create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
				create_info.format = swapChain.currentFormat;
				create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				create_info.subresourceRange.baseArrayLayer = 0u;
				create_info.subresourceRange.baseMipLevel = 0u;
				create_info.subresourceRange.layerCount = 1u;
				create_info.subresourceRange.levelCount = 1u;

				vkCheck(vkCreateImageView(g_API->device, &create_info, nullptr, &swapChain.images[i].view));	    }
		}

		// Change Images layout
		{
			VkCommandBuffer cmd;
			vkCheck(graphics_vulkan_singletimecmb_begin(&cmd));

			for (u32 i = 0; i < swapChain.images.size(); ++i) {

				VkImageMemoryBarrier imageBarrier{};
				imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
				imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
				imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.image = swapChain.images[i].image;
				imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBarrier.subresourceRange.layerCount = 1u;
				imageBarrier.subresourceRange.levelCount = 1u;

				vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0u, 0u, 0u, 0u, 0u, 1u, &imageBarrier);

			}

			graphics_vulkan_singletimecmb_end(cmd);
		}

		// Create Semaphores
		if (!recreating) {
			swapChain.semAcquireImage = graphics_vulkan_semaphore_create();
			swapChain.semPresent = graphics_vulkan_semaphore_create();
		}

		swapChain.imageFences.clear();
		swapChain.imageFences.resize(swapChain.images.size(), VK_NULL_HANDLE);

		return true;
    }

    bool graphics_vulkan_swapchain_destroy(bool resizing)
    {
		SwapChain_vk& swapChain = g_API->swapchain;
		
		for (u32 i = 0; i < swapChain.images.size(); ++i) {
			vkDestroyImageView(g_API->device, swapChain.images[i].view, nullptr);
		}
		swapChain.images.clear();

		if (!resizing) {
			vkDestroySemaphore(g_API->device, swapChain.semAcquireImage, nullptr);
			vkDestroySemaphore(g_API->device, swapChain.semPresent, nullptr);

			vkDestroySwapchainKHR(g_API->device, swapChain.swapChain, nullptr);
			vkDestroySurfaceKHR(g_API->instance, swapChain.surface, nullptr);
		}
		return true;
    }

    static void update_graphics_state(CommandList cmd_)
    {
		GraphicsState& state = graphics_state_get()->graphics[cmd_];
		
		// That means the state is not changed
		if (state.flags == 0u) return;

		RenderPass_vk& renderPass = *reinterpret_cast<RenderPass_vk*>(state.render_pass);

		VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[cmd_];

		bool updatePipeline = state.flags & (
				GraphicsPipelineState_Shader |
				GraphicsPipelineState_InputLayoutState |
				GraphicsPipelineState_BlendState |
				GraphicsPipelineState_DepthStencilState |
				GraphicsPipelineState_RasterizerState |
				GraphicsPipelineState_Topology
			);

		Shader_vk* vertex_shader = reinterpret_cast<Shader_vk*>(state.vertex_shader);
		Shader_vk* pixel_shader = reinterpret_cast<Shader_vk*>(state.pixel_shader);
		Shader_vk* geometry_shader = reinterpret_cast<Shader_vk*>(state.geometry_shader);

		// Bind Vertex Buffers
		if (state.flags & GraphicsPipelineState_VertexBuffer) {
			
			VkBuffer buffers[GraphicsLimit_VertexBuffer] = {};
			VkDeviceSize offsets[GraphicsLimit_VertexBuffer] = {};
			for (u32 i = 0; i < state.vertex_buffer_count; ++i) {
				if (state.vertex_buffers[i] == nullptr) {
					buffers[i] = VK_NULL_HANDLE;
					offsets[i] = 0u;
				}
				else {
					buffers[i] = reinterpret_cast<Buffer_vk*>(state.vertex_buffers[i])->buffer;
					offsets[i] = VkDeviceSize(state.vertex_buffer_offsets[i]);
				}
			}
			
			// TODO: strides??
			if (state.vertex_buffer_count)
				vkCmdBindVertexBuffers(cmd, 0u, state.vertex_buffer_count, buffers, offsets);
		}
		// Bind Index Buffer
		if (state.flags & GraphicsPipelineState_IndexBuffer && state.index_buffer != nullptr) {
			Buffer_vk& buffer = *reinterpret_cast<Buffer_vk*>(state.index_buffer);
			vkCmdBindIndexBuffer(cmd, buffer.buffer, state.index_buffer_offset, graphics_vulkan_parse_indextype(buffer.info.index_type));
		}

		// Bind RenderPass
		if (state.flags & GraphicsPipelineState_RenderPass && g_API->activeRenderPass[cmd_] != renderPass.renderPass) {
			updatePipeline = true;
		}

		// Bind Pipeline
		size_t pipelineHash = 0u;

		if (updatePipeline) {

			// Set active renderpass
			g_API->activeRenderPass[cmd_] = renderPass.renderPass;

			// Compute hash
			pipelineHash = graphics_vulkan_pipeline_compute_hash(state);

			// Find Pipeline
			VulkanPipeline* pipelinePtr = nullptr;
			{
				// Critical Section
				SV_LOCK_GUARD(g_API->pipeline_mutex, lock);
				auto it = g_API->pipelines.find(pipelineHash);
				if (it == g_API->pipelines.end()) {
					
					// Create New Pipeline Object
					VulkanPipeline& p = g_API->pipelines[pipelineHash];
		    
					graphics_vulkan_pipeline_create(p, vertex_shader, pixel_shader, geometry_shader);
					// TODO handle error
					pipelinePtr = &p;

				}
				else {
					pipelinePtr = &it->second;
				}
			}
			VulkanPipeline& pipeline = *pipelinePtr;

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_vulkan_pipeline_get(pipeline, state, pipelineHash));
		}

		// Bind Viewports
		if (state.flags & GraphicsPipelineState_Viewport) {
			
			VkViewport viewports[GraphicsLimit_Viewport];
			for (u32 i = 0; i < state.viewport_count; ++i) {
				const Viewport& vp = state.viewports[i];

				viewports[i].x			= vp.x;
				viewports[i].y			= vp.y;
				viewports[i].width		= vp.width;
				viewports[i].height		= vp.height;
				viewports[i].minDepth	= vp.min_depth;
				viewports[i].maxDepth	= vp.max_depth;
			}
			for (u32 i = state.viewport_count; i < GraphicsLimit_Viewport; ++i) {
				viewports[i].x = 0.f;
				viewports[i].y = 0.f;
				viewports[i].width = 1.f;
				viewports[i].height = 1.f;
				viewports[i].minDepth = 0.f;
				viewports[i].maxDepth = 1.f;
			}

			vkCmdSetViewport(cmd, 0u, GraphicsLimit_Viewport, viewports);

		}

		// Bind Scissors
		if (state.flags & GraphicsPipelineState_Scissor) {

			VkRect2D scissors[GraphicsLimit_Scissor];
			for (u32 i = 0; i < state.scissor_count; ++i) {
				const Scissor& sc = state.scissors[i];

				scissors[i].offset.x = sc.x;
				scissors[i].offset.y = sc.y;
				scissors[i].extent.width = sc.width;
				scissors[i].extent.height = sc.height;
			}
			for (u32 i = state.scissor_count; i < GraphicsLimit_Scissor; ++i) {
				scissors[i].offset.x = 0u;
				scissors[i].offset.y = 0u;
				scissors[i].extent.width = 1u;
				scissors[i].extent.height = 1u;
			}

			vkCmdSetScissor(cmd, 0u, GraphicsLimit_Scissor, scissors);

		}

		// Set stencil reference
		if (state.flags & GraphicsPipelineState_StencilRef) {
			vkCmdSetStencilReference(cmd, VK_STENCIL_FRONT_AND_BACK, state.stencil_reference);
		}

		// Set line width
		if (state.flags & GraphicsPipelineState_LineWidth) {
			vkCmdSetLineWidth(cmd, state.line_width);
		}

		// Update Descriptors
		if (state.flags & (GraphicsPipelineState_ConstantBuffer | GraphicsPipelineState_ShaderResource | GraphicsPipelineState_UnorderedAccessView | GraphicsPipelineState_Sampler)) {

			if (pipelineHash == 0u) {
				pipelineHash = graphics_vulkan_pipeline_compute_hash(state);
			}

			// TODO: This is safe??
			VulkanPipeline& pipeline = g_API->pipelines[pipelineHash];

			if (state.flags & GraphicsPipelineState_Resource_VS && state.vertex_shader != NULL) {

				pipeline.descriptorSets[ShaderType_Vertex] = update_descriptors(vertex_shader->layout, ShaderType_Vertex, state.flags & GraphicsPipelineState_ConstantBuffer, state.flags & GraphicsPipelineState_ShaderResource, state.flags & GraphicsPipelineState_UnorderedAccessView, state.flags & GraphicsPipelineState_Sampler, cmd_);
				
			}
			if (state.flags & GraphicsPipelineState_Resource_PS && state.pixel_shader != NULL) {

				pipeline.descriptorSets[ShaderType_Pixel] = update_descriptors(pixel_shader->layout, ShaderType_Pixel, state.flags & GraphicsPipelineState_ConstantBuffer, state.flags & GraphicsPipelineState_ShaderResource, state.flags & GraphicsPipelineState_UnorderedAccessView, state.flags & GraphicsPipelineState_Sampler, cmd_);

			}
			if (state.flags & GraphicsPipelineState_Resource_GS && state.geometry_shader != NULL) {

				pipeline.descriptorSets[ShaderType_Geometry] = update_descriptors(geometry_shader->layout, ShaderType_Geometry, state.flags & GraphicsPipelineState_ConstantBuffer, state.flags & GraphicsPipelineState_ShaderResource, state.flags & GraphicsPipelineState_UnorderedAccessView, state.flags & GraphicsPipelineState_Sampler, cmd_);

			}

			u32 offset = 0u;
			for (u32 i = 0; i < ShaderType_GraphicsCount; ++i) {
				if (pipeline.descriptorSets[i] == VK_NULL_HANDLE) {
					if (i == offset) {
						offset++;
						continue;
					}
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, offset, i - offset, &pipeline.descriptorSets[offset], 0u, nullptr);
					offset = i + 1u;
				}
			}

			if (offset != ShaderType_GraphicsCount) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, offset, ShaderType_GraphicsCount - offset, &pipeline.descriptorSets[offset], 0u, nullptr);
			}
		}

		state.flags = 0u;
    }

    static VkDescriptorSet update_descriptors(const ShaderDescriptorSetLayout& layout, ShaderType shader_type, bool constant_buffers, bool shader_resources, bool unordered_access_views, bool samplers, CommandList cmd_)
    {
		auto& state = *graphics_state_get();
		
		VkWriteDescriptorSet write_desc[GraphicsLimit_ConstantBuffer + GraphicsLimit_ShaderResource + GraphicsLimit_UnorderedAccessView + GraphicsLimit_Sampler];
		u32 write_count = 0u;

		VkDescriptorSet desc_set = allocate_descriptors_sets(g_API->GetFrame().descPool[cmd_], layout);

		for (ShaderResourceBinding binding : layout.bindings) {

			switch (binding.descriptor_type) {
				
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				if (!constant_buffers) continue;
				else {
				
					Buffer_vk* buffer = NULL;

					if (shader_type == ShaderType_Compute) {
						buffer = reinterpret_cast<Buffer_vk*>(state.compute[cmd_].constant_buffers[binding.userBinding]);
					}
					else {
						buffer = reinterpret_cast<Buffer_vk*>(state.graphics[cmd_].constant_buffers[shader_type][binding.userBinding]);
					}

					if (buffer == NULL) continue;
					write_desc[write_count].pImageInfo = NULL;
					write_desc[write_count].pTexelBufferView = NULL;
						
					if (buffer->info.usage == ResourceUsage_Dynamic) {

						buffer->buffer_info.buffer = buffer->dynamic_allocation[cmd_].buffer;
						buffer->buffer_info.offset = buffer->dynamic_allocation[cmd_].offset;
						buffer->buffer_info.range = buffer->info.size;
					}
				
					write_desc[write_count].pBufferInfo = &buffer->buffer_info;
				}
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				if (!shader_resources) continue;
				else {

					Image_vk* image = NULL;

					if (shader_type == ShaderType_Compute) {
						image = reinterpret_cast<Image_vk*>(state.compute[cmd_].shader_resources[binding.userBinding]);
					}
					else {
						image = reinterpret_cast<Image_vk*>(state.graphics[cmd_].shader_resources[shader_type][binding.userBinding]);
					}

					if (image == nullptr) continue;
					
					write_desc[write_count].pImageInfo = &image->shader_resource_view;
					write_desc[write_count].pBufferInfo = nullptr;
					write_desc[write_count].pTexelBufferView = nullptr;
				}
				break;
				
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				if (!unordered_access_views) continue;
				else {

					Image_vk* image = NULL;

					if (shader_type == ShaderType_Compute) {
						image = reinterpret_cast<Image_vk*>(state.compute[cmd_].unordered_access_views[binding.userBinding]);
					}
					else {
						image = reinterpret_cast<Image_vk*>(state.graphics[cmd_].unordered_access_views[shader_type][binding.userBinding]);
					}

					if (image == nullptr) continue;
					write_desc[write_count].pImageInfo = &image->unordered_access_view;
					write_desc[write_count].pBufferInfo = nullptr;
					write_desc[write_count].pTexelBufferView = nullptr;
				}
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				if (!shader_resources) continue;
				else {

					Buffer_vk* buffer = NULL;

					if (shader_type == ShaderType_Compute) {
						buffer = reinterpret_cast<Buffer_vk*>(state.compute[cmd_].shader_resources[binding.userBinding]);
					}
					else {
						buffer = reinterpret_cast<Buffer_vk*>(state.graphics[cmd_].shader_resources[shader_type][binding.userBinding]);
					}

					if (buffer == NULL) continue;
					
					write_desc[write_count].pImageInfo = NULL;
					write_desc[write_count].pTexelBufferView = NULL;
					write_desc[write_count].pBufferInfo = &buffer->buffer_info;
				}
				break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				if (!shader_resources) continue;
				else {

					Buffer_vk* buffer = NULL;

					if (shader_type == ShaderType_Compute) {
						buffer = reinterpret_cast<Buffer_vk*>(state.compute[cmd_].shader_resources[binding.userBinding]);
					}
					else {
						buffer = reinterpret_cast<Buffer_vk*>(state.graphics[cmd_].shader_resources[shader_type][binding.userBinding]);
					}

					if (buffer == NULL) continue;
					
					write_desc[write_count].pImageInfo = NULL;
					write_desc[write_count].pTexelBufferView = &buffer->srv_texel_buffer_view;
					write_desc[write_count].pBufferInfo = NULL;
				}
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				if (!unordered_access_views) continue;
				else {

					Buffer_vk* buffer = NULL;

					if (shader_type == ShaderType_Compute) {
						buffer = reinterpret_cast<Buffer_vk*>(state.compute[cmd_].unordered_access_views[binding.userBinding]);
					}
					else {
						buffer = reinterpret_cast<Buffer_vk*>(state.graphics[cmd_].unordered_access_views[shader_type][binding.userBinding]);
					}

					if (buffer == NULL) continue;
					
					write_desc[write_count].pImageInfo = NULL;
					write_desc[write_count].pTexelBufferView = &buffer->uav_texel_buffer_view;
					write_desc[write_count].pBufferInfo = NULL;
				}
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLER:
				if (!samplers) continue;
				else {
				
					Sampler_vk* sampler = NULL;

					if (shader_type == ShaderType_Compute) {
						sampler = reinterpret_cast<Sampler_vk*>(state.compute[cmd_].samplers[binding.userBinding]);
					}
					else {
						sampler = reinterpret_cast<Sampler_vk*>(state.graphics[cmd_].samplers[shader_type][binding.userBinding]);
					}
					
					if (sampler == nullptr) continue;
					write_desc[write_count].pImageInfo = &sampler->image_info;
					write_desc[write_count].pBufferInfo = nullptr;
					write_desc[write_count].pTexelBufferView = nullptr;
				}
				break;

			default:
				assert(0);
				return desc_set;
			
				
			}

			write_desc[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_desc[write_count].pNext = nullptr;
			write_desc[write_count].dstSet = desc_set;
			write_desc[write_count].dstBinding = binding.vulkanBinding;
			write_desc[write_count].dstArrayElement = 0u;
			write_desc[write_count].descriptorCount = 1u;
			write_desc[write_count].descriptorType = binding.descriptor_type;

			++write_count;
		}

		vkUpdateDescriptorSets(g_API->device, write_count, write_desc, 0u, nullptr);
		return desc_set;
    }

	static void update_compute_state(CommandList cmd_)
	{
		ComputeState& state = graphics_state_get()->compute[cmd_];

		// That means the state is not changed
		//TODO: if (state.flags == 0u) return;

		VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[cmd_];

		Shader_vk* shader = reinterpret_cast<Shader_vk*>(state.compute_shader);

		// TODO
		bool update_pipeline = true;

		if (update_pipeline) {

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shader->compute.pipeline);
		}

		if (state.update_resources) {

			state.update_resources = false;

			VkDescriptorSet desc_set = update_descriptors(shader->layout, ShaderType_Compute, true, true, true, true, cmd_);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shader->compute.pipeline_layout, 0u, 1u, &desc_set, 0u, nullptr);
		}
	}
	
    VkResult graphics_vulkan_singletimecmb_begin(VkCommandBuffer* pCmd)
    {
		Frame& frame = g_API->frames[g_API->currentFrame];

		// Allocate CommandBuffer
		{
			
			VkCommandBufferAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.commandPool = frame.transientCommandPool;
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc_info.commandBufferCount = 1u;

			VkResult res = vkAllocateCommandBuffers(g_API->device, &alloc_info, pCmd);

			if (res != VK_SUCCESS) {
				
				SV_LOG_ERROR("Can't allocate SingleTime CommandBuffer\n");
				return res;
			}
		}

		// Begin CommandBuffer
		{
			VkCommandBufferBeginInfo begin_info{};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			begin_info.pInheritanceInfo = nullptr;

			VkResult res = vkBeginCommandBuffer(*pCmd, &begin_info);
			if (res != VK_SUCCESS) {
				SV_LOG_ERROR("Can't begin SingleTime CommandBuffer\n");
				vkFreeCommandBuffers(g_API->device, g_API->frames[g_API->currentFrame].transientCommandPool, 1u, pCmd);
				return res;
			}
		}

		return VK_SUCCESS;
    }

    VkResult graphics_vulkan_singletimecmb_end(VkCommandBuffer cmd)
    {
		Frame& frame = g_API->frames[g_API->currentFrame];

		VkResult res = vkEndCommandBuffer(cmd);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 0u;
		submit_info.commandBufferCount = 1u;
		submit_info.pCommandBuffers = &cmd;
		submit_info.signalSemaphoreCount = 0u;

		vkExt(vkQueueSubmit(g_API->queueGraphics, 1u, &submit_info, VK_NULL_HANDLE));
		vkQueueWaitIdle(g_API->queueGraphics);

		// Free CommandBuffer
		if (res == VK_SUCCESS) {

			vkFreeCommandBuffers(g_API->device, frame.transientCommandPool, 1u, &cmd);
		}

		return res;
    }

    void graphics_vulkan_buffer_copy(VkCommandBuffer cmd, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
    {
		VkBufferCopy copy_info{};
		copy_info.dstOffset = dstOffset;
		copy_info.srcOffset = srcOffset;
		copy_info.size = size;
		vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1u, &copy_info);
    }

    VkResult graphics_vulkan_imageview_create(VkImage image, VkFormat format, VkImageViewType viewType, VkImageAspectFlags aspectFlags, u32 layerCount, VkImageView& view)
    {
		VkImageViewCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.flags = 0u;
		create_info.image = image;
		create_info.viewType = viewType;
		create_info.format = format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = aspectFlags;
		create_info.subresourceRange.baseMipLevel = 0u;
		create_info.subresourceRange.levelCount = 1u;
		create_info.subresourceRange.baseArrayLayer = 0u;
		create_info.subresourceRange.layerCount = layerCount;

		return vkCreateImageView(g_API->device, &create_info, nullptr, &view);
    }

    VkSemaphore graphics_vulkan_semaphore_create()
    {
		VkSemaphore sem = VK_NULL_HANDLE;
		VkSemaphoreCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(g_API->device, &create_info, nullptr, &sem) != VK_SUCCESS)
			return VK_NULL_HANDLE;

		return sem;
    }

    VkFence graphics_vulkan_fence_create(bool sign)
    {
		VkFence fence = VK_NULL_HANDLE;
		VkFenceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		createInfo.flags = sign ? VK_FENCE_CREATE_SIGNALED_BIT : 0u;

		if (vkCreateFence(g_API->device, &createInfo, nullptr, &fence) != VK_SUCCESS)
			return VK_NULL_HANDLE;

		return fence;
    }

    void graphics_vulkan_acquire_image()
    {
		SwapChain_vk& sc = g_API->swapchain;
		vkAcquireNextImageKHR(g_API->device, sc.swapChain, UINT64_MAX, sc.semAcquireImage, VK_NULL_HANDLE, &sc.imageIndex);

		PipelineState& state = *graphics_state_get();
	
		if (state.present_image) {

			// TODO: Specific blit
			SwapChain_vk::Image& sc = g_API->swapchain.images[g_API->swapchain.imageIndex];
			Image_vk temp;
			temp.image = sc.image;
			temp.layers = 1u;
			temp.info.format = graphics_vulkan_parse_format(g_API->swapchain.currentFormat);
			temp.info.type = GPUImageType_RenderTarget;
			temp.info.usage = ResourceUsage_Static;
			temp.info.cpu_access = CPUAccess_None;
			temp.info.width = g_API->swapchain.currentExtent.width;
			temp.info.height = g_API->swapchain.currentExtent.height;

			Image_vk& present = *reinterpret_cast<Image_vk*>(state.present_image);
	    
			GPUImageBlit blit;
			blit.src_region.offset0 = { 0, 0, 0 };
			blit.src_region.offset1 = { (i32)present.info.width, (i32)present.info.height, 1 };
			blit.dst_region.offset1 = { (i32)temp.info.width, 0, 1 };
			blit.dst_region.offset0 = { 0, (i32)temp.info.height, 0 };

			graphics_image_blit(state.present_image, (GPUImage*)&temp, state.present_image_layout, GPUImageLayout_Present, 1u, &blit, SamplerFilter_Linear, graphics_commandlist_last());
		}
    }

    void graphics_vulkan_submit_commandbuffers()
    {
		Frame& frame = g_API->frames[g_API->currentFrame];

		// End CommandBuffers & RenderPasses
		for (u32 i = 0; i < g_API->activeCMDCount; ++i) {

			VkCommandBuffer cmd = g_API->frames[g_API->currentFrame].commandBuffers[i];
			vkAssert(vkEndCommandBuffer(cmd));
		}

		// CPU Sync
		SwapChain_vk* sc = &g_API->swapchain;
	
		if (sc->imageFences[sc->imageIndex] != VK_NULL_HANDLE) {
			vkAssert(vkWaitForFences(g_API->device, 1u, &sc->imageFences[sc->imageIndex], VK_TRUE, UINT64_MAX));
		}
		sc->imageFences[sc->imageIndex] = frame.fence;

		vkAssert(vkResetFences(g_API->device, 1u, &frame.fence));
		
		VkPipelineStageFlags waitDstStage[] = {
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
		};

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1u;
		submit_info.pWaitSemaphores = &sc->semAcquireImage;
		submit_info.pWaitDstStageMask = waitDstStage;
		submit_info.commandBufferCount = g_API->activeCMDCount;
		submit_info.pCommandBuffers = frame.commandBuffers;
		submit_info.signalSemaphoreCount = 1u;
		submit_info.pSignalSemaphores = &sc->semPresent;

		g_API->activeCMDCount = 0u;

		vkAssert(vkQueueSubmit(g_API->queueGraphics, 1u, &submit_info, frame.fence));
    }

    void graphics_vulkan_present()
    {
		SwapChain_vk& sc = g_API->swapchain;
		
		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1u;
		present_info.pWaitSemaphores = &sc.semPresent;
		present_info.swapchainCount = 1u;
		present_info.pSwapchains = &sc.swapChain;
		present_info.pImageIndices = &sc.imageIndex;
		present_info.pResults = nullptr;

		vkQueuePresentKHR(g_API->queueGraphics, &present_info);

		g_API->currentFrame++;
		if (g_API->currentFrame == g_API->frameCount) g_API->currentFrame = 0u;
    }

    bool graphics_vulkan_buffer_create(Buffer_vk& buffer, const GPUBufferDesc& desc)
    {
		VkBufferUsageFlags bufferUsage = 0u;		

		// Special case: It uses dynamic memory from an allocator
		if (desc.usage == ResourceUsage_Dynamic && buffer.info.buffer_type & GPUBufferType_Constant)
		{
			return true;
		}

		// Usage Flags
		if (desc.buffer_type & GPUBufferType_Vertex) {
			bufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		}
		if (desc.buffer_type & GPUBufferType_Index) {
			bufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		}
		if (desc.buffer_type & GPUBufferType_Constant) {
			bufferUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		if (desc.buffer_type & GPUBufferType_ShaderResource) {

			if (desc.format == Format_Unknown) {
				bufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			}
			else bufferUsage |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
		}
		if (desc.buffer_type & GPUBufferType_UnorderedAccessView) {

			if (desc.format == Format_Unknown) {
				bufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			}
			else bufferUsage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
		}

		bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

		// Create Buffer
		{
			VkBufferCreateInfo buffer_info{};
			buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_info.flags = 0u;
			buffer_info.usage = bufferUsage;
			buffer_info.size = desc.size;

			buffer_info.pQueueFamilyIndices = nullptr;
			buffer_info.queueFamilyIndexCount = 0u;
			buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo alloc_info{};
			alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			vkCheck(vmaCreateBuffer(g_API->allocator, &buffer_info, &alloc_info, &buffer.buffer, &buffer.allocation, nullptr));
		}

		// Set data
		if (desc.data) {
			
			SV_LOCK_GUARD(g_API->frames[g_API->currentFrame].transient_command_pool_mutex, lock);

			VkCommandBuffer cmd;
			vkCheck(graphics_vulkan_singletimecmb_begin(&cmd));

			StagingBuffer staging_buffer;

			vkCheck(create_stagingbuffer(staging_buffer, desc.size));
			memcpy(staging_buffer.data, desc.data, desc.size);

			VkBufferMemoryBarrier bufferBarrier{};

			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = 0;
			bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = buffer.buffer;
			bufferBarrier.offset = 0u;
			bufferBarrier.size = VK_WHOLE_SIZE;

			vkCmdPipelineBarrier(cmd,
								 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0u,
								 0u,
								 nullptr,
								 1u,
								 &bufferBarrier,
								 0u,
								 nullptr);

			graphics_vulkan_buffer_copy(cmd, staging_buffer.buffer, buffer.buffer, 0u, 0u, desc.size);

			bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferBarrier.dstAccessMask = 0u;

			if (desc.buffer_type & GPUBufferType_Vertex) {
				bufferBarrier.dstAccessMask |= VK_ACCESS_INDEX_READ_BIT;
			}
			if (desc.buffer_type & GPUBufferType_Index) {
				bufferBarrier.dstAccessMask |= VK_ACCESS_INDEX_READ_BIT;
			}
			if (desc.buffer_type & GPUBufferType_Constant) {
				bufferBarrier.dstAccessMask |= VK_ACCESS_UNIFORM_READ_BIT;
			}
			if (desc.buffer_type & GPUBufferType_ShaderResource) {
				bufferBarrier.dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
			}
			if (desc.buffer_type & GPUBufferType_UnorderedAccessView) {
				bufferBarrier.dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
			}

			vkCmdPipelineBarrier(cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
								 0u,
								 0u,
								 nullptr,
								 1u,
								 &bufferBarrier,
								 0u,
								 nullptr);

			vkCheck(graphics_vulkan_singletimecmb_end(cmd));

			vkCheck(destroy_stagingbuffer(staging_buffer));
		}

		// Buffer View
		{
			buffer.srv_texel_buffer_view = VK_NULL_HANDLE;
			buffer.uav_texel_buffer_view = VK_NULL_HANDLE;
			
			if (desc.buffer_type & GPUBufferType_ShaderResource) {

				VkBufferViewCreateInfo view_info = {};
				view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
				view_info.buffer = buffer.buffer;
				view_info.format = graphics_vulkan_parse_format(desc.format);
				view_info.offset = 0u;
				view_info.range = VK_WHOLE_SIZE;

				vkCheck(vkCreateBufferView(g_API->device, &view_info, NULL, &buffer.srv_texel_buffer_view));
			}

			if (desc.buffer_type & GPUBufferType_UnorderedAccessView) {

				VkBufferViewCreateInfo view_info = {};
				view_info.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
				view_info.buffer = buffer.buffer;
				view_info.format = graphics_vulkan_parse_format(desc.format);
				view_info.offset = 0u;
				view_info.range = VK_WHOLE_SIZE;

				vkCheck(vkCreateBufferView(g_API->device, &view_info, NULL, &buffer.uav_texel_buffer_view));
			}
		}

		// Desc
		buffer.buffer_info.buffer = buffer.buffer;
		buffer.buffer_info.offset = 0u;
		buffer.buffer_info.range = desc.size;

		return true;
    }

    bool graphics_vulkan_image_create(Image_vk& image, const GPUImageDesc& desc)
    {
		VkImageType imageType = VK_IMAGE_TYPE_2D;
		VkFormat format = graphics_vulkan_parse_format(desc.format);
		VkImageUsageFlags imageUsage = 0u;
		VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
		u32 image_flags = 0u;
		
		{
			if (desc.type & GPUImageType_RenderTarget) {
				imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			}
			if (desc.type & GPUImageType_ShaderResource) {
				imageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
			}
			if (desc.type & GPUImageType_UnorderedAccessView) {
				imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
			}
			if (desc.type & GPUImageType_DepthStencil) {
				imageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			}
			if (desc.type & GPUImageType_CubeMap) {
				image_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
				image.layers = 6u;
				view_type = VK_IMAGE_VIEW_TYPE_CUBE;
			}
		}

		// Create Image
		{
			VkImageCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			create_info.flags = image_flags;
			create_info.imageType = imageType;
			create_info.format = format;
			create_info.extent.width = desc.width;
			create_info.extent.height = desc.height;
			create_info.extent.depth = 1u;
			create_info.arrayLayers = image.layers;
			create_info.samples = VK_SAMPLE_COUNT_1_BIT;
			create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
			create_info.usage = imageUsage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			create_info.queueFamilyIndexCount = 0u;
			create_info.pQueueFamilyIndices = nullptr;
			create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			create_info.mipLevels = 1u;

			VmaAllocationCreateInfo alloc_info{};
			alloc_info.requiredFlags = VMA_MEMORY_USAGE_GPU_ONLY;
			
			vkCheck(vmaCreateImage(g_API->allocator, &create_info, &alloc_info, &image.image, &image.allocation, nullptr));
		}

		// Set Data
		VkCommandBuffer cmd;

		vkCheck(graphics_vulkan_singletimecmb_begin(&cmd));
		
		if (desc.data) {

			VkImageAspectFlags aspect = graphics_vulkan_aspect_from_image_layout(desc.layout, desc.format);

			// Memory barrier to set the image transfer dst layout
			VkImageMemoryBarrier memBarrier{};
			memBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			memBarrier.srcAccessMask = graphics_vulkan_access_from_image_layout(GPUImageLayout_Undefined);
			memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			memBarrier.oldLayout = graphics_vulkan_parse_image_layout(GPUImageLayout_Undefined);
			memBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			memBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			memBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			memBarrier.image = image.image;
			memBarrier.subresourceRange.aspectMask = aspect;
			memBarrier.subresourceRange.layerCount = image.layers;
			memBarrier.subresourceRange.levelCount = 1u;

			vkCmdPipelineBarrier(cmd,
								 graphics_vulkan_stage_from_image_layout(GPUImageLayout_Undefined),
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 0u,
								 0u,
								 nullptr,
								 0u,
								 nullptr,
								 1u,
								 &memBarrier);

			StagingBuffer staging_buffer;

			// Create staging buffer and copy desc.pData
			if (desc.type & GPUImageType_CubeMap) {

				create_stagingbuffer(staging_buffer, desc.size * 6u);

				u8** images = (u8 * *)desc.data;

				foreach(i, 6u) {
					
					u32 k = i;

					if (k == 0u) k = 4u;
					else if (k == 1u) k = 5u;
					else if (k == 4u) k = 0u;
					else if (k == 5u) k = 1u;

					memcpy((u8*)staging_buffer.data + desc.size * k, images[i], desc.size);
				}

				foreach(i, 6u) {

					// Copy buffer to image
					VkBufferImageCopy copy_info{};
					copy_info.bufferOffset = desc.size * i;
					copy_info.bufferRowLength = 0u;
					copy_info.bufferImageHeight = 0u;
					copy_info.imageSubresource.aspectMask = aspect;
					copy_info.imageSubresource.baseArrayLayer = i;
					copy_info.imageSubresource.layerCount = 1u;
					copy_info.imageSubresource.mipLevel = 0u;
					copy_info.imageOffset = { 0, 0, 0 };
					copy_info.imageExtent = { desc.width, desc.height, 1u };

					vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy_info);
				}
			}
			else {

				create_stagingbuffer(staging_buffer, desc.size);
				memcpy(staging_buffer.data, desc.data, desc.size);

				// Copy buffer to image
				VkBufferImageCopy copy_info{};
				copy_info.bufferOffset = 0u;
				copy_info.bufferRowLength = 0u;
				copy_info.bufferImageHeight = 0u;
				copy_info.imageSubresource.aspectMask = aspect;
				copy_info.imageSubresource.baseArrayLayer = 0u;
				copy_info.imageSubresource.layerCount = 1u;
				copy_info.imageSubresource.mipLevel = 0u;
				copy_info.imageOffset = { 0, 0, 0 };
				copy_info.imageExtent = { desc.width, desc.height, 1u };

				vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy_info);
			}
			
			// Set the layout to desc.layout
			memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			memBarrier.dstAccessMask = graphics_vulkan_access_from_image_layout(desc.layout);
			memBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			memBarrier.newLayout = graphics_vulkan_parse_image_layout(desc.layout);

			vkCmdPipelineBarrier(cmd,
								 VK_PIPELINE_STAGE_TRANSFER_BIT,
								 graphics_vulkan_stage_from_image_layout(desc.layout),
								 0u,
								 0u,
								 nullptr,
								 0u,
								 nullptr,
								 1u,
								 &memBarrier);

			vkCheck(graphics_vulkan_singletimecmb_end(cmd));

			vkCheck(destroy_stagingbuffer(staging_buffer));
		}
		else {

			VkImageMemoryBarrier memBarrier{};
			memBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			memBarrier.srcAccessMask = graphics_vulkan_access_from_image_layout(GPUImageLayout_Undefined);
			memBarrier.dstAccessMask = graphics_vulkan_access_from_image_layout(desc.layout);
			memBarrier.oldLayout = graphics_vulkan_parse_image_layout(GPUImageLayout_Undefined);
			memBarrier.newLayout = graphics_vulkan_parse_image_layout(desc.layout);
			memBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			memBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			memBarrier.image = image.image;
			memBarrier.subresourceRange.aspectMask = graphics_vulkan_aspect_from_image_layout(desc.layout, desc.format);
			memBarrier.subresourceRange.layerCount = image.layers;
			memBarrier.subresourceRange.levelCount = 1u;

			vkCmdPipelineBarrier(cmd,
								 graphics_vulkan_stage_from_image_layout(GPUImageLayout_Undefined),
								 graphics_vulkan_stage_from_image_layout(desc.layout),
								 0u,
								 0u,
								 nullptr,
								 0u,
								 nullptr,
								 1u,
								 &memBarrier);

			vkCheck(graphics_vulkan_singletimecmb_end(cmd));
		}

		// TODO: MipMapping

		// Create Render Target View
		if (desc.type & GPUImageType_RenderTarget) {
			vkCheck(graphics_vulkan_imageview_create(image.image, format, view_type, VK_IMAGE_ASPECT_COLOR_BIT, image.layers, image.render_target_view));
		}
		// Create Depth Stencil View
		if (desc.type & GPUImageType_DepthStencil) {
			VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			
			if (graphics_format_has_stencil(desc.format)) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

			vkCheck(graphics_vulkan_imageview_create(image.image, format, view_type, aspect, image.layers, image.depth_stencil_view));
		}
		// Create Shader Resource View
		if (desc.type & GPUImageType_ShaderResource) {

			if (desc.type & GPUImageType_DepthStencil) {

				vkCheck(graphics_vulkan_imageview_create(image.image, format, view_type, VK_IMAGE_ASPECT_DEPTH_BIT, image.layers, image.shader_resource_view.imageView));
				image.shader_resource_view.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else {

				vkCheck(graphics_vulkan_imageview_create(image.image, format, view_type, VK_IMAGE_ASPECT_COLOR_BIT, image.layers, image.shader_resource_view.imageView));
				image.shader_resource_view.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			image.shader_resource_view.sampler = VK_NULL_HANDLE;
		}

		// Create Unordered Access View
		if (desc.type & GPUImageType_UnorderedAccessView) {

			if (desc.type & GPUImageType_DepthStencil) {

				// TODO
				/*vkCheck(graphics_vulkan_imageview_create(image.image, format, view_type, VK_IMAGE_ASPECT_DEPTH_BIT, image.layers, image.shaderResouceView));

				// Set image info
				image.image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				image.image_info.sampler = VK_NULL_HANDLE;
				image.image_info.imageView = image.shaderResouceView;*/
			}
			else {

				vkCheck(graphics_vulkan_imageview_create(image.image, format, view_type, VK_IMAGE_ASPECT_COLOR_BIT, image.layers, image.unordered_access_view.imageView));
				image.unordered_access_view.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			}

			image.unordered_access_view.sampler = VK_NULL_HANDLE;
		}

		// Set ID
		image.ID = g_API->GetID();

		return true;
    }

    bool graphics_vulkan_sampler_create(Sampler_vk& sampler, const SamplerDesc& desc)
    {
		VkSamplerCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		create_info.minFilter = graphics_vulkan_parse_filter(desc.min_filter);
		create_info.magFilter = graphics_vulkan_parse_filter(desc.mag_filter);
		create_info.addressModeU = graphics_vulkan_parse_addressmode(desc.address_mode_u);
		create_info.addressModeV = graphics_vulkan_parse_addressmode(desc.address_mode_v);
		create_info.addressModeW = graphics_vulkan_parse_addressmode(desc.address_mode_w);
		create_info.compareEnable = VK_FALSE;
		create_info.compareOp = VK_COMPARE_OP_NEVER;
		create_info.unnormalizedCoordinates = VK_FALSE;

		// TODO: Border Color
		create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		// TODO: MipMapping
		create_info.mipLodBias;
		create_info.mipmapMode;

		create_info.anisotropyEnable;
		create_info.maxAnisotropy;

		create_info.minLod;
		create_info.maxLod;

		vkCheck(vkCreateSampler(g_API->device, &create_info, nullptr, &sampler.sampler));

		sampler.image_info.sampler = sampler.sampler;
		sampler.image_info.imageView = VK_NULL_HANDLE;
		sampler.image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		return true;
    }

    bool graphics_vulkan_shader_create(Shader_vk& shader, const ShaderDesc& desc)
    {
		// Create ShaderModule
		{
			VkShaderModuleCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			create_info.flags;
			create_info.codeSize = desc.bin_data_size;
			create_info.pCode = reinterpret_cast<const u32*>(desc.bin_data);

			vkCheck(vkCreateShaderModule(g_API->device, &create_info, nullptr, &shader.module));
		}

		// Semantic Names
		foreach(i, desc.semantic_name_count)
		{
			auto& sn = desc.semantic_names[i];
			shader.semanticNames[sn.name] = sn.location;
		}

		std::vector<VkDescriptorSetLayoutBinding> bindings;

		foreach(i, VulkanDescriptorType_MaxEnum) {
			shader.layout.count[i] = 0u;
		}

		foreach(i, desc.resource_count)
		{
			const ShaderResource* res = desc.resources + i;

			bindings.emplace_back();
			VkDescriptorSetLayoutBinding& binding = bindings.back();
			binding.binding = res->binding;
			binding.descriptorCount = 1u;
			binding.stageFlags = graphics_vulkan_parse_shadertype(desc.type);
			binding.pImmutableSamplers = nullptr;

			switch (res->type)
			{
			case ShaderResourceType_ConstantBuffer:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				shader.layout.count[VulkanDescriptorType_UniformBuffer]++;
				break;
			case ShaderResourceType_Image:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				shader.layout.count[VulkanDescriptorType_SampledImage]++;
				break;
			case ShaderResourceType_TexelBuffer:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				shader.layout.count[VulkanDescriptorType_UniformTexelBuffer]++;
				break;
			case ShaderResourceType_StorageBuffer:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				shader.layout.count[VulkanDescriptorType_StorageBuffer]++;
				break;
			case ShaderResourceType_UAImage:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				shader.layout.count[VulkanDescriptorType_StorageImage]++;
				break;
			case ShaderResourceType_UATexelBuffer:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
				shader.layout.count[VulkanDescriptorType_StorageTexelBuffer]++;
				break;
			case ShaderResourceType_Sampler:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				shader.layout.count[VulkanDescriptorType_Sampler]++;
				break;
			}
		}

		// Calculate user bindings values
		for (u32 i = 0; i < bindings.size(); ++i) {
			
			const VkDescriptorSetLayoutBinding& binding = bindings[i];
			
			ShaderResourceBinding& srb = shader.layout.bindings.emplace_back();
			
			srb.descriptor_type = binding.descriptorType;
			srb.vulkanBinding = binding.binding;
			srb.userBinding = binding.binding;

			switch (binding.descriptorType)
			{

			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				srb.userBinding -= 0u;
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
				srb.userBinding -= GraphicsLimit_ConstantBuffer;
				break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				srb.userBinding -= GraphicsLimit_ConstantBuffer + GraphicsLimit_ShaderResource;
				break;

			case VK_DESCRIPTOR_TYPE_SAMPLER:
				srb.userBinding -= GraphicsLimit_ConstantBuffer + GraphicsLimit_ShaderResource + GraphicsLimit_UnorderedAccessView;
				break;

			default:
				assert(0);
			}
		}

		// Create set layout
		{
			VkDescriptorSetLayoutCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			create_info.bindingCount = u32(bindings.size());
			create_info.pBindings = bindings.data();

			vkCheck(vkCreateDescriptorSetLayout(g_API->device, &create_info, nullptr, &shader.layout.setLayout));
		}

		// ID
		shader.ID = g_API->GetID();

		// Compute pipeline inside compute shader
		if (desc.type == ShaderType_Compute) {
			VkComputePipelineCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			info.pNext = NULL;
			info.flags = 0u;
			info.basePipelineHandle = VK_NULL_HANDLE;
			info.basePipelineIndex = 0;

			info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			info.stage.flags = 0u;
			info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			info.stage.module = shader.module;
			info.stage.pName = "main";
			info.stage.pSpecializationInfo = NULL;

			VkPipelineLayoutCreateInfo layout_info{};
			layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			layout_info.setLayoutCount = 1u;
			layout_info.pSetLayouts = &shader.layout.setLayout;
			layout_info.pushConstantRangeCount = 0u;

			vkCheck(vkCreatePipelineLayout(g_API->device, &layout_info, nullptr, &shader.compute.pipeline_layout));
			
			info.layout = shader.compute.pipeline_layout;

			vkCheck(vkCreateComputePipelines(g_API->device, VK_NULL_HANDLE, 1, &info, NULL, &shader.compute.pipeline));
		}

		return true;
    }

    bool graphics_vulkan_renderpass_create(RenderPass_vk& renderPass, const RenderPassDesc& desc)
    {
		VkAttachmentDescription attachments[GraphicsLimit_Attachment];

		VkAttachmentReference colorAttachments[GraphicsLimit_AttachmentRT];
		VkAttachmentReference depthStencilAttachment;

		VkSubpassDescription subpass_info{};

		bool hasDepthStencil = false;
		u32 colorIt = 0u;

		renderPass.mutex = mutex_create();

		for (u32 i = 0; i < desc.attachment_count; ++i) {
			const AttachmentDesc& attDesc = desc.attachments[i];

			attachments[i] = {};

			// Reference
			switch (attDesc.type)
			{
			case AttachmentType_RenderTarget:
				colorAttachments[colorIt].layout = graphics_vulkan_parse_image_layout(attDesc.layout);
				colorAttachments[colorIt].attachment = i;
				colorIt++;
				break;
			case AttachmentType_DepthStencil:
				if (hasDepthStencil) return false;

				depthStencilAttachment.attachment = i;
				depthStencilAttachment.layout = graphics_vulkan_parse_image_layout(attDesc.layout, attDesc.format);

				hasDepthStencil = true;
				break;
			}

			// Description
			attachments[i].flags = 0u;
			attachments[i].format = graphics_vulkan_parse_format(attDesc.format);
			attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachments[i].loadOp = graphics_vulkan_parse_attachment_loadop(attDesc.load_op);
			attachments[i].storeOp = graphics_vulkan_parse_attachment_storeop(attDesc.store_op);
			attachments[i].initialLayout = graphics_vulkan_parse_image_layout(attDesc.initial_layout);
			attachments[i].finalLayout = graphics_vulkan_parse_image_layout(attDesc.final_layout);

			if (attDesc.type == AttachmentType_DepthStencil) {
				attachments[desc.attachment_count].stencilLoadOp = graphics_vulkan_parse_attachment_loadop(attDesc.stencil_load_op);
				attachments[desc.attachment_count].stencilStoreOp = graphics_vulkan_parse_attachment_storeop(attDesc.stencil_store_op);
			}
		}

		subpass_info.pColorAttachments = colorAttachments;
		subpass_info.colorAttachmentCount = colorIt;
		subpass_info.pDepthStencilAttachment = hasDepthStencil ? &depthStencilAttachment : nullptr;
		subpass_info.flags = 0u;
		subpass_info.inputAttachmentCount = 0u;
		subpass_info.pInputAttachments = nullptr;
		subpass_info.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass_info.pPreserveAttachments = nullptr;
		subpass_info.preserveAttachmentCount = 0u;
		subpass_info.pResolveAttachments = nullptr;

		VkRenderPassCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		create_info.pSubpasses = &subpass_info;
		create_info.subpassCount = 1u;
		create_info.attachmentCount = desc.attachment_count;
		create_info.pAttachments = attachments;

		vkCheck(vkCreateRenderPass(g_API->device, &create_info, nullptr, &renderPass.renderPass));

		renderPass.beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPass.beginInfo.renderPass = renderPass.renderPass;
		renderPass.beginInfo.renderArea.offset.x = 0;
		renderPass.beginInfo.renderArea.offset.y = 0;
		renderPass.beginInfo.clearValueCount = desc.attachment_count;
		
		return true;
    }

    bool graphics_vulkan_blendstate_create(BlendState_vk& blendState, const BlendStateDesc& desc)
    {
		blendState.hash = graphics_compute_hash_blendstate(&desc);
		return true;
    }

    bool graphics_vulkan_depthstencilstate_create(DepthStencilState_vk& depthStencilState, const DepthStencilStateDesc& desc)
    {
		depthStencilState.hash = graphics_compute_hash_depthstencilstate(&desc);
		return true;
    }

    bool graphics_vulkan_buffer_destroy(Buffer_vk& buffer)
    {
		vmaDestroyBuffer(g_API->allocator, buffer.buffer, buffer.allocation);
		if (buffer.srv_texel_buffer_view) {
			vkDestroyBufferView(g_API->device, buffer.srv_texel_buffer_view, NULL);
		}
		if (buffer.uav_texel_buffer_view) {
			vkDestroyBufferView(g_API->device, buffer.uav_texel_buffer_view, NULL);
		}
		return true;
    }

    bool graphics_vulkan_image_destroy(Image_vk& image)
    {
		vmaDestroyImage(g_API->allocator, image.image, image.allocation);
		if (image.render_target_view != VK_NULL_HANDLE)	vkDestroyImageView(g_API->device, image.render_target_view, nullptr);
		if (image.depth_stencil_view != VK_NULL_HANDLE)	vkDestroyImageView(g_API->device, image.depth_stencil_view, nullptr);
		if (image.shader_resource_view.imageView != VK_NULL_HANDLE) vkDestroyImageView(g_API->device, image.shader_resource_view.imageView, nullptr);
		if (image.unordered_access_view.imageView != VK_NULL_HANDLE) vkDestroyImageView(g_API->device, image.unordered_access_view.imageView, nullptr);
		return true;
    }

    bool graphics_vulkan_sampler_destroy(Sampler_vk& sampler)
    {
		vkDestroySampler(g_API->device, sampler.sampler, nullptr);
		return true;
    }

    bool graphics_vulkan_shader_destroy(Shader_vk& shader)
    {
		vkDestroyShaderModule(g_API->device, shader.module, nullptr);
		vkDestroyDescriptorSetLayout(g_API->device, shader.layout.setLayout, nullptr);

		// Compute stuff
		{
			vkDestroyPipelineLayout(g_API->device, shader.compute.pipeline_layout, nullptr);
			vkDestroyPipeline(g_API->device, shader.compute.pipeline, nullptr);
		}
		
		return true;
    }

    bool graphics_vulkan_renderpass_destroy(RenderPass_vk& renderPass)
    {
		mutex_destroy(renderPass.mutex);
	
		vkDestroyRenderPass(g_API->device, renderPass.renderPass, nullptr);
		for (auto& it : renderPass.frameBuffers) {
			vkDestroyFramebuffer(g_API->device, it.second, nullptr);
		}
		return true;
    }

}

#endif