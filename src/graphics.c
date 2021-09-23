#include "Hosebase/graphics.h"

#if SV_GRAPHICS

#include "graphics_internal.h"

#include "vulkan/graphics_vulkan.h"

typedef struct {

	PipelineState       pipeline_state;
	GraphicsDevice      device;

	GraphicsState		def_graphics_state;
	ComputeState 		def_compute_state;

	InputLayoutState*	def_input_layout_state;
	BlendState*			def_blend_state;
	DepthStencilState*	def_depth_stencil_state;
	RasterizerState*	def_rasterizer_state;

	DynamicArray(GraphicsPrimitive*) primitives_to_destroy;
	Mutex      primitives_to_destroy_mutex;
	
} GraphicsData;
 
static GraphicsData* gfx = NULL;

b8 _graphics_initialize(const GraphicsInitializeDesc* desc)
{
	b8 res;

	gfx = memory_allocate(sizeof(GraphicsData));
	
	// Initialize API
	SV_LOG_INFO("Trying to initialize vulkan device\n");
	graphics_vulkan_device_prepare(&gfx->device);
	res = gfx->device.initialize(desc->validation);
	
	if (!res) {
		SV_LOG_ERROR("Can't initialize vulkan device\n");
		return FALSE;
	}
	else SV_LOG_INFO("Vulkan device initialized successfuly\n");

	gfx->device.buffer_mutex = mutex_create();
	gfx->device.image_mutex = mutex_create();
	gfx->device.sampler_mutex = mutex_create();
	gfx->device.shader_mutex = mutex_create();
	gfx->device.render_pass_mutex = mutex_create();
	gfx->device.input_layout_state_mutex = mutex_create();
	gfx->device.depth_stencil_state_mutex = mutex_create();
	gfx->device.blend_state_mutex = mutex_create();
	gfx->device.rasterizer_state_mutex = mutex_create();

	gfx->primitives_to_destroy_mutex = mutex_create();

	gfx->primitives_to_destroy = array_init(GraphicsPrimitive*, 2.f);

	// Create default states
	{
		InputLayoutStateDesc desc;
		desc.slot_count = 0u;
		desc.element_count = 0u;
		SV_CHECK(graphics_inputlayoutstate_create(&gfx->def_input_layout_state, &desc));
	}
	{
		BlendAttachmentDesc att;
		att.blend_enabled = FALSE;
		att.src_color_blend_factor = BlendFactor_One;
		att.dst_color_blend_factor = BlendFactor_One;
		att.color_blend_op = BlendOperation_Add;
		att.src_alpha_blend_factor = BlendFactor_One;
		att.dst_alpha_blend_factor = BlendFactor_One;
		att.alpha_blend_op = BlendOperation_Add;
		att.color_write_mask = ColorComponent_All;
			
		BlendStateDesc desc;
		desc.attachment_count = 1u;
		desc.attachments = &att;
		desc.blend_constants = v4_set(0.f, 0.f, 0.f, 0.f);

		SV_CHECK(graphics_blendstate_create(&gfx->def_blend_state, &desc));
	}
	{
		DepthStencilStateDesc desc;
		desc.depth_test_enabled = FALSE;
		desc.depth_write_enabled = FALSE;
		desc.depth_compare_op = CompareOperation_Always;
		desc.stencil_test_enabled = FALSE;
		desc.read_mask = 0xFF;
		desc.write_mask = 0xFF;
		SV_CHECK(graphics_depthstencilstate_create(&gfx->def_depth_stencil_state, &desc));
	}
	{
		RasterizerStateDesc desc;
		desc.wireframe = FALSE;
		desc.cull_mode = RasterizerCullMode_None;
		desc.clockwise = TRUE;
		SV_CHECK(graphics_rasterizerstate_create(&gfx->def_rasterizer_state, &desc));
	}
	
	// Graphic State
	memory_zero(&gfx->def_graphics_state, sizeof(GraphicsState));
	gfx->def_graphics_state.input_layout_state = gfx->def_input_layout_state;
	gfx->def_graphics_state.blend_state = gfx->def_blend_state;
	gfx->def_graphics_state.depth_stencil_state = gfx->def_depth_stencil_state;
	gfx->def_graphics_state.rasterizer_state = gfx->def_rasterizer_state;
	for (u32 i = 0; i < GraphicsLimit_Viewport; ++i) {
		gfx->def_graphics_state.viewports[i] = (Viewport){
			.x = 0.f,
			.y = 0.f,
			.width = 100.f,
			.height = 100.f,
			.min_depth = 0.f,
			.max_depth = 1.f
		};
	}
	gfx->def_graphics_state.viewport_count = GraphicsLimit_Viewport;
	for (u32 i = 0; i < GraphicsLimit_Scissor; ++i) {
		gfx->def_graphics_state.scissors[i] = (Scissor){
			.x = 0u,
			.y = 0u,
			.width = 100u,
			.height = 100u
		};
	}
	gfx->def_graphics_state.scissor_count = GraphicsLimit_Scissor;
	gfx->def_graphics_state.topology = GraphicsTopology_Triangles;
	gfx->def_graphics_state.stencil_reference = 0u;
	gfx->def_graphics_state.line_width = 1.f;
	for (u32 i = 0; i < GraphicsLimit_Attachment; ++i) {
		gfx->def_graphics_state.clear_colors[i] = v4_set(0.f, 0.f, 0.f, 1.f);
	}
	gfx->def_graphics_state.clear_depth = 1.f;
	gfx->def_graphics_state.clear_stencil = 0u;
	gfx->def_graphics_state.flags =
		GraphicsPipelineState_InputLayoutState |
		GraphicsPipelineState_BlendState |
		GraphicsPipelineState_DepthStencilState |
		GraphicsPipelineState_RasterizerState |
		GraphicsPipelineState_Viewport |
		GraphicsPipelineState_Scissor |
		GraphicsPipelineState_Topology |
		GraphicsPipelineState_StencilRef |
		GraphicsPipelineState_LineWidth |
		GraphicsPipelineState_RenderPass
		;

	SV_CHECK(graphics_shader_initialize());

	memory_zero(&gfx->def_compute_state, sizeof(ComputeState));

	return TRUE;
}

inline void destroy_unused_primitive(GraphicsPrimitive* primitive)
{
	// TODO
#if SV_GFX
	if (primitive.name.size())
		SV_LOG_WARNING("Destroing '%s'\n", primitive.name.c_str());
#endif
	
	gfx->device.destroy(primitive);
}

static void destroy_primitives();

