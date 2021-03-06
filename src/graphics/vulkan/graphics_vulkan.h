#ifndef _GRAPHICS_VULKAN
#define _GRAPHICS_VULKAN

#include "..//graphics_internal.h"

// DEVICE FUNCTIONS

#ifdef __cplusplus
extern "C" void graphics_vulkan_device_prepare(GraphicsDevice* device);
#else
void graphics_vulkan_device_prepare(GraphicsDevice* device);
#endif

#endif

#ifdef SV_VULKAN_IMPLEMENTATION
#ifndef _VULKAN_IMPLEMENTATION
#define _VULKAN_IMPLEMENTATION

namespace sv {
	b8	graphics_vulkan_initialize(b8);
	b8	graphics_vulkan_close();
	void* graphics_vulkan_get();

	b8 graphics_vulkan_create(GraphicsPrimitiveType type, const void* desc, GraphicsPrimitive* res);
	b8 graphics_vulkan_destroy(GraphicsPrimitive* primitive);

	CommandList graphics_vulkan_commandlist_begin();
	CommandList graphics_vulkan_commandlist_last();
	u32		graphics_vulkan_commandlist_count();

	void graphics_vulkan_renderpass_begin(CommandList);
	void graphics_vulkan_renderpass_end(CommandList);

	void graphics_vulkan_swapchain_resize();

	void graphics_vulkan_gpu_wait();

	void graphics_vulkan_frame_begin();
	void graphics_vulkan_frame_end();

	void graphics_vulkan_draw(u32, u32, u32, u32, CommandList);
	void graphics_vulkan_draw_indexed(u32, u32, u32, u32, u32, CommandList);
	void graphics_vulkan_dispatch(u32, u32, u32, CommandList);

	void graphics_vulkan_image_clear(GPUImage*, GPUImageLayout, GPUImageLayout, Color, float, u32, CommandList);
	void graphics_vulkan_image_blit(GPUImage*, GPUImage*, GPUImageLayout, GPUImageLayout, u32, const GPUImageBlit*, SamplerFilter, CommandList);
	void graphics_vulkan_buffer_update(GPUBuffer*, GPUBufferState, const void*, u32, u32, CommandList);
	void graphics_vulkan_barrier(const GPUBarrier*, u32, CommandList);

	void graphics_vulkan_event_begin(const char* name, CommandList cmd);
	void graphics_vulkan_event_mark(const char* name, CommandList cmd);
	void graphics_vulkan_event_end(CommandList cmd);
}

/*
  TODO list:
  - Per frame and per commandlist GPU allocator
  - Remove per buffer memory allocators
  - Reset uniform descriptors after updating a dynamic buffer
  - At the beginning of the frame reuse memory and free unused memory if it has

*/

#if SV_PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#elif SV_PLATFORM_ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#elif SV_PLATFORM_LINUX
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include "Hosebase/external/volk.h"

#pragma warning(push, 0)
#pragma warning(disable : 4701)
#pragma warning(disable : 4703)
#include "Hosebase/external/vk_mem_alloc.h"
#pragma warning(pop)

#if SV_SLOW
#define vkAssert(x) if((x) != VK_SUCCESS) assert(!#x)
#else
#define vkAssert(x) x
#endif

#define vkCheck(x) if((x) != VK_SUCCESS) return false
#define vkExt(x) do{ VkResult res = (x); if(res != VK_SUCCESS) return res; }while(0)

#undef CreateSemaphore

#include "graphics_vulkan_conversions.h"

// TEMP TODO
#include <map>
#include <vector>
#include <unordered_map>
#include <string>

namespace sv {

    struct Graphics_vk;

    Graphics_vk& graphics_vulkan_device_get();

    constexpr u32 VULKAN_MAX_DESCRIPTOR_SETS = 100u;
    constexpr u32 VULKAN_MAX_DESCRIPTOR_TYPES = 32u;
    constexpr u32 VULKAN_DESCRIPTOR_ALLOC_COUNT = 10u;
    constexpr f64 VULKAN_UNUSED_OBJECTS_TIMECHECK = 30.0;
    constexpr f64 VULKAN_UNUSED_OBJECTS_LIFETIME = 10.0;

    // MEMORY

    struct StagingBuffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation allocation = VK_NULL_HANDLE;
		void* data = nullptr;
    };

    struct VulkanGPUAllocator {
		
		struct Buffer {
			StagingBuffer staging_buffer;
			u8* current = nullptr;
			u8* end = nullptr;
		};

		std::vector<Buffer> buffers;
    };

    struct DynamicAllocation {
		VkBuffer buffer = VK_NULL_HANDLE;
		void* data = nullptr;
		u32 offset = 0u;

		inline bool isValid() const noexcept { return buffer != VK_NULL_HANDLE && data != nullptr; }
    };

    // DESCRIPTORS

	enum VulkanDescriptorType : u32 {
		VulkanDescriptorType_UniformBuffer,
		VulkanDescriptorType_UniformTexelBuffer,
		VulkanDescriptorType_SampledImage,
		VulkanDescriptorType_StorageBuffer,
		VulkanDescriptorType_StorageImage,
		VulkanDescriptorType_StorageTexelBuffer,
		VulkanDescriptorType_Sampler,
		VulkanDescriptorType_MaxEnum
	};

    struct VulkanDescriptorPool {
		VkDescriptorPool pool;
		u32 count[VulkanDescriptorType_MaxEnum];
		u32 sets;
    };

    struct VulkanDescriptorSet {
		std::vector<VkDescriptorSet>	sets;
		u32			used = 0u;
    };

    struct DescriptorPool {
		std::vector<VulkanDescriptorPool>			     pools;
		// TODO: Get out of here
		std::map<VkDescriptorSetLayout, VulkanDescriptorSet> sets;
    };

    struct ShaderResourceBinding {
		VkDescriptorType descriptor_type;
		u32				 vulkanBinding;
		u32				 userBinding;
    };
    struct ShaderDescriptorSetLayout {
		VkDescriptorSetLayout setLayout;
		std::vector<ShaderResourceBinding> bindings;
		u32 count[VulkanDescriptorType_MaxEnum];
    };

    // PIPELINE

    struct VulkanPipeline {
		VulkanPipeline& operator=(const VulkanPipeline& other)
			{
				layout = other.layout;
				pipelines = other.pipelines;
				return *this;
			}

		Mutex mutex;
		Mutex creationMutex;

		VkPipelineLayout	             layout = VK_NULL_HANDLE;
		std::unordered_map<u64, VkPipeline>   pipelines;
		VkDescriptorSet		             descriptorSets[ShaderType_GraphicsCount] = {};
		f64			                     lastUsage;
    };

    struct Shader_vk;

    size_t graphics_vulkan_pipeline_compute_hash(const GraphicsState& state);
    bool graphics_vulkan_pipeline_create(VulkanPipeline& pipeline, Shader_vk* pVertexShader, Shader_vk* pPixelShader, Shader_vk* pGeometryShader);
    bool graphics_vulkan_pipeline_destroy(VulkanPipeline& pipeline);
    VkPipeline graphics_vulkan_pipeline_get(VulkanPipeline& pipeline, GraphicsState& state, size_t hash);

    // PRIMITIVES

    // Buffer
    struct Buffer_vk : public GPUBuffer {
		VkBuffer				buffer;
		VmaAllocation			allocation;
		VkDescriptorBufferInfo	buffer_info;
		VkBufferView            srv_texel_buffer_view;
		VkBufferView            uav_texel_buffer_view;
		DynamicAllocation		dynamic_allocation[GraphicsLimit_CommandList];
    };
    // Image
    struct Image_vk : public GPUImage {
		VmaAllocation			allocation;
		VkImage					image = VK_NULL_HANDLE;
		VkImageView				render_target_view = VK_NULL_HANDLE;
		VkImageView				depth_stencil_view = VK_NULL_HANDLE;
		VkDescriptorImageInfo	shader_resource_view = {};
		VkDescriptorImageInfo	unordered_access_view = {};
		u32						layers = 1u;
		u64						ID;
    };
    // Sampler
    struct Sampler_vk : public Sampler {
		VkSampler				sampler = VK_NULL_HANDLE;
		VkDescriptorImageInfo	image_info;
    };
    // Shader
    struct Shader_vk : public Shader {
		VkShaderModule		      module = VK_NULL_HANDLE;
		std::unordered_map<std::string, u32>  semanticNames;
		ShaderDescriptorSetLayout layout;
		u64			              ID;

		struct {
			VkPipelineLayout pipeline_layout;
			VkPipeline       pipeline;
			f64 last_usage;
		} compute;
    };
    // RenderPass
    struct RenderPass_vk : public RenderPass {
		VkRenderPass				renderPass;
		std::vector<std::pair<u64, VkFramebuffer>>	frameBuffers;
		VkRenderPassBeginInfo			beginInfo;
		Mutex					mutex;
    };
    
    // BlendState
    struct BlendState_vk : public BlendState {
		u64 hash;
    };
    // DepthStencilState
    struct DepthStencilState_vk : public DepthStencilState {
		u64 hash;
    };

    // API STRUCTS

    struct Frame {
		VkCommandPool		commandPool;
		VkCommandBuffer		commandBuffers[GraphicsLimit_CommandList];
		VkCommandPool		transientCommandPool;
		Mutex               transient_command_pool_mutex;
		VkFence				fence;
		DescriptorPool		descPool[GraphicsLimit_CommandList];
		VulkanGPUAllocator	allocator[GraphicsLimit_CommandList];
    };

    struct SwapChain_vk {
	    
		VkSurfaceKHR						surface = VK_NULL_HANDLE;
		VkSwapchainKHR						swapChain = VK_NULL_HANDLE;

		VkSemaphore							semAcquireImage = VK_NULL_HANDLE;
		VkSemaphore							semPresent = VK_NULL_HANDLE;

		VkSurfaceCapabilitiesKHR			capabilities;
		std::vector<VkPresentModeKHR>		presentModes;
		std::vector<VkSurfaceFormatKHR>		formats;

		VkFormat							currentFormat;
		VkColorSpaceKHR						currentColorSpace;
		VkPresentModeKHR					currentPresentMode;
		VkExtent2D							currentExtent;

		std::vector<VkFence>				imageFences;
		u32									imageIndex;

		struct Image {
			VkImage image;
			VkImageView view;
		};
		std::vector<Image> images;
    };

    struct Graphics_vk {

		struct {
			
			VkPhysicalDevice		         physicalDevice = VK_NULL_HANDLE;
			VkPhysicalDeviceProperties	     properties;
			VkPhysicalDeviceFeatures	     features;
			VkPhysicalDeviceMemoryProperties memoryProps;

			struct {
				u32 graphics = UINT32_MAX;

				inline bool IsComplete() const noexcept { return graphics != u32_max; }

			} familyIndex;

		} card;

		VkInstance   instance = VK_NULL_HANDLE;
		VkDevice     device = VK_NULL_HANDLE;
		VmaAllocator allocator;

		SwapChain_vk swapchain;

		VkDebugUtilsMessengerEXT debug = VK_NULL_HANDLE;

		std::vector<const char*> extensions;
		std::vector<const char*> validationLayers;
		std::vector<const char*> deviceExtensions;
		std::vector<const char*> deviceValidationLayers;

		VkQueue queueGraphics = VK_NULL_HANDLE;
	
		f64 lastTime = 0.0;

		// Frame Members

		std::vector<Frame> frames;
		u32	    frameCount;
		u32	    currentFrame = 0u;
		Mutex       mutexCMD;
		u32	    activeCMDCount = 0u;

		inline Frame& GetFrame() noexcept { return frames[currentFrame]; }
		inline VkCommandBuffer GetCMD(CommandList cmd) { return frames[currentFrame].commandBuffers[cmd]; }

		// Binding Members

		VkRenderPass activeRenderPass[GraphicsLimit_CommandList];

		// TODO
		std::unordered_map<u64, VulkanPipeline>        pipelines;
		Mutex			                               pipeline_mutex;

		u64 IDCount = 0u;
		Mutex IDMutex;
	
		inline u64 GetID() noexcept { SV_LOCK_GUARD(IDMutex, lock); return IDCount++; }

    };

    // API FUNCTIONS

    VkResult graphics_vulkan_singletimecmb_begin(VkCommandBuffer* pCmd);
    VkResult graphics_vulkan_singletimecmb_end(VkCommandBuffer cmd);

    void graphics_vulkan_buffer_copy(VkCommandBuffer cmd, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);
    VkResult graphics_vulkan_imageview_create(VkImage image, VkFormat format, VkImageViewType viewType, VkImageAspectFlags aspectFlags, u32 layerCount, VkImageView& view);

    VkSemaphore graphics_vulkan_semaphore_create();
    VkFence graphics_vulkan_fence_create(bool sign);

    void graphics_vulkan_acquire_image();
    void graphics_vulkan_submit_commandbuffers();
    void graphics_vulkan_present();

    bool graphics_vulkan_buffer_create(Buffer_vk& buffer, const GPUBufferDesc& desc);
    bool graphics_vulkan_image_create(Image_vk& image, const GPUImageDesc& desc);
    bool graphics_vulkan_sampler_create(Sampler_vk& sampler, const SamplerDesc& desc);
    bool graphics_vulkan_shader_create(Shader_vk& shader, const ShaderDesc& desc);
    bool graphics_vulkan_renderpass_create(RenderPass_vk& renderPass, const RenderPassDesc& desc);
    bool graphics_vulkan_blendstate_create(BlendState_vk& blendState, const BlendStateDesc& desc);
    bool graphics_vulkan_depthstencilstate_create(DepthStencilState_vk& depthStencilState, const DepthStencilStateDesc& desc);
    bool graphics_vulkan_swapchain_create();

    bool graphics_vulkan_buffer_destroy(Buffer_vk& buffer);
    bool graphics_vulkan_image_destroy(Image_vk& image);
    bool graphics_vulkan_sampler_destroy(Sampler_vk& sampler);
    bool graphics_vulkan_shader_destroy(Shader_vk& shader);
    bool graphics_vulkan_renderpass_destroy(RenderPass_vk& renderPass);
    bool graphics_vulkan_swapchain_destroy( bool resizing);

}

#endif
#endif
