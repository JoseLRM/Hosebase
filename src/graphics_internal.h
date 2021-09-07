#pragma once

#include "Hosebase/graphics.h"
#include "Hosebase/os.h"

#ifdef __cplusplus
extern "C" {
#endif

// Primitives internal

typedef struct {
	GraphicsPrimitiveType type;
	
#if SV_GFX
	std::string name;
#endif

} GraphicsPrimitive;

struct GPUBuffer {
	GraphicsPrimitive primitive;
	GPUBufferInfo info;
};

struct GPUImage {
	GraphicsPrimitive primitive;
	GPUImageInfo info;
};

struct Sampler {
	GraphicsPrimitive primitive;
	SamplerInfo info;
};

struct Shader {
	GraphicsPrimitive primitive;
	ShaderInfo info;
};

struct RenderPass {
	GraphicsPrimitive primitive;
	RenderPassInfo info;
};

struct InputLayoutState {
	GraphicsPrimitive primitive;
	InputLayoutStateInfo info;
};

struct BlendState {
	GraphicsPrimitive primitive;
	BlendStateInfo info;
};

struct DepthStencilState {
	GraphicsPrimitive primitive;
	DepthStencilStateInfo info;
};

struct RasterizerState {
	GraphicsPrimitive primitive;
	RasterizerStateInfo info;
};

// Pipeline state

#define GraphicsPipelineState_None 0
#define GraphicsPipelineState_VertexBuffer SV_BIT(0)
#define GraphicsPipelineState_IndexBuffer SV_BIT(1)
	
#define GraphicsPipelineState_ConstantBuffer SV_BIT(2)
#define GraphicsPipelineState_ShaderResource SV_BIT(2)
#define GraphicsPipelineState_UnorderedAccessView SV_BIT(2)
#define GraphicsPipelineState_Sampler SV_BIT(2)
		
#define GraphicsPipelineState_Resource_VS SV_BIT(3)
#define GraphicsPipelineState_Resource_PS SV_BIT(4)
#define GraphicsPipelineState_Resource_GS SV_BIT(5)
#define GraphicsPipelineState_Resource_HS SV_BIT(6)
#define GraphicsPipelineState_Resource_DS SV_BIT(7)
#define GraphicsPipelineState_Resource_MS SV_BIT(8)
#define GraphicsPipelineState_Resource_TS SV_BIT(9)

#define GraphicsPipelineState_Shader SV_BIT(26)
#define GraphicsPipelineState_Shader_VS SV_BIT(27)
#define GraphicsPipelineState_Shader_PS SV_BIT(28)
#define GraphicsPipelineState_Shader_GS SV_BIT(29)
#define GraphicsPipelineState_Shader_HS SV_BIT(30)
#define GraphicsPipelineState_Shader_DS SV_BIT(31)
#define GraphicsPipelineState_Shader_MS SV_BIT(32)
#define GraphicsPipelineState_Shader_TS SV_BIT(33)

#define GraphicsPipelineState_RenderPass SV_BIT(34)

#define GraphicsPipelineState_InputLayoutState SV_BIT(35)
#define GraphicsPipelineState_BlendState SV_BIT(36)
#define GraphicsPipelineState_DepthStencilState SV_BIT(37)
#define GraphicsPipelineState_RasterizerState SV_BIT(38)

#define GraphicsPipelineState_Viewport (u64)((u64)1 << (u64)39) 
#define GraphicsPipelineState_Scissor SV_BIT(40)
#define GraphicsPipelineState_Topology SV_BIT(41)
#define GraphicsPipelineState_StencilRef SV_BIT(42)
#define GraphicsPipelineState_LineWidth SV_BIT(43)

typedef struct {
	GPUBuffer*				vertex_buffers[GraphicsLimit_VertexBuffer];
	u32						vertex_buffer_offsets[GraphicsLimit_VertexBuffer];
	u32						vertex_buffer_count;
	
	GPUBuffer*				index_buffer;
	u32						index_buffer_offset;
	
	void*				    shader_resources[ShaderType_GraphicsCount][GraphicsLimit_ShaderResource];
	u32						shader_resource_count[ShaderType_GraphicsCount];
	
	GPUBuffer*				constant_buffers[ShaderType_GraphicsCount][GraphicsLimit_ConstantBuffer];
	u32						constant_buffer_count[ShaderType_GraphicsCount];
	
	void*				    unordered_access_views[ShaderType_GraphicsCount][GraphicsLimit_UnorderedAccessView];
	u32						unordered_access_view_count[ShaderType_GraphicsCount];
	
	Sampler*				samplers[ShaderType_GraphicsCount][GraphicsLimit_Sampler];
	u32						sampler_count[ShaderType_GraphicsCount];
	
	Shader*				    vertex_shader;
	Shader*				    pixel_shader;
	Shader*				    geometry_shader;
	
	InputLayoutState*		input_layout_state;
	BlendState*			    blend_state;
	DepthStencilState*		depth_stencil_state;
	RasterizerState*		rasterizer_state;
	
	Viewport				viewports[GraphicsLimit_Viewport];
	u32						viewport_count;
	Scissor					scissors[GraphicsLimit_Scissor];
	u32						scissor_count;
	GraphicsTopology		topology;
	u32						stencil_reference;
	float					line_width;
	
	RenderPass*			    render_pass;
	GPUImage*				attachments[GraphicsLimit_Attachment];
	v4					    clear_colors[GraphicsLimit_Attachment];
	f32                     clear_depth;
	u32                     clear_stencil;
	
	u64 flags;
} GraphicsState;

typedef struct {
	Shader*    compute_shader;

	void*      shader_resources[GraphicsLimit_ShaderResource];
	u32		   shader_resource_count;

	GPUBuffer* constant_buffers[GraphicsLimit_ConstantBuffer];
	u32		   constant_buffer_count;
		
	void*	   unordered_access_views[GraphicsLimit_UnorderedAccessView];
	u32		   unordered_access_view_count;

	Sampler*   samplers[GraphicsLimit_Sampler];
	u32        sampler_count;

	b8         update_resources;
} ComputeState;

typedef struct {
	GraphicsState graphics[GraphicsLimit_CommandList];
	ComputeState  compute[GraphicsLimit_CommandList];

	GPUImage*      present_image;
	GPUImageLayout present_image_layout;
} PipelineState;

// GraphicsAPI Device

typedef b8(*FNP_graphics_api_initialize)(b8);
typedef b8(*FNP_graphics_api_close)();
typedef void*(*FNP_graphics_api_get)();

typedef b8(*FNP_graphics_create)(GraphicsPrimitiveType, const void*, GraphicsPrimitive*);
typedef b8(*FNP_graphics_destroy)(GraphicsPrimitive*);

typedef CommandList(*FNP_graphics_api_commandlist_begin)();
typedef CommandList(*FNP_graphics_api_commandlist_last)();
typedef u32(*FNP_graphics_api_commandlist_count)();

typedef void(*FNP_graphics_api_renderpass_begin)(CommandList);
typedef void(*FNP_graphics_api_renderpass_end)(CommandList);

typedef void(*FNP_graphics_api_swapchain_resize)();

typedef void(*FNP_graphics_api_gpu_wait)();

typedef void(*FNP_graphics_api_frame_begin)();
typedef void(*FNP_graphics_api_frame_end)();

typedef void(*FNP_graphics_api_draw)(u32, u32, u32, u32, CommandList);
typedef void(*FNP_graphics_api_draw_indexed)(u32, u32, u32, u32, u32, CommandList);
typedef void(*FNP_graphics_api_dispatch)(u32, u32, u32, CommandList);

typedef void(*FNP_graphics_api_image_clear)(GPUImage*, GPUImageLayout, GPUImageLayout, Color, float, u32, CommandList);
typedef void(*FNP_graphics_api_image_blit)(GPUImage*, GPUImage*, GPUImageLayout, GPUImageLayout, u32, const GPUImageBlit*, SamplerFilter, CommandList);
typedef void(*FNP_graphics_api_buffer_update)(GPUBuffer*, GPUBufferState, const void*, u32, u32, CommandList);
typedef void(*FNP_graphics_api_barrier)(const GPUBarrier*, u32, CommandList);

typedef void(*FNP_graphics_api_event_begin)(const char*, CommandList);
typedef void(*FNP_graphics_api_event_mark)(const char*, CommandList);
typedef void(*FNP_graphics_api_event_end)(CommandList);

typedef struct {

	FNP_graphics_api_initialize	initialize;
	FNP_graphics_api_close		close;
	FNP_graphics_api_get		get;
	
	FNP_graphics_create    create;
	FNP_graphics_destroy   destroy;

	FNP_graphics_api_commandlist_begin	commandlist_begin;
	FNP_graphics_api_commandlist_last	commandlist_last;
	FNP_graphics_api_commandlist_count	commandlist_count;

	FNP_graphics_api_renderpass_begin	renderpass_begin;
	FNP_graphics_api_renderpass_end		renderpass_end;

	FNP_graphics_api_swapchain_resize swapchain_resize;

	FNP_graphics_api_gpu_wait gpu_wait;

	FNP_graphics_api_frame_begin		frame_begin;
	FNP_graphics_api_frame_begin		frame_end;

	FNP_graphics_api_draw		  draw;
	FNP_graphics_api_draw_indexed draw_indexed;
	FNP_graphics_api_dispatch     dispatch;
		
	FNP_graphics_api_image_clear	image_clear;
	FNP_graphics_api_image_blit	image_blit;
	FNP_graphics_api_buffer_update	buffer_update;
	FNP_graphics_api_barrier	barrier;

	FNP_graphics_api_event_begin	event_begin;
	FNP_graphics_api_event_mark	event_mark;
	FNP_graphics_api_event_end	event_end;

	// TODO
	InstanceAllocator buffer_allocator;
	Mutex			  buffer_mutex;

	InstanceAllocator image_allocator;
	Mutex             image_mutex;

	InstanceAllocator sampler_allocator;
	Mutex             sampler_mutex;

	InstanceAllocator shader_allocator;
	Mutex             shader_mutex;

	InstanceAllocator render_pass_allocator;
	Mutex             render_pass_mutex;

	InstanceAllocator input_layout_state_allocator;
	Mutex             input_layout_state_mutex;

	InstanceAllocator blend_state_allocator;
	Mutex             blend_state_mutex;

	InstanceAllocator depth_stencil_state_allocator;
	Mutex             depth_stencil_state_mutex;

	InstanceAllocator rasterizer_state_allocator;
	Mutex             rasterizer_state_mutex;

	GraphicsAPI api;

} GraphicsDevice;

void graphics_swapchain_resize();

GraphicsDevice*	graphics_device_get();
PipelineState*	graphics_state_get();

// Shader utils

b8 graphics_shader_initialize();
b8 graphics_shader_close();

#ifdef __cplusplus
}
#endif