void _graphics_close()
{
	if (gfx == NULL) return;
	
	graphics_destroy(gfx->def_blend_state);
	graphics_destroy(gfx->def_depth_stencil_state);
	//graphics_destroy(gfx->def_input_layout_state);
	graphics_destroy(gfx->def_rasterizer_state);
	
	destroy_primitives();
	
	mutex_lock(gfx->device.buffer_mutex);
	mutex_lock(gfx->device.image_mutex);
	mutex_lock(gfx->device.sampler_mutex);
	mutex_lock(gfx->device.shader_mutex);
	mutex_lock(gfx->device.render_pass_mutex);
	mutex_lock(gfx->device.input_layout_state_mutex);
	mutex_lock(gfx->device.blend_state_mutex);
	mutex_lock(gfx->device.depth_stencil_state_mutex);
	mutex_lock(gfx->device.rasterizer_state_mutex);

	u32 count;
	
	count = instance_allocator_size(&gfx->device.buffer_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed buffers\n", count);
		
		foreach_instance(it, &gfx->device.buffer_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}
			
	count = instance_allocator_size(&gfx->device.image_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed images\n", count);
		
		foreach_instance(it, &gfx->device.image_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}

	count = instance_allocator_size(&gfx->device.sampler_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed samplers\n", count);
		
		foreach_instance(it, &gfx->device.sampler_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}

	count = instance_allocator_size(&gfx->device.shader_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed shaders\n", count);
		
		foreach_instance(it, &gfx->device.shader_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}

	count = instance_allocator_size(&gfx->device.render_pass_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed render passes\n", count);
		
		foreach_instance(it, &gfx->device.render_pass_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}

	count = instance_allocator_size(&gfx->device.input_layout_state_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed input layout states\n", count);
		
		foreach_instance(it, &gfx->device.input_layout_state_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}

	count = instance_allocator_size(&gfx->device.blend_state_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed blend states\n", count);
		
		foreach_instance(it, &gfx->device.blend_state_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}

	count = instance_allocator_size(&gfx->device.depth_stencil_state_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed depth stencil states\n", count);
		
		foreach_instance(it, &gfx->device.depth_stencil_state_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}

	count = instance_allocator_size(&gfx->device.rasterizer_state_allocator);
	if (count) {
		SV_LOG_WARNING("There are %u unfreed rasterizer states\n", count);
		
		foreach_instance(it, &gfx->device.rasterizer_state_allocator) {
			destroy_unused_primitive((GraphicsPrimitive*)it.ptr);
		}
	}
	
	instance_allocator_close(&gfx->device.buffer_allocator);
	instance_allocator_close(&gfx->device.image_allocator);
	instance_allocator_close(&gfx->device.sampler_allocator);
	instance_allocator_close(&gfx->device.shader_allocator);
	instance_allocator_close(&gfx->device.input_layout_state_allocator);
	instance_allocator_close(&gfx->device.blend_state_allocator);
	instance_allocator_close(&gfx->device.depth_stencil_state_allocator);
	instance_allocator_close(&gfx->device.rasterizer_state_allocator);

	mutex_unlock(gfx->device.buffer_mutex);
	mutex_unlock(gfx->device.image_mutex);
	mutex_unlock(gfx->device.sampler_mutex);
	mutex_unlock(gfx->device.shader_mutex);
	mutex_unlock(gfx->device.render_pass_mutex);
	mutex_unlock(gfx->device.input_layout_state_mutex);
	mutex_unlock(gfx->device.blend_state_mutex);
	mutex_unlock(gfx->device.depth_stencil_state_mutex);
	mutex_unlock(gfx->device.rasterizer_state_mutex);
	
	gfx->device.close();

	graphics_shader_close();

	mutex_destroy(gfx->device.buffer_mutex);
	mutex_destroy(gfx->device.image_mutex);
	mutex_destroy(gfx->device.sampler_mutex);
	mutex_destroy(gfx->device.shader_mutex);
	mutex_destroy(gfx->device.render_pass_mutex);
	mutex_destroy(gfx->device.input_layout_state_mutex);
	mutex_destroy(gfx->device.depth_stencil_state_mutex);
	mutex_destroy(gfx->device.blend_state_mutex);
	mutex_destroy(gfx->device.rasterizer_state_mutex);

	mutex_destroy(gfx->primitives_to_destroy_mutex);

	array_close(&gfx->primitives_to_destroy);

	memory_free(gfx);
}

inline void destroy_graphics_primitive(GraphicsPrimitive* p)
{
	if (gfx == NULL)
	{
		SV_LOG_ERROR("Trying to destroy a graphics primitive after the system is closed");
		return;
	}
	
	gfx->device.destroy(p);
	
	switch (p->type)
	{
	case GraphicsPrimitiveType_Image:
	{
		mutex_lock(gfx->device.image_mutex);
		instance_allocator_destroy(&gfx->device.image_allocator, p);
		mutex_unlock(gfx->device.image_mutex);
		break;
	}
	case GraphicsPrimitiveType_Sampler:
	{
		mutex_lock(gfx->device.sampler_mutex);
		instance_allocator_destroy(&gfx->device.sampler_allocator, p);
		mutex_unlock(gfx->device.sampler_mutex);
		break;
	}
	case GraphicsPrimitiveType_Buffer:
	{
		mutex_lock(gfx->device.buffer_mutex);
		instance_allocator_destroy(&gfx->device.buffer_allocator, p);
		mutex_unlock(gfx->device.buffer_mutex);
		break;
	}
	case GraphicsPrimitiveType_Shader:
	{
		mutex_lock(gfx->device.shader_mutex);
		instance_allocator_destroy(&gfx->device.shader_allocator, p);
		mutex_unlock(gfx->device.shader_mutex);
		break;
	}
	case GraphicsPrimitiveType_RenderPass:
	{
		mutex_lock(gfx->device.render_pass_mutex);
		instance_allocator_destroy(&gfx->device.render_pass_allocator, p);
		mutex_unlock(gfx->device.render_pass_mutex);
		break;
	}
	case GraphicsPrimitiveType_InputLayoutState:
	{
		mutex_lock(gfx->device.input_layout_state_mutex);
		instance_allocator_destroy(&gfx->device.input_layout_state_allocator, p);
		mutex_unlock(gfx->device.input_layout_state_mutex);
		break;
	}
	case GraphicsPrimitiveType_BlendState:
	{
		mutex_lock(gfx->device.blend_state_mutex);
		instance_allocator_destroy(&gfx->device.blend_state_allocator, p);
		mutex_unlock(gfx->device.blend_state_mutex);
		break;
	}
	case GraphicsPrimitiveType_DepthStencilState:
	{
		mutex_lock(gfx->device.depth_stencil_state_mutex);
		instance_allocator_destroy(&gfx->device.depth_stencil_state_allocator, p);
		mutex_unlock(gfx->device.depth_stencil_state_mutex);
		break;
	}
	case GraphicsPrimitiveType_RasterizerState:
	{
		mutex_lock(gfx->device.rasterizer_state_mutex);
		instance_allocator_destroy(&gfx->device.rasterizer_state_allocator, p);
		mutex_unlock(gfx->device.rasterizer_state_mutex);
		break;
	}
	}
}

static void destroy_primitives()
{
	mutex_lock(gfx->primitives_to_destroy_mutex);
	
	foreach(i, gfx->primitives_to_destroy.size) {

		GraphicsPrimitive** primitive = array_get(&gfx->primitives_to_destroy, i);
		destroy_graphics_primitive(*primitive);
	}
	array_reset(&gfx->primitives_to_destroy);

	mutex_unlock(gfx->primitives_to_destroy_mutex);
}

void _graphics_begin()
{
	if (core.frame_count % 20u == 0u) {
		destroy_primitives();
	}
	
	gfx->pipeline_state.present_image = NULL;
	
	gfx->device.frame_begin();
}

void _graphics_end()
{
	gfx->device.frame_end();
}

void graphics_present_image(GPUImage* image, GPUImageLayout layout)
{
	gfx->pipeline_state.present_image = image;
	gfx->pipeline_state.present_image_layout = layout;
}

void graphics_swapchain_resize()
{
	gfx->device.swapchain_resize();
	SV_LOG_INFO("Swapchain resized\n");
}

PipelineState* graphics_state_get()
{
	return &gfx->pipeline_state;
}
GraphicsDevice* graphics_device_get()
{
	return &gfx->device;
}

/////////////////////////////////////// HASH FUNCTIONS ///////////////////////////////////////

u64 graphics_compute_hash_inputlayoutstate(const InputLayoutStateDesc* desc)
{
	if (desc == NULL) return 0u;
	
	u64 hash = 0u;
	hash = hash_combine(hash, desc->slot_count);

	for (u32 i = 0; i < desc->slot_count; ++i) {
		const InputSlotDesc* slot = desc->slots + i;
		hash = hash_combine(hash, slot->slot);
		hash = hash_combine(hash, slot->stride);
		hash = hash_combine(hash, (u64)(slot->instanced));
	}

	hash = hash_combine(hash, desc->element_count);

	for (u32 i = 0; i < desc->element_count; ++i) {
		const InputElementDesc* element = desc->elements + i;
		hash = hash_combine(hash, (u64)(element->format));
		hash = hash_combine(hash, element->input_slot);
		hash = hash_combine(hash, element->offset);
		hash = hash_combine(hash, hash_string(element->name));
	}

	return hash;
}
u64 graphics_compute_hash_blendstate(const BlendStateDesc* desc)
{
	if (desc == NULL) return 0u;

	u64 hash = 0u;
	hash = hash_combine(hash, (u64)((f64)desc->blend_constants.x * 2550.0));
	hash = hash_combine(hash, (u64)((f64)desc->blend_constants.y * 2550.0));
	hash = hash_combine(hash, (u64)((f64)desc->blend_constants.z * 2550.0));
	hash = hash_combine(hash, (u64)((f64)desc->blend_constants.w * 2550.0));
	hash = hash_combine(hash, desc->attachment_count);

	for (u32 i = 0; i < desc->attachment_count; ++i)
	{
		const BlendAttachmentDesc* att = desc->attachments + i;
		hash = hash_combine(hash, att->alpha_blend_op);
		hash = hash_combine(hash, att->blend_enabled ? 1u : 0u);
		hash = hash_combine(hash, att->color_blend_op);
		hash = hash_combine(hash, att->color_write_mask);
		hash = hash_combine(hash, att->dst_alpha_blend_factor);
		hash = hash_combine(hash, att->dst_color_blend_factor);
		hash = hash_combine(hash, att->src_alpha_blend_factor);
		hash = hash_combine(hash, att->src_color_blend_factor);
	}

	return hash;
}
u64 graphics_compute_hash_rasterizerstate(const RasterizerStateDesc* desc)
{
	if (desc == NULL) return 0u;

	u64 hash = 0u;
	hash = hash_combine(hash, desc->clockwise ? 1u : 0u);
	hash = hash_combine(hash, desc->cull_mode);
	hash = hash_combine(hash, desc->wireframe ? 1u : 0u);

	return hash;
}
u64 graphics_compute_hash_depthstencilstate(const DepthStencilStateDesc* desc)
{
	if (desc == NULL) return 0u;

	u64 hash = 0u;
	hash = hash_combine(hash, desc->depth_compare_op);
	hash = hash_combine(hash, desc->depth_test_enabled ? 1u : 0u);
	hash = hash_combine(hash, desc->depth_write_enabled ? 1u : 0u);
	hash = hash_combine(hash, desc->stencil_test_enabled ? 1u : 0u);
	hash = hash_combine(hash, desc->read_mask);
	hash = hash_combine(hash, desc->write_mask);
	hash = hash_combine(hash, desc->front.compare_op);
	hash = hash_combine(hash, desc->front.depth_fail_op);
	hash = hash_combine(hash, desc->front.fail_op);
	hash = hash_combine(hash, desc->front.pass_op);
	hash = hash_combine(hash, desc->back.compare_op);
	hash = hash_combine(hash, desc->back.depth_fail_op);
	hash = hash_combine(hash, desc->back.fail_op);
	hash = hash_combine(hash, desc->back.pass_op);

	return hash;
}

b8 graphics_format_has_stencil(Format format)
{
	if (format == Format_D24_UNORM_S8_UINT)
		return TRUE;
	else return FALSE;
}
u32 graphics_format_size(Format format)
{
	switch (format)
	{
	case Format_R32G32B32A32_FLOAT:
	case Format_R32G32B32A32_UINT:
	case Format_R32G32B32A32_SINT:
		return 16u;

	case Format_R32G32B32_FLOAT:
	case Format_R32G32B32_UINT:
	case Format_R32G32B32_SINT:
		return 12u;
	case Format_R16G16B16A16_FLOAT:
	case Format_R16G16B16A16_UNORM:
	case Format_R16G16B16A16_UINT:
	case Format_R16G16B16A16_SNORM:
	case Format_R16G16B16A16_SINT:
	case Format_R32G32_FLOAT:
	case Format_R32G32_UINT:
	case Format_R32G32_SINT:
		return 8u;

	case Format_R8G8B8A8_UNORM:
	case Format_R8G8B8A8_SRGB:
	case Format_R8G8B8A8_UINT:
	case Format_R8G8B8A8_SNORM:
	case Format_R8G8B8A8_SINT:
	case Format_R16G16_FLOAT:
	case Format_R16G16_UNORM:
	case Format_R16G16_UINT:
	case Format_R16G16_SNORM:
	case Format_R16G16_SINT:
	case Format_D32_FLOAT:
	case Format_R32_FLOAT:
	case Format_R32_UINT:
	case Format_R32_SINT:
	case Format_D24_UNORM_S8_UINT:
	case Format_B8G8R8A8_UNORM:
	case Format_B8G8R8A8_SRGB:
		return 4u;

	case Format_R8G8_UNORM:
	case Format_R8G8_UINT:
	case Format_R8G8_SNORM:
	case Format_R8G8_SINT:
	case Format_R16_FLOAT:
	case Format_D16_UNORM:
	case Format_R16_UNORM:
	case Format_R16_UINT:
	case Format_R16_SNORM:
	case Format_R16_SINT:
		return 2u;

	case Format_R8_UNORM:
	case Format_R8_UINT:
	case Format_R8_SNORM:
	case Format_R8_SINT:
		return 1u;

	case Format_Unknown:
		return 0u;

	case Format_BC1_UNORM:
	case Format_BC1_SRGB:
	case Format_BC2_UNORM:
	case Format_BC2_SRGB:
	case Format_BC3_UNORM:
	case Format_BC3_SRGB:
	case Format_BC4_UNORM:
	case Format_BC4_SNORM:
	case Format_BC5_UNORM:
	case Format_BC5_SNORM:
	default:
		SV_LOG_INFO("Unknown format size\n");
		return 0u;
	}
}

GraphicsAPI graphics_api()
{
	return GraphicsAPI_Vulkan;
}

////////////////////////////////////////// PRIMITIVES /////////////////////////////////////////

b8 graphics_buffer_create(GPUBuffer** buffer, const GPUBufferDesc* desc)
{
#if SV_GFX

	if (desc->usage == ResourceUsage_Static && desc->cpu_access & CPUAccess_Write) {
		SV_LOG_ERROR("Buffer with static usage can't have CPU access\n");
		return FALSE;
	}
#endif
		
	// Allocate memory
	{
		mutex_lock(gfx->device.buffer_mutex);
		*buffer = (GPUBuffer*)instance_allocator_create(&gfx->device.buffer_allocator);
		mutex_unlock(gfx->device.buffer_mutex);
	}
	// Create API primitive
	// TODO: Handle error
	gfx->device.create(GraphicsPrimitiveType_Buffer, desc, (GraphicsPrimitive*)*buffer);

	// Set parameters
	GPUBuffer* p = *buffer;
	p->primitive.type = GraphicsPrimitiveType_Buffer;
	p->info.buffer_type = desc->buffer_type;
	p->info.usage = desc->usage;
	p->info.size = desc->size;
	p->info.index_type = desc->index_type;
	p->info.cpu_access = desc->cpu_access;
	p->info.format = desc->format;

	return TRUE;
}

b8 graphics_shader_create(Shader** shader, const ShaderDesc* desc)
{
#if SV_GFX
#endif

	// Allocate memory
	{
		mutex_lock(gfx->device.shader_mutex);
		*shader = (Shader*)instance_allocator_create(&gfx->device.shader_allocator);
		mutex_unlock(gfx->device.shader_mutex);
	}

	// Create API primitive
	// TODO: Handle error
	gfx->device.create(GraphicsPrimitiveType_Shader, desc, (GraphicsPrimitive*)*shader);

	// Set parameters
	Shader* p = *shader;
	p->primitive.type = GraphicsPrimitiveType_Shader;
	p->info.shader_type = desc->shader_type;

	return TRUE;
}

b8 graphics_image_create(GPUImage** image, const GPUImageDesc* desc)
{
#if SV_GFX
#endif

	// Allocate memory
	{
		mutex_lock(gfx->device.image_mutex);
		*image = (GPUImage*)instance_allocator_create(&gfx->device.image_allocator);
		mutex_unlock(gfx->device.image_mutex);
	}

	// Create API primitive
	// TODO: Handle error
	gfx->device.create(GraphicsPrimitiveType_Image, desc, (GraphicsPrimitive*)*image);

	// Set parameters
	GPUImage* p = *image;
	p->primitive.type = GraphicsPrimitiveType_Image;
	p->info.format = desc->format;
	p->info.width = desc->width;
	p->info.height = desc->height;
	p->info.type = desc->type;

	return TRUE;
}
		
b8 graphics_sampler_create(Sampler** sampler, const SamplerDesc* desc)
{
#if SV_GFX
#endif

	// Allocate memory
	{
		mutex_lock(gfx->device.sampler_mutex);
		*sampler = (Sampler*)instance_allocator_create(&gfx->device.sampler_allocator);
		mutex_unlock(gfx->device.sampler_mutex);
	}

	// Create API primitive
	// TODO: Handle error
	gfx->device.create(GraphicsPrimitiveType_Sampler, desc, (GraphicsPrimitive*)*sampler);

	// Set parameters
	Sampler* p = *sampler;
	p->primitive.type = GraphicsPrimitiveType_Sampler;

	return TRUE;
}

b8 graphics_renderpass_create(RenderPass** renderPass, const RenderPassDesc* desc)
{
#if SV_GFX
#endif

	// Allocate memory
	{
		mutex_lock(gfx->device.render_pass_mutex);
		*renderPass = (RenderPass*)instance_allocator_create(&gfx->device.render_pass_allocator);
		mutex_unlock(gfx->device.render_pass_mutex);
	}

	// Create API primitive
	// TODO: Handle error
	gfx->device.create(GraphicsPrimitiveType_RenderPass, desc, (GraphicsPrimitive*)*renderPass);

	// Set parameters
	RenderPass* p = *renderPass;
	p->primitive.type = GraphicsPrimitiveType_RenderPass;
	p->info.depthstencil_attachment_index = desc->attachment_count;
	for (u32 i = 0; i < desc->attachment_count; ++i) {
		if (desc->attachments[i].type == AttachmentType_DepthStencil) {
			p->info.depthstencil_attachment_index = i;
			break;
		}
	}

	p->info.attachment_count = desc->attachment_count;
	
	foreach(i, desc->attachment_count) {

		p->info.attachments[i] = desc->attachments[i];
	}

	return TRUE;
}

b8 graphics_inputlayoutstate_create(InputLayoutState** inputLayoutState, const InputLayoutStateDesc* desc)
{
#if SV_GFX
#endif

	// Allocate memory
	{
		mutex_lock(gfx->device.input_layout_state_mutex);
		*inputLayoutState = (InputLayoutState*)instance_allocator_create(&gfx->device.input_layout_state_allocator);
		mutex_unlock(gfx->device.input_layout_state_mutex);
	}

	// Create API primitive
	// TODO: Handle error
	gfx->device.create(GraphicsPrimitiveType_InputLayoutState, desc, (GraphicsPrimitive*)*inputLayoutState);

	// Set parameters
	InputLayoutState* p = *inputLayoutState;
	p->primitive.type = GraphicsPrimitiveType_InputLayoutState;
		
	p->info.slot_count = desc->slot_count;
	p->info.element_count = desc->element_count;

	foreach(i, desc->slot_count) {
		p->info.slots[i] = desc->slots[i];
	}
	foreach(i, desc->element_count) {
		p->info.elements[i] = desc->elements[i];
	}

	return TRUE;
}

b8 graphics_blendstate_create(BlendState** blendState, const BlendStateDesc* desc)
{
#if SV_GFX
#endif

	// Allocate memory
	{
		mutex_lock(gfx->device.blend_state_mutex);
		*blendState = (BlendState*)instance_allocator_create(&gfx->device.blend_state_allocator);
		mutex_unlock(gfx->device.blend_state_mutex);
	}

	// Create API primitive
	// TODO: Handle error
	gfx->device.create(GraphicsPrimitiveType_BlendState, desc, (GraphicsPrimitive*)*blendState);

	// Set parameters
	BlendState* p = *blendState;
	p->primitive.type = GraphicsPrimitiveType_BlendState;
		
	p->info.blend_constants = desc->blend_constants;
	p->info.attachment_count = desc->attachment_count;

	foreach(i, desc->attachment_count) {
		p->info.attachments[i] = desc->attachments[i];
	}

	return TRUE;
}

b8 graphics_depthstencilstate_create(DepthStencilState** depthStencilState, const DepthStencilStateDesc* desc)
{
#if SV_GFX
#endif

	// Allocate memory
	{
		mutex_lock(gfx->device.depth_stencil_state_mutex);
		*depthStencilState = (DepthStencilState*)instance_allocator_create(&gfx->device.depth_stencil_state_allocator);
		mutex_unlock(gfx->device.depth_stencil_state_mutex);
	}

	// Create API primitive
	// TODO: Handle error
	gfx->device.create(GraphicsPrimitiveType_DepthStencilState, desc, (GraphicsPrimitive*)*depthStencilState);

	// Set parameters
	DepthStencilState* p = *depthStencilState;
	p->primitive.type = GraphicsPrimitiveType_DepthStencilState;
	memory_copy(&p->info, desc, sizeof(DepthStencilStateInfo));

	return TRUE;
}

b8 graphics_rasterizerstate_create(RasterizerState** rasterizerState, const RasterizerStateDesc* desc)
{
#if SV_GFX
#endif

	// Allocate memory
	{
		mutex_lock(gfx->device.rasterizer_state_mutex);
		*rasterizerState = (RasterizerState*)instance_allocator_create(&gfx->device.rasterizer_state_allocator);
		mutex_unlock(gfx->device.rasterizer_state_mutex);
	}

	// Create API primitive
	gfx->device.create(GraphicsPrimitiveType_RasterizerState, desc, (GraphicsPrimitive*)*rasterizerState);

	// Set parameters
	RasterizerState* p = *rasterizerState;
	p->primitive.type = GraphicsPrimitiveType_RasterizerState;
	memory_copy(&p->info, desc, sizeof(RasterizerStateInfo));

	return TRUE;
}

void graphics_destroy(void* primitive)
{
	if (primitive == NULL) return;
	mutex_lock(gfx->primitives_to_destroy_mutex);
	GraphicsPrimitive* p = primitive;
	array_push(&gfx->primitives_to_destroy, p);
	mutex_unlock(gfx->primitives_to_destroy_mutex);
}

void graphics_destroy_struct(void* data, size_t size)
{
	assert(size % sizeof(void*) == 0u);

	u8* it = (u8*)data;
	u8* end = it + size;

	while (it != end) {

		void** primitive = (void**)it;
		graphics_destroy(*primitive);
		*primitive = NULL;

		it += sizeof(void*);
	}
}

CommandList graphics_commandlist_begin()
{
	CommandList cmd = gfx->device.commandlist_begin();

	gfx->pipeline_state.graphics[cmd] = gfx->def_graphics_state;
	gfx->pipeline_state.compute[cmd] = gfx->def_compute_state;

	return cmd;
}

CommandList graphics_commandlist_last()
{
	return gfx->device.commandlist_last();
}

CommandList graphics_commandlist_get()
{
	return (graphics_commandlist_count() != 0u) ? graphics_commandlist_last() : graphics_commandlist_begin();
}

u32 graphics_commandlist_count()
{
	return gfx->device.commandlist_count();
}

void graphics_gpu_wait()
{
	gfx->device.gpu_wait();
}

/////////////////////////////////////// RESOURCES ////////////////////////////////////////////////////////////

inline u64 get_resource_shader_flag(ShaderType shader_type)
{
	switch (shader_type)
	{
	case ShaderType_Vertex:
		return GraphicsPipelineState_Resource_VS;
	case ShaderType_Pixel:
		return GraphicsPipelineState_Resource_PS;
	case ShaderType_Geometry:
		return GraphicsPipelineState_Resource_GS;
	case ShaderType_Hull:
		return GraphicsPipelineState_Resource_HS;
	case ShaderType_Domain:
		return GraphicsPipelineState_Resource_DS;
	default:
		return 0;
	}
}

void graphics_resources_unbind(CommandList cmd)
{
	graphics_vertex_buffer_unbind_commandlist(cmd);
	graphics_index_buffer_unbind(cmd);
	graphics_constant_buffer_unbind_commandlist(cmd);
	graphics_shader_resource_unbind_commandlist(cmd);
	graphics_unordered_access_view_unbind_commandlist(cmd);
	graphics_sampler_unbind_commandlist(cmd);
}

void graphics_vertex_buffer_bind_array(GPUBuffer** buffers, u32* offsets, u32 count, u32 begin_slot, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->vertex_buffer_count = SV_MAX(state->vertex_buffer_count, begin_slot + count);

	memory_copy(state->vertex_buffers + begin_slot, buffers, sizeof(GPUBuffer*) * count);
	memory_copy(state->vertex_buffer_offsets + begin_slot, offsets, sizeof(u32) * count);
	state->flags |= GraphicsPipelineState_VertexBuffer;
}

void graphics_vertex_buffer_bind(GPUBuffer* buffer, u32 offset, u32 slot, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->vertex_buffer_count = SV_MAX(state->vertex_buffer_count, slot + 1u);
	state->vertex_buffers[slot] = buffer;
	state->vertex_buffer_offsets[slot] = offset;
	gfx->pipeline_state.graphics[cmd].flags |= GraphicsPipelineState_VertexBuffer;
}

void graphics_vertex_buffer_unbind(u32 slot, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->vertex_buffers[slot] = NULL;

	// Compute Vertex Buffer Count
	for (i32 i = (i32)(state->vertex_buffer_count) - 1; i >= 0; --i) {
		if (state->vertex_buffers[i] != NULL) {
			state->vertex_buffer_count = i + 1;
			break;
		}
	}

	gfx->pipeline_state.graphics[cmd].flags |= GraphicsPipelineState_VertexBuffer;
}

void graphics_vertex_buffer_unbind_commandlist(CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	memory_zero(state->vertex_buffers, state->vertex_buffer_count * sizeof(GPUBuffer*));
	state->vertex_buffer_count = 0u;

	gfx->pipeline_state.graphics[cmd].flags |= GraphicsPipelineState_VertexBuffer;
}

void graphics_index_buffer_bind(GPUBuffer* buffer, u32 offset, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->index_buffer = buffer;
	state->index_buffer_offset = offset;
	state->flags |= GraphicsPipelineState_IndexBuffer;
}

void graphics_index_buffer_unbind(CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->index_buffer = NULL;
	state->flags |= GraphicsPipelineState_IndexBuffer;
}

void graphics_constant_buffer_bind_array(GPUBuffer** buffers, u32 count, u32 begin_slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->constant_buffer_count = SV_MAX(state->constant_buffer_count, begin_slot + count);

		memory_copy(state->constant_buffers, buffers, sizeof(GPUBuffer*) * count);
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->constant_buffer_count[shader_type] = SV_MAX(state->constant_buffer_count[shader_type], begin_slot + count);

		memory_copy(state->constant_buffers[shader_type], buffers, sizeof(GPUBuffer*) * count);
		state->flags |= GraphicsPipelineState_ConstantBuffer;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_constant_buffer_bind(GPUBuffer* buffer, u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->constant_buffers[slot] = buffer;
		state->constant_buffer_count = SV_MAX(state->constant_buffer_count, slot + 1u);
		   
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->constant_buffers[shader_type][slot] = buffer;
		state->constant_buffer_count[shader_type] = SV_MAX(state->constant_buffer_count[shader_type], slot + 1u);
			
		state->flags |= GraphicsPipelineState_ConstantBuffer;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_constant_buffer_unbind(u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->constant_buffers[slot] = NULL;

		// Compute Constant Buffer Count
		u32* count = &state->constant_buffer_count;

		for (i32 i = (i32)(*count) - 1; i >= 0; --i) {
			if (state->constant_buffers[i] != NULL) {
				*count = i + 1;
				break;
			}
		}

		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
			
		state->constant_buffers[shader_type][slot] = NULL;

		// Compute Constant Buffer Count
		u32* count = &state->constant_buffer_count[shader_type];

		for (i32 i = (i32)(*count) - 1; i >= 0; --i) {
			if (state->constant_buffers[shader_type][i] != NULL) {
				*count = i + 1;
				break;
			}
		}

		state->flags |= GraphicsPipelineState_ConstantBuffer;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_constant_buffer_unbind_shader(ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];
		state->constant_buffer_count = 0u;
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->constant_buffer_count[shader_type] = 0u;
		state->flags |= GraphicsPipelineState_ConstantBuffer;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_constant_buffer_unbind_commandlist(CommandList cmd)
{
	{
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		for (u32 i = 0; i < ShaderType_GraphicsCount; ++i) {
			state->constant_buffer_count[i] = 0u;
			state->flags |= get_resource_shader_flag((ShaderType)i);
		}
		state->flags |= GraphicsPipelineState_ConstantBuffer;
	}
	{
		ComputeState* state = &gfx->pipeline_state.compute[cmd];
		state->constant_buffer_count = 0u;
		state->update_resources = TRUE;
	}
}

void graphics_shader_resource_bind_array(GPUImage** images, u32 count, u32 begin_slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->shader_resource_count = SV_MAX(state->shader_resource_count, begin_slot + count);
		
		memory_copy(state->shader_resources + begin_slot, images, sizeof(GPUImage*) * count);
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->shader_resource_count[shader_type] = SV_MAX(state->shader_resource_count[shader_type], begin_slot + count);

		memory_copy(state->shader_resources[shader_type] + begin_slot, images, sizeof(GPUImage*) * count);
		state->flags |= GraphicsPipelineState_ShaderResource;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_shader_resource_bind_image(GPUImage* image, u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->shader_resources[slot] = image;
		state->shader_resource_count = SV_MAX(state->shader_resource_count, slot + 1u);
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->shader_resources[shader_type][slot] = image;
		state->shader_resource_count[shader_type] = SV_MAX(state->shader_resource_count[shader_type], slot + 1u);
		state->flags |= GraphicsPipelineState_ShaderResource;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_shader_resource_bind_buffer_array(GPUBuffer** buffers, u32 count, u32 begin_slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->shader_resource_count = SV_MAX(state->shader_resource_count, begin_slot + count);

		memory_copy(state->shader_resources + begin_slot, buffers, sizeof(GPUBuffer*) * count);
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->shader_resource_count[shader_type] = SV_MAX(state->shader_resource_count[shader_type], begin_slot + count);

		memory_copy(state->shader_resources[shader_type] + begin_slot, buffers, sizeof(GPUBuffer*) * count);
		state->flags |= GraphicsPipelineState_ShaderResource;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_shader_resource_bind_buffer(GPUBuffer* buffer, u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->shader_resources[slot] = buffer;
		state->shader_resource_count = SV_MAX(state->shader_resource_count, slot + 1u);
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->shader_resources[shader_type][slot] = buffer;
		state->shader_resource_count[shader_type] = SV_MAX(state->shader_resource_count[shader_type], slot + 1u);
		state->flags |= GraphicsPipelineState_ShaderResource;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_shader_resource_unbind(u32 slot, ShaderType shader_type, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->shader_resources[shader_type][slot] = NULL;

	// Compute Images Count
	u32* count = &state->shader_resource_count[shader_type];

	for (i32 i = (i32)(*count) - 1; i >= 0; --i) {
		if (state->shader_resources[shader_type][i] != NULL) {
			*count = i + 1;
			break;
		}
	}

	state->flags |= GraphicsPipelineState_ShaderResource;
	state->flags |= get_resource_shader_flag(shader_type);
}

void graphics_shader_resource_unbind_shader(ShaderType shader_type, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	memory_zero(state->shader_resources[shader_type], state->shader_resource_count[shader_type] * sizeof(GPUImage*));
	state->shader_resource_count[shader_type] = 0u;

	state->flags |= GraphicsPipelineState_ShaderResource;
	state->flags |= get_resource_shader_flag(shader_type);
}
	
void graphics_shader_resource_unbind_commandlist(CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	for (u32 i = 0; i < ShaderType_GraphicsCount; ++i) {
		state->shader_resource_count[i] = 0u;
		state->flags |= get_resource_shader_flag((ShaderType)i);
	}
	state->flags |= GraphicsPipelineState_ShaderResource;
}

void graphics_unordered_access_view_bind_buffer_array(GPUBuffer** buffers, u32 count, u32 begin_slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->unordered_access_view_count = SV_MAX(state->unordered_access_view_count, begin_slot + count);

		memory_copy(state->unordered_access_views, buffers, sizeof(GPUBuffer*) * count);
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->unordered_access_view_count[shader_type] = SV_MAX(state->unordered_access_view_count[shader_type], begin_slot + count);

		memory_copy(state->unordered_access_views[shader_type], buffers, sizeof(GPUBuffer*) * count);
		state->flags |= GraphicsPipelineState_UnorderedAccessView;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_unordered_access_view_bind_buffer(GPUBuffer* buffer, u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->unordered_access_views[slot] = buffer;
		state->unordered_access_view_count = SV_MAX(state->unordered_access_view_count, slot + 1u);
		   
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->unordered_access_views[shader_type][slot] = buffer;
		state->unordered_access_view_count[shader_type] = SV_MAX(state->unordered_access_view_count[shader_type], slot + 1u);
			
		state->flags |= GraphicsPipelineState_UnorderedAccessView;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_unordered_access_view_bind_image_array(GPUImage** images, u32 count, u32 begin_slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->unordered_access_view_count = SV_MAX(state->unordered_access_view_count, begin_slot + count);

		memory_copy(state->unordered_access_views, images, sizeof(GPUImage*) * count);
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->unordered_access_view_count[shader_type] = SV_MAX(state->unordered_access_view_count[shader_type], begin_slot + count);

		memory_copy(state->unordered_access_views[shader_type], images, sizeof(GPUImage*) * count);
		state->flags |= GraphicsPipelineState_UnorderedAccessView;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_unordered_access_view_bind_image(GPUImage* image, u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->unordered_access_views[slot] = image;
		state->unordered_access_view_count = SV_MAX(state->unordered_access_view_count, slot + 1u);
		   
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->unordered_access_views[shader_type][slot] = image;
		state->unordered_access_view_count[shader_type] = SV_MAX(state->unordered_access_view_count[shader_type], slot + 1u);
			
		state->flags |= GraphicsPipelineState_UnorderedAccessView;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_unordered_access_view_unbind(u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->unordered_access_views[slot] = NULL;

		// Compute Constant Buffer Count
		u32* count = &state->unordered_access_view_count;

		for (i32 i = (i32)(*count) - 1; i >= 0; --i) {
			if (state->unordered_access_views[i] != NULL) {
				*count = i + 1;
				break;
			}
		}

		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
			
		state->unordered_access_views[shader_type][slot] = NULL;

		// Compute Constant Buffer Count
		u32* count = &state->unordered_access_view_count[shader_type];

		for (i32 i = (i32)(*count) - 1; i >= 0; --i) {
			if (state->unordered_access_views[shader_type][i] != NULL) {
				*count = i + 1;
				break;
			}
		}

		state->flags |= GraphicsPipelineState_UnorderedAccessView;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_unordered_access_view_unbind_shader(ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {
			
		ComputeState* state = &gfx->pipeline_state.compute[cmd];
		state->unordered_access_view_count = 0u;
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->unordered_access_view_count[shader_type] = 0u;
		state->flags |= GraphicsPipelineState_UnorderedAccessView;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_unordered_access_view_unbind_commandlist(CommandList cmd)
{
	{
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		for (u32 i = 0; i < ShaderType_GraphicsCount; ++i) {
			state->unordered_access_view_count[i] = 0u;
			state->flags |= get_resource_shader_flag((ShaderType)i);
		}
		state->flags |= GraphicsPipelineState_UnorderedAccessView;
	}
	{
		ComputeState* state = &gfx->pipeline_state.compute[cmd];
		state->unordered_access_view_count = 0u;
		state->update_resources = TRUE;
	}
}

void graphics_sampler_bind_array(Sampler** samplers, u32 count, u32 begin_slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {

		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->sampler_count = SV_MAX(state->sampler_count, begin_slot + count);

		memory_copy(state->samplers, samplers, sizeof(Sampler*) * count);
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		state->sampler_count[shader_type] = SV_MAX(state->sampler_count[shader_type], begin_slot + count);

		memory_copy(state->samplers, samplers, sizeof(Sampler*) * count);
		state->flags |= GraphicsPipelineState_Sampler;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_sampler_bind(Sampler* sampler, u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {

		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->samplers[slot] = sampler;
		state->sampler_count = SV_MAX(state->sampler_count, slot + 1u);
		   
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
			
		state->samplers[shader_type][slot] = sampler;
		state->sampler_count[shader_type] = SV_MAX(state->sampler_count[shader_type], slot + 1u);
		state->flags |= GraphicsPipelineState_Sampler;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_sampler_unbind(u32 slot, ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {

		ComputeState* state = &gfx->pipeline_state.compute[cmd];

		state->samplers[slot] = NULL;

		// Compute Constant Buffer Count
		u32* count = &state->sampler_count;

		for (i32 i = (i32)(*count) - 1; i >= 0; --i) {
			if (state->samplers[i] != NULL) {
				*count = i + 1;
				break;
			}
		}

		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
			
		state->samplers[shader_type][slot] = NULL;
			
		// Compute Images Count
		u32* count = &state->sampler_count[shader_type];
			
		for (i32 i = (i32)(*count) - 1; i >= 0; --i) {
			if (state->samplers[shader_type][i] != NULL) {
				*count = i + 1;
				break;
			}
		}

		state->flags |= GraphicsPipelineState_Sampler;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_sampler_unbind_shader(ShaderType shader_type, CommandList cmd)
{
	if (shader_type == ShaderType_Compute) {

		ComputeState* state = &gfx->pipeline_state.compute[cmd];
		state->sampler_count = 0u;
		state->update_resources = TRUE;
	}
	else {
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
			
		state->sampler_count[shader_type] = 0u;
			
		state->flags |= GraphicsPipelineState_Sampler;
		state->flags |= get_resource_shader_flag(shader_type);
	}
}

void graphics_sampler_unbind_commandlist(CommandList cmd)
{
	{
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		for (u32 i = 0; i < ShaderType_GraphicsCount; ++i) {
			state->sampler_count[i] = 0u;
			state->flags |= get_resource_shader_flag((ShaderType)i);
		}
		state->flags |= GraphicsPipelineState_Sampler;
	}
	{
		ComputeState* state = &gfx->pipeline_state.compute[cmd];
		state->sampler_count = 0u;
		state->update_resources = TRUE;
	}
}

////////////////////////////////////////////// STATE //////////////////////////////////////////////////

void graphics_state_unbind(CommandList cmd)
{
	if (gfx->pipeline_state.graphics[cmd].render_pass) graphics_renderpass_end(cmd);
		
	graphics_shader_unbind_commandlist(cmd);
	graphics_inputlayoutstate_unbind(cmd);
	graphics_blendstate_unbind(cmd);
	graphics_depthstencilstate_unbind(cmd);
	graphics_rasterizerstate_unbind(cmd);

	graphics_stencil_reference_set(0u, cmd);
	graphics_line_width_set(1.f, cmd);
}

void graphics_shader_bind(Shader* shader, CommandList cmd)
{
	if (shader->info.shader_type == ShaderType_Compute) {

		ComputeState* state = &gfx->pipeline_state.compute[cmd];
			
		state->compute_shader = shader;
		//state->flags |= GraphicsPipelineState_Shader_CS;
	}
	else {
			
		GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

		switch (shader->info.shader_type)
		{
		case ShaderType_Vertex:
			state->vertex_shader = shader;
			state->flags |= GraphicsPipelineState_Shader_VS;
			break;
		case ShaderType_Pixel:
			state->pixel_shader = shader;
			state->flags |= GraphicsPipelineState_Shader_PS;
			break;
		case ShaderType_Geometry:
			state->geometry_shader = shader;
			state->flags |= GraphicsPipelineState_Shader_GS;
			break;
		}

		state->flags |= GraphicsPipelineState_Shader;
	}
}

void graphics_inputlayoutstate_bind(InputLayoutState* input_layout_state, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
	state->input_layout_state = input_layout_state;
	state->flags |= GraphicsPipelineState_InputLayoutState;
}

void graphics_blendstate_bind(BlendState* blend_state, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
	state->blend_state = blend_state;
	state->flags |= GraphicsPipelineState_BlendState;
}

void graphics_depthstencilstate_bind(DepthStencilState* depth_stencil_state, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
	state->depth_stencil_state = depth_stencil_state;
	state->flags |= GraphicsPipelineState_DepthStencilState;
}

void graphics_rasterizerstate_bind(RasterizerState* rasterizer_state, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
	state->rasterizer_state = rasterizer_state;
	state->flags |= GraphicsPipelineState_RasterizerState;
}

void graphics_shader_unbind(ShaderType shader_type, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	switch (shader_type)
	{
	case ShaderType_Vertex:
		state->vertex_shader = NULL;
		state->flags |= GraphicsPipelineState_Shader_VS;
		break;
	case ShaderType_Pixel:
		state->pixel_shader = NULL;
		state->flags |= GraphicsPipelineState_Shader_PS;
		break;
	case ShaderType_Geometry:
		state->geometry_shader = NULL;
		state->flags |= GraphicsPipelineState_Shader_GS;
		break;
	}

	state->flags |= GraphicsPipelineState_Shader;
}

void graphics_shader_unbind_commandlist(CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	if (state->vertex_shader) {
		state->vertex_shader = NULL;
		state->flags |= GraphicsPipelineState_Shader_VS;
	}
	if (state->pixel_shader) {
		state->pixel_shader = NULL;
		state->flags |= GraphicsPipelineState_Shader_PS;
	}
	if (state->geometry_shader) {
		state->geometry_shader = NULL;
		state->flags |= GraphicsPipelineState_Shader_GS;
	}

	state->flags |= GraphicsPipelineState_Shader;
}

void graphics_inputlayoutstate_unbind(CommandList cmd)
{
	graphics_inputlayoutstate_bind(gfx->def_input_layout_state, cmd);
}

void graphics_blendstate_unbind(CommandList cmd)
{
	graphics_blendstate_bind(gfx->def_blend_state, cmd);
}

void graphics_depthstencilstate_unbind(CommandList cmd)
{
	graphics_depthstencilstate_bind(gfx->def_depth_stencil_state, cmd);
}

void graphics_rasterizerstate_unbind(CommandList cmd)
{
	graphics_rasterizerstate_bind(gfx->def_rasterizer_state, cmd);
}

void graphics_viewport_set_array(const Viewport* viewports, u32 count, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
	assert(count < GraphicsLimit_Viewport);
	memory_copy(state->viewports, viewports, (size_t)count * sizeof(Viewport));
	state->viewport_count = count;
	state->flags |= GraphicsPipelineState_Viewport;
}

void graphics_viewport_set(Viewport viewport, u32 slot, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->viewports[slot] = viewport;
	state->viewport_count = SV_MAX(gfx->pipeline_state.graphics[cmd].viewport_count, slot + 1u);
	state->flags |= GraphicsPipelineState_Viewport;
}

void graphics_viewport_set_image(GPUImage* image, u32 slot, CommandList cmd)
{
	graphics_viewport_set(graphics_image_viewport(image), slot, cmd);
}

void graphics_scissor_set_array(const Scissor* scissors, u32 count, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	assert(count < GraphicsLimit_Scissor);
	memory_copy(state->scissors, scissors, (size_t)count * sizeof(Scissor));
	state->scissor_count = count;
	state->flags |= GraphicsPipelineState_Scissor;
}

void graphics_scissor_set(Scissor scissor, u32 slot, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->scissors[slot] = scissor;
	state->scissor_count = SV_MAX(gfx->pipeline_state.graphics[cmd].scissor_count, slot + 1u);
	state->flags |= GraphicsPipelineState_Scissor;
}

void graphics_scissor_set_image(GPUImage* image, u32 slot, CommandList cmd)
{
	graphics_scissor_set(graphics_image_scissor(image), slot, cmd);
}

Viewport graphics_viewport_get(u32 slot, CommandList cmd)
{
	return gfx->pipeline_state.graphics[cmd].viewports[slot];
}

Scissor graphics_scissor_get(u32 slot, CommandList cmd)
{
	return gfx->pipeline_state.graphics[cmd].scissors[slot];
}

const GPUBufferInfo* graphics_buffer_info(const GPUBuffer* buffer)
{
	return &buffer->info;
}

const GPUImageInfo* graphics_image_info(const GPUImage* image)
{
	return &image->info;
}

const ShaderInfo* graphics_shader_info(const Shader* shader)
{
	return &shader->info;
}

const RenderPassInfo* graphics_renderpass_info(const RenderPass* renderpass)
{
	return &renderpass->info;
}

const SamplerInfo* graphics_sampler_info(const Sampler* sampler)
{
	return &sampler->info;
}

const InputLayoutStateInfo* graphics_inputlayoutstate_info(const InputLayoutState* ils)
{
	return &ils->info;
}

const BlendStateInfo* graphics_blendstate_info(const BlendState* bs)
{
	return &bs->info;
}

const RasterizerStateInfo* graphics_rasterizerstate_info(const RasterizerState* rs)
{
	return &rs->info;
}

const DepthStencilStateInfo* graphics_depthstencilstate_info(const DepthStencilState* dss)
{
	return &dss->info;
}

GPUImage* graphics_image_get(u32 slot, ShaderType shader_type, CommandList cmd)
{
	return gfx->pipeline_state.graphics[cmd].shader_resources[shader_type][slot];
}

void graphics_topology_set(GraphicsTopology topology, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
	state->topology = topology;
	state->flags |= GraphicsPipelineState_Topology;
}

void graphics_stencil_reference_set(u32 ref, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
	state->stencil_reference = ref;
	state->flags |= GraphicsPipelineState_StencilRef;
}

void graphics_line_width_set(float line_width, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];
	state->line_width = line_width;
	state->flags |= GraphicsPipelineState_LineWidth;
}

GraphicsTopology graphics_topology_get(CommandList cmd)
{
	return gfx->pipeline_state.graphics[cmd].topology;
}

u32 graphics_stencil_reference_get(CommandList cmd)
{
	return gfx->pipeline_state.graphics[cmd].stencil_reference;
}

float graphics_line_width_get(CommandList cmd)
{
	return gfx->pipeline_state.graphics[cmd].line_width;
}

void graphics_renderpass_begin(RenderPass* rp, GPUImage** attachments, const Color* colors, float depth, u32 stencil, CommandList cmd)
{
	GraphicsState* state = &gfx->pipeline_state.graphics[cmd];

	state->render_pass = rp;

	u32 att_count = rp->info.attachment_count;
	memory_copy(state->attachments, attachments, sizeof(GPUImage*) * att_count);
	gfx->pipeline_state.graphics[cmd].flags |= GraphicsPipelineState_RenderPass;

	if (colors != NULL) {
		foreach(i, rp->info.attachment_count)
			gfx->pipeline_state.graphics[cmd].clear_colors[i] = color_to_v4(colors[i]);
	}
	else {
		foreach(i, rp->info.attachment_count)
			gfx->pipeline_state.graphics[cmd].clear_colors[i] = color_to_v4(color_black());
	}
	gfx->pipeline_state.graphics[cmd].clear_depth = depth;
	gfx->pipeline_state.graphics[cmd].clear_stencil = stencil;

	gfx->device.renderpass_begin(cmd);
}
void graphics_renderpass_end(CommandList cmd)
{
	gfx->device.renderpass_end(cmd);
	gfx->pipeline_state.graphics[cmd].render_pass = NULL;
	gfx->pipeline_state.graphics[cmd].flags |= GraphicsPipelineState_RenderPass;
}

////////////////////////////////////////// DRAW CALLS /////////////////////////////////////////

void graphics_draw(u32 vertexCount, u32 instanceCount, u32 startVertex, u32 startInstance, CommandList cmd)
{
	gfx->device.draw(vertexCount, instanceCount, startVertex, startInstance, cmd);
}
void graphics_draw_indexed(u32 indexCount, u32 instanceCount, u32 startIndex, u32 startVertex, u32 startInstance, CommandList cmd)
{
	gfx->device.draw_indexed(indexCount, instanceCount, startIndex, startVertex, startInstance, cmd);
}

void graphics_dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z, CommandList cmd)
{
	gfx->device.dispatch(group_count_x, group_count_y, group_count_z, cmd);
}
void graphics_dispatch_image(GPUImage* image, u32 group_size_x, u32 group_size_y, CommandList cmd)
{
	const GPUImageInfo* info = &image->info;

	u32 x_group = (u32)math_truncate_high((f32)info->width / (f32)group_size_x);
	u32 y_group = (u32)math_truncate_high((f32)info->height / (f32)group_size_y);

	graphics_dispatch(x_group, y_group, 1, cmd);
}

////////////////////////////////////////// MEMORY /////////////////////////////////////////

void graphics_buffer_update(GPUBuffer* buffer, GPUBufferState buffer_state, const void* data, u32 size, u32 offset, CommandList cmd)
{
	gfx->device.buffer_update(buffer, buffer_state, data, size, offset, cmd);
}

void graphics_barrier(const GPUBarrier* barriers, u32 count, CommandList cmd)
{
	gfx->device.barrier(barriers, count, cmd);
}

void graphics_image_blit(GPUImage* src, GPUImage* dst, GPUImageLayout srcLayout, GPUImageLayout dstLayout, u32 count, const GPUImageBlit* imageBlit, SamplerFilter filter, CommandList cmd)
{
	gfx->device.image_blit(src, dst, srcLayout, dstLayout, count, imageBlit, filter, cmd);
}

void graphics_image_clear(GPUImage* image, GPUImageLayout oldLayout, GPUImageLayout newLayout, Color clearColor, float depth, u32 stencil, CommandList cmd)
{
	gfx->device.image_clear(image, oldLayout, newLayout, clearColor, depth, stencil, cmd);
}

b8 graphics_shader_include_write(const char* name, const char* str)
{
	char filepath[FILE_PATH_SIZE] = "library/shader_utils/";
	string_append(filepath, name, FILE_PATH_SIZE);
	string_append(filepath, ".hlsl", FILE_PATH_SIZE);
	
#if SV_GFX
	//TODO: if (std::filesystem::exists(filePath)) return TRUE;
#endif
	
	return file_write_text(filepath, str, string_size(str), FALSE, TRUE);
}

Viewport graphics_image_viewport(const GPUImage* i)
{
	return (Viewport){
		.x = 0.f,
		.y = 0.f,
		.width = (f32)(i->info.width),
		.height = (f32)(i->info.height),
		.min_depth = 0.f,
		.max_depth = 1.f
	};
}
Scissor graphics_image_scissor(const GPUImage* i)
{
	return (Scissor){
		.x = 0u,
		.y = 0u,
		.width = i->info.width,
		.height = i->info.height
	};
}

// DEBUG

#if SV_GFX

void graphics_event_begin(const char* name, CommandList cmd)
{
	gfx->device.event_begin(name, cmd);
}
void graphics_event_mark(const char* name, CommandList cmd)
{
	gfx->device.event_mark(name, cmd);
}
void graphics_event_end(CommandList cmd)
{
	gfx->device.event_end(cmd);
}

void graphics_name_set(Primitive* primitive_, const char* name)
{
	if (primitive_ == NULL) return;
	Primitive_internal& primitive = *reinterpret_cast<Primitive_internal*>(primitive_);
	primitive.name = name;
}

#endif

#endif