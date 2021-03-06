#pragma once

#include "Hosebase/defines.h"

#if SV_GRAPHICS

#include "Hosebase/allocators.h"

#ifdef __cplusplus
extern "C" {
#endif
	
typedef struct GPUBuffer GPUBuffer;
typedef struct GPUImage GPUImage;
typedef struct Sampler Sampler;
typedef struct RenderPass RenderPass;
typedef struct Shader Shader;
typedef struct BlendState BlendState;
typedef struct DepthStencilState DepthStencilState;

// Enums

typedef enum {
	GraphicsAPI_Invalid,
	GraphicsAPI_Vulkan,
} GraphicsAPI;

typedef enum {
	GraphicsLimit_CommandList = 32,
	GraphicsLimit_VertexBuffer = 32,
	GraphicsLimit_ConstantBuffer = 32u,
	GraphicsLimit_ShaderResource = 32u,
	GraphicsLimit_UnorderedAccessView = 32u,
	GraphicsLimit_Sampler = 16u,
	GraphicsLimit_Viewport = 16u,
	GraphicsLimit_Scissor = 16u,
	GraphicsLimit_InputSlot = 16u,
	GraphicsLimit_InputElement = 16u,
	GraphicsLimit_GPUBarrier = 8u,
	GraphicsLimit_AttachmentRT = 8u,
	GraphicsLimit_Attachment = (GraphicsLimit_AttachmentRT + 1u)
} GraphicsLimit;

typedef enum {
	GraphicsPrimitiveType_Invalid,
	GraphicsPrimitiveType_Image,
	GraphicsPrimitiveType_Sampler,
	GraphicsPrimitiveType_Buffer,
	GraphicsPrimitiveType_Shader,
	GraphicsPrimitiveType_RenderPass,
	GraphicsPrimitiveType_BlendState,
	GraphicsPrimitiveType_DepthStencilState,
} GraphicsPrimitiveType;

typedef enum {
	GPUBufferType_Invalid = 0,
	GPUBufferType_Vertex = SV_BIT(0),
	GPUBufferType_Index = SV_BIT(1),
	GPUBufferType_Constant = SV_BIT(2),
	GPUBufferType_ShaderResource = SV_BIT(3),
	GPUBufferType_UnorderedAccessView = SV_BIT(4),
} GPUBufferType;

typedef enum {
	GPUBufferState_Undefined,
	GPUBufferState_Vertex,
	GPUBufferState_Index,
	GPUBufferState_Constant,
	GPUBufferState_ShaderResource,
	GPUBufferState_UnorderedAccessView,
} GPUBufferState;

typedef enum {
	ShaderType_Vertex,
	ShaderType_Pixel,
	ShaderType_Geometry,
	ShaderType_Hull,
	ShaderType_Domain,
	ShaderType_GraphicsCount = 5u,

	ShaderType_Compute,
} ShaderType;

typedef enum {
	GPUImageType_RenderTarget	     = SV_BIT(0),
	GPUImageType_ShaderResource      = SV_BIT(1),
	GPUImageType_UnorderedAccessView = SV_BIT(2),
	GPUImageType_DepthStencil	     = SV_BIT(3),
	GPUImageType_CubeMap		     = SV_BIT(4),
} GPUImageType;

typedef enum {
	ResourceUsage_Default,
	ResourceUsage_Static,
	ResourceUsage_Dynamic,
	ResourceUsage_Staging
} ResourceUsage;
	
typedef enum {
	CPUAccess_None = 0u,
	CPUAccess_Read = SV_BIT(0),
	CPUAccess_Write = SV_BIT(1),
	CPUAccess_All = CPUAccess_Read | CPUAccess_Write,
} CPUAccess;

typedef enum {
	GPUImageLayout_Undefined,
	GPUImageLayout_General,
	GPUImageLayout_RenderTarget,
	GPUImageLayout_DepthStencil,
	GPUImageLayout_DepthStencilReadOnly,
	GPUImageLayout_ShaderResource,
	GPUImageLayout_UnorderedAccessView,
	GPUImageLayout_Present,
} GPUImageLayout;
	
typedef enum {

	Format_Unknown,
	Format_R32G32B32A32_FLOAT,
	Format_R32G32B32A32_UINT,
	Format_R32G32B32A32_SINT,
	Format_R32G32B32_FLOAT,
	Format_R32G32B32_UINT,
	Format_R32G32B32_SINT,
	Format_R16G16B16A16_FLOAT,
	Format_R16G16B16A16_UNORM,
	Format_R16G16B16A16_UINT,
	Format_R16G16B16A16_SNORM,
	Format_R16G16B16A16_SINT,
	Format_R16G16B16_FLOAT,
	Format_R16G16B16_UNORM,
	Format_R16G16B16_UINT,
	Format_R16G16B16_SNORM,
	Format_R16G16B16_SINT,
	Format_R32G32_FLOAT,
	Format_R32G32_UINT,
	Format_R32G32_SINT,
	Format_R8G8B8A8_UNORM,
	Format_R8G8B8A8_SRGB,
	Format_R8G8B8A8_UINT,
	Format_R8G8B8A8_SNORM,
	Format_R8G8B8A8_SINT,
	Format_R16G16_FLOAT,
	Format_R16G16_UNORM,
	Format_R16G16_UINT,
	Format_R16G16_SNORM,
	Format_R16G16_SINT,
	Format_D32_FLOAT,
	Format_R32_FLOAT,
	Format_R32_UINT,
	Format_R32_SINT,
	Format_D24_UNORM_S8_UINT,
	Format_R8G8_UNORM,
	Format_R8G8_UINT,
	Format_R8G8_SNORM,
	Format_R8G8_SINT,
	Format_R16_FLOAT,
	Format_D16_UNORM,
	Format_R16_UNORM,
	Format_R16_UINT,
	Format_R16_SNORM,
	Format_R16_SINT,
	Format_R8_UNORM,
	Format_R8_UINT,
	Format_R8_SNORM,
	Format_R8_SINT,
	Format_BC1_UNORM,
	Format_BC1_SRGB,
	Format_BC2_UNORM,
	Format_BC2_SRGB,
	Format_BC3_UNORM,
	Format_BC3_SRGB,
	Format_BC4_UNORM,
	Format_BC4_SNORM,
	Format_BC5_UNORM,
	Format_BC5_SNORM,
	Format_B8G8R8A8_UNORM,
	Format_B8G8R8A8_SRGB,

} Format;
	
typedef enum {
	GraphicsTopology_Points,
	GraphicsTopology_Lines,
	GraphicsTopology_LineStrip,
	GraphicsTopology_Triangles,
	GraphicsTopology_TriangleStrip,
} GraphicsTopology;
	
typedef enum {
	IndexType_16,
	IndexType_32,
} IndexType;
	
typedef enum {
	CullMode_None,
	CullMode_Front,
	CullMode_Back,
	CullMode_FrontAndBack,
} CullMode;

typedef enum {
	AttachmentOperation_DontCare,
	AttachmentOperation_Load,
	AttachmentOperation_Store,
	AttachmentOperation_Clear
} AttachmentOperation;

typedef enum {
	AttachmentType_RenderTarget,
	AttachmentType_DepthStencil
} AttachmentType;

typedef enum {
	CompareOperation_Never,
	CompareOperation_Less,
	CompareOperation_Equal,
	CompareOperation_LessOrEqual,
	CompareOperation_Greater,
	CompareOperation_NotEqual,
	CompareOperation_GreaterOrEqual,
	CompareOperation_Always,
} CompareOperation;

typedef enum {
	StencilOperation_Keep,
	StencilOperation_Zero,
	StencilOperation_Replace,
	StencilOperation_IncrementAndClamp,
	StencilOperation_DecrementAndClamp,
	StencilOperation_Invert,
	StencilOperation_IncrementAndWrap,
	StencilOperation_DecrementAndWrap
} StencilOperation;

typedef enum {
	BlendOperation_Add,
	BlendOperation_Substract,
	BlendOperation_ReverseSubstract,
	BlendOperation_Min,
	BlendOperation_Max,
} BlendOperation;

typedef enum {
	ColorComponent_None = 0,
	ColorComponent_R = SV_BIT(0),
	ColorComponent_G = SV_BIT(1),
	ColorComponent_B = SV_BIT(2),
	ColorComponent_A = SV_BIT(3),
	ColorComponent_RGB = (ColorComponent_R | ColorComponent_G | ColorComponent_B),
	ColorComponent_All = (ColorComponent_RGB | ColorComponent_A),
} ColorComponent;

typedef enum {
	BlendFactor_Zero,
	BlendFactor_One,
	BlendFactor_SrcColor,
	BlendFactor_OneMinusSrcColor,
	BlendFactor_DstColor,
	BlendFactor_OneMinusDstColor,
	BlendFactor_SrcAlpha,
	BlendFactor_OneMinusSrcAlpha,
	BlendFactor_DstAlpha,
	BlendFactor_OneMinusDstAlpha,
	BlendFactor_ConstantColor,
	BlendFactor_OneMinusConstantColor,
	BlendFactor_ConstantAlpha,
	BlendFactor_OneMinusConstantAlpha,
	BlendFactor_SrcAlphaSaturate,
	BlendFactor_Src1Color,
	BlendFactor_OneMinusSrc1Color,
	BlendFactor_Src1Alpha,
	BlendFactor_OneMinusSrc1Alpha,
} BlendFactor;

typedef enum {
	SamplerFilter_Nearest,
	SamplerFilter_Linear,
} SamplerFilter;

typedef enum {
	SamplerAddressMode_Wrap,
	SamplerAddressMode_Mirror,
	SamplerAddressMode_Clamp,
	SamplerAddressMode_Border
} SamplerAddressMode;
	
typedef enum {
	BarrierType_Image
} BarrierType;

typedef struct {
	f32 x, y, width, height, min_depth, max_depth;
} Viewport;

typedef struct {
	u32 x, y, width, height;
} Scissor;



typedef struct {
	union {
		struct {
			GPUImage* image;
			GPUImageLayout old_layout;
			GPUImageLayout new_layout;
		} image;
	};
	BarrierType type;
	
} GPUBarrier;

SV_INLINE GPUBarrier gpu_barrier_image(GPUImage* image, GPUImageLayout old_layout, GPUImageLayout new_layout)
{
	GPUBarrier barrier;
	barrier.image.image = image;
	barrier.image.old_layout = old_layout;
	barrier.image.new_layout = new_layout;
	barrier.type = BarrierType_Image;
	return barrier;
}

typedef struct {
	v3_i32 offset0;
	v3_i32 offset1;
} GPUImageRegion;

typedef struct {
	GPUImageRegion src_region;
	GPUImageRegion dst_region;
} GPUImageBlit;

// Primitive Descriptors

typedef struct {
	u32	            buffer_type;
	ResourceUsage	usage;
	u32	            cpu_access;
	u32				size;
	IndexType		index_type;
	void*			data;
	Format          format;
} GPUBufferDesc;

typedef struct {
	u32	            buffer_type;
	ResourceUsage	usage;
	u32             cpu_access;
	u32				size;
	IndexType		index_type;
	Format          format;
} GPUBufferInfo;

typedef struct {
	void*				data;
	u32					size;
	Format				format;
	GPUImageLayout		layout;
	u32                 type;
	ResourceUsage		usage;
	u32                 cpu_access;
	u32					width;
	u32					height;
} GPUImageDesc;

typedef struct {
	Format				format;
	u32                 type;
	ResourceUsage		usage;
	u32                 cpu_access;
	u32					width;
	u32					height;
} GPUImageInfo;

typedef struct {
	SamplerAddressMode	address_mode_u;
	SamplerAddressMode	address_mode_v;
	SamplerAddressMode	address_mode_w;
	SamplerFilter		min_filter;
	SamplerFilter		mag_filter;
} SamplerDesc;

typedef SamplerDesc SamplerInfo;

typedef enum {
    ShaderResourceType_ConstantBuffer,
    ShaderResourceType_Image,
    ShaderResourceType_TexelBuffer,
    ShaderResourceType_StorageBuffer,
    ShaderResourceType_UAImage,
    ShaderResourceType_UATexelBuffer,
    ShaderResourceType_Sampler,
} ShaderResourceType;

typedef struct {
    char name[NAME_SIZE];
    ShaderResourceType type;
    u32 binding;
} ShaderResource;

typedef struct {
    char name[NAME_SIZE];
    u32 location;
} ShaderSemanticName;

typedef struct {
	ShaderType type;
    void* bin_data;
    u32 bin_data_size;

    ShaderSemanticName semantic_names[20];
    u32 semantic_name_count;

    u32 resource_count;
    ShaderResource resources[GraphicsLimit_ConstantBuffer + GraphicsLimit_ShaderResource + GraphicsLimit_UnorderedAccessView + GraphicsLimit_Sampler];
}  ShaderDesc;

typedef ShaderDesc ShaderInfo;

typedef struct {
	AttachmentOperation	load_op;
	AttachmentOperation store_op;
	AttachmentOperation stencil_load_op;
	AttachmentOperation stencil_store_op;
	Format				format;
	GPUImageLayout		initial_layout;
	GPUImageLayout		layout;
	GPUImageLayout		final_layout;
	AttachmentType		type;
} AttachmentDesc;

typedef struct {
	AttachmentDesc* attachments;
	u32				attachment_count;
} RenderPassDesc;

typedef struct {
	u32            depthstencil_attachment_index;
	AttachmentDesc attachments[GraphicsLimit_Attachment];
	u32            attachment_count;
} RenderPassInfo;

typedef struct {
	b8				    blend_enabled;
	BlendFactor			src_color_blend_factor;
	BlendFactor			dst_color_blend_factor;
	BlendOperation		color_blend_op;
	BlendFactor			src_alpha_blend_factor;
	BlendFactor			dst_alpha_blend_factor;
	BlendOperation		alpha_blend_op;
	u32                 color_write_mask;
} BlendAttachmentDesc;

typedef struct {
	BlendAttachmentDesc* attachments;
	u32		             attachment_count;
	v4		             blend_constants;
} BlendStateDesc;

typedef struct {
	BlendAttachmentDesc attachments[GraphicsLimit_AttachmentRT];
	u32                 attachment_count;
	v4			        blend_constants;
} BlendStateInfo;

typedef struct {
	StencilOperation fail_op;
	StencilOperation pass_op;
	StencilOperation depth_fail_op;
	CompareOperation compare_op;
} StencilStateDesc;

typedef struct {
	b8               depth_test_enabled;
	b8               depth_write_enabled;
	CompareOperation depth_compare_op;
	b8		         stencil_test_enabled;
	u32		         read_mask;
	u32		         write_mask;
	StencilStateDesc front;
	StencilStateDesc back;
} DepthStencilStateDesc;

typedef DepthStencilStateDesc DepthStencilStateInfo;

// Format functions

b8  graphics_format_has_stencil(Format format);
u32 graphics_format_size(Format format);

typedef u32 CommandList;

typedef struct {
	b8 validation;
} GraphicsInitializeDesc;

b8   _graphics_initialize(const GraphicsInitializeDesc* desc);
void _graphics_close();
b8 graphics_begin();
void graphics_end();

typedef enum {
	SwapchainRotation_0,
	SwapchainRotation_90,
	SwapchainRotation_180,
	SwapchainRotation_270,
} SwapchainRotation;

void graphics_swapchain_resize();
SwapchainRotation graphics_swapchain_rotation();
m4 graphics_swapchain_rotation_matrix();

GraphicsAPI graphics_api();

void graphics_present_image(GPUImage* image, GPUImageLayout layout);

// Hash functions

u64 graphics_compute_hash_blendstate(const BlendStateDesc* desc);
u64 graphics_compute_hash_depthstencilstate(const DepthStencilStateDesc* desc);

// Primitives
    
b8 graphics_buffer_create(GPUBuffer** buffer, const GPUBufferDesc* desc);
b8 graphics_shader_create(Shader** shader, const ShaderDesc* desc);
b8 graphics_image_create(GPUImage** image, const GPUImageDesc* desc);
b8 graphics_sampler_create(Sampler** sampler, const SamplerDesc* desc);
b8 graphics_renderpass_create(RenderPass** render_pass, const RenderPassDesc* desc);
b8 graphics_blendstate_create(BlendState** blend_state, const BlendStateDesc* desc);
b8 graphics_depthstencilstate_create(DepthStencilState** depth_stencil_state, const DepthStencilStateDesc* desc);

void graphics_destroy(void* primitive);
	
void graphics_destroy_struct(void* data, size_t size);

// CommandList functions

CommandList graphics_commandlist_begin();
CommandList graphics_commandlist_last();
CommandList graphics_commandlist_get();
u32	        graphics_commandlist_count();

void graphics_gpu_wait();

// Resource functions

void graphics_resources_unbind(CommandList cmd);

void graphics_vertex_buffer_bind_array(GPUBuffer** buffers, u32* offsets, u32 count, u32 beginSlot, CommandList cmd);
void graphics_vertex_buffer_bind(GPUBuffer* buffer, u32 offset, u32 slot, CommandList cmd);
void graphics_vertex_buffer_unbind(u32 slot, CommandList cmd);
void graphics_vertex_buffer_unbind_commandlist(CommandList cmd);
	
void graphics_index_buffer_bind(GPUBuffer* buffer, u32 offset, CommandList cmd);
void graphics_index_buffer_unbind(CommandList cmd);

void graphics_constant_buffer_bind(void* data, u32 size, u32 slot, ShaderType shader_type, CommandList cmd);

// Shader Resources

typedef enum {
	ResourceType_ShaderResource,
	ResourceType_ConstantBuffer,
	ResourceType_UnorderedAccessView,
	ResourceType_Sampler,
} ResourceType;

void graphics_resource_bind(ResourceType type, void* primitive, u32 slot, ShaderType shader_type, CommandList cmd);

// State functions

void graphics_shader_bind(Shader* shader, CommandList cmd);
void graphics_shader_bind_asset(Asset asset, CommandList cmd);
void graphics_blendstate_bind(BlendState* blendState, CommandList cmd);
void graphics_depthstencilstate_bind(DepthStencilState* depthStencilState, CommandList cmd);

// Input layout

void graphics_inputlayout_reset(u32 slots, CommandList cmd);

void graphics_inputlayout_set_slot(u32 slot, u32 stride, b8 instanced, CommandList cmd);
void graphics_inputlayout_add_element(u32 slot, const char* name, Format format, u32 index, CommandList cmd);

// Rasterizer

void graphics_rasterizer_set(b8 wireframe, CullMode cull_mode, b8 clockwise, CommandList cmd);

void graphics_shader_unbind(ShaderType shaderType, CommandList cmd);
void graphics_shader_unbind_commandlist(CommandList cmd);
void graphics_blendstate_unbind(CommandList cmd);
void graphics_depthstencilstate_unbind(CommandList cmd);

void graphics_topology_set(GraphicsTopology topology, CommandList cmd);
void graphics_stencil_reference_set(u32 ref, CommandList cmd);
void graphics_line_width_set(float lineWidth, CommandList cmd);

GraphicsTopology graphics_topology_get(CommandList cmd);
u32				 graphics_stencil_reference_get(CommandList cmd);
float			 graphics_line_width_get(CommandList cmd);

void graphics_viewport_set_array(const Viewport* viewports, u32 count, CommandList cmd);
void graphics_viewport_set(Viewport viewport, u32 slot, CommandList cmd);
void graphics_viewport_set_image(GPUImage* image, u32 slot, CommandList cmd);
void graphics_scissor_set_array(const Scissor* scissors, u32 count, CommandList cmd);
void graphics_scissor_set(Scissor scissor, u32 slot, CommandList cmd);
void graphics_scissor_set_image(GPUImage* image, u32 slot, CommandList cmd);

Viewport graphics_viewport_get(u32 slot, CommandList cmd);
Scissor  graphics_scissor_get(u32 slot, CommandList cmd);

Viewport graphics_image_viewport(const GPUImage* image);
Scissor  graphics_image_scissor(const GPUImage* image);

// Info

const GPUBufferInfo*	     graphics_buffer_info(const GPUBuffer* buffer);
const GPUImageInfo*		     graphics_image_info(const GPUImage* image);
const ShaderInfo*		     graphics_shader_info(const Shader* shader);
const RenderPassInfo*	     graphics_renderpass_info(const RenderPass* renderpass);
const SamplerInfo*		     graphics_sampler_info(const Sampler* sampler);
const BlendStateInfo*	     graphics_blendstate_info(const BlendState* blendstate);
const DepthStencilStateInfo* graphics_depthstencilstate_info(const DepthStencilState* dss);

// Resource getters

GPUImage* graphics_image_get(u32 slot, ShaderType shader_type, CommandList cmd);

// RenderPass functions

void graphics_renderpass_begin(RenderPass* renderPass, GPUImage** attachments, const Color* colors, f32 depth, u32 stencil, CommandList cmd);
void graphics_renderpass_end(CommandList cmd);

// Draw Calls

void graphics_draw(u32 vertexCount, u32 instanceCount, u32 startVertex, u32 startInstance, CommandList cmd);
void graphics_draw_indexed(u32 indexCount, u32 instanceCount, u32 startIndex, u32 startVertex, u32 startInstance, CommandList cmd);

void graphics_dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z, CommandList cmd);
void graphics_dispatch_image(GPUImage* image, u32 group_size_x, u32 group_size_y, CommandList cmd);

// Memory

void graphics_buffer_update(GPUBuffer* buffer, GPUBufferState buffer_state, const void* data, u32 size, u32 offset, CommandList cmd);
void graphics_barrier(const GPUBarrier* barriers, u32 count, CommandList cmd);
void graphics_image_blit(GPUImage* src, GPUImage* dst, GPUImageLayout srcLayout, GPUImageLayout dstLayout, u32 count, const GPUImageBlit* imageBlit, SamplerFilter filter, CommandList cmd);
void graphics_image_clear(GPUImage* image, GPUImageLayout oldLayout, GPUImageLayout newLayout, Color clearColor, float depth, u32 stencil, CommandList cmd); // Not use if necessary, renderpasses have best performance!!

// Assets

// TODO: SV_DEFINE_ASSET_PTR(TextureAsset, GPUImage*);

// DEBUG

#if SV_EDITOR

void graphics_event_begin(const char* name, CommandList cmd);
void graphics_event_mark(const char* name, CommandList cmd);
void graphics_event_end(CommandList cmd);

void graphics_name_set(Primitive* primitive, const char* name);

#else
#define graphics_event_begin(name, cmd) {}
#define graphics_event_mark(name, cmd) {}
#define graphics_event_end(cmd) {}
#define graphics_name_set(primitive, name) {}
#define graphics_offscreen_validation(offscreen) {}
#endif

#ifdef __cplusplus
}
#endif

#endif