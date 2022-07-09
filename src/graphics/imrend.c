#include "Hosebase/defines.h"

#if SV_GRAPHICS

#include "Hosebase/render_utils.h"
#include "Hosebase/asset_system.h"

typedef struct {
	v4 position;
	v2 texcoord;
	Color color;
	u32 padding;
} ImRendVertex;

typedef struct {
	v4 bounds;
	b8 additive;
} ImRendScissor;

typedef struct {
	Buffer buffer;
	GPUImage* render_target;
	GPUImage* depth_map;
	
	DynamicArray(m4) matrix_stack;
	DynamicArray(ImRendScissor) scissor_stack;
	
	m4 current_matrix;

	b8 depth_read, depth_write;
	
	struct {
		ImRendCamera current;
		b8 is_custom;
		m4 view_matrix;
		m4 projection_matrix;
	} camera;
		
	struct {

		GPUBuffer* cbuffer_primitive;
		GPUBuffer* cbuffer_mesh;
	    
	} gfx;
} ImRendState;

typedef struct {

	RenderPass* render_pass;
	RenderPass* render_pass_depth;

	BlendState* bs_transparent;

	DepthStencilState* dss_none;
	DepthStencilState* dss_write;
	DepthStencilState* dss_read;
	DepthStencilState* dss_read_write;

	Sampler* sampler_def_linear;
	Sampler* sampler_def_nearest;

} ImRendGfx;

typedef struct {
	ImRendState state[GraphicsLimit_CommandList];
	ImRendGfx gfx;

	Asset vs_primitive;
	Asset ps_primitive;
} ImRendData;

static ImRendData* imrend = NULL;
    
#define SV_IMREND() ImRendState* state = &imrend->state[cmd]

typedef enum {
	ImRendHeader_PushMatrix,
	ImRendHeader_PopMatrix,
	ImRendHeader_PushScissor,
	ImRendHeader_PopScissor,
	ImRendHeader_DepthTest,
	ImRendHeader_LineWidth,

	ImRendHeader_Camera,
	ImRendHeader_CustomCamera,

	ImRendHeader_DrawCall,
	ImRendHeader_CustomDraw,
} ImRendHeader;

typedef enum {
	ImRendDrawCall_Quad,
	ImRendDrawCall_Sprite,
	ImRendDrawCall_Triangle,
	ImRendDrawCall_Line,
	
	ImRendDrawCall_MeshWireframe,
	
	ImRendDrawCall_Text,
	ImRendDrawCall_TextArea,
} ImRendDrawCall;

b8 imrend_initialize()
{
	imrend = (ImRendData*)memory_allocate(sizeof(ImRendData));

	imrend->vs_primitive = asset_load_from_file("base_shaders/primitive_vs.hlsl", AssetPriority_KeepItLoading);
	imrend->ps_primitive = asset_load_from_file("base_shaders/primitive_ps.hlsl", AssetPriority_KeepItLoading);

	{
		AttachmentDesc att[3];
		att[0].load_op = AttachmentOperation_Load;
		att[0].store_op = AttachmentOperation_Store;
		att[0].stencil_load_op = AttachmentOperation_DontCare;
		att[0].stencil_store_op = AttachmentOperation_DontCare;
		att[0].format = Format_R32G32B32A32_FLOAT;
		att[0].initial_layout = GPUImageLayout_RenderTarget;
		att[0].layout = GPUImageLayout_RenderTarget;
		att[0].final_layout = GPUImageLayout_RenderTarget;
		att[0].type = AttachmentType_RenderTarget;

		att[1].load_op = AttachmentOperation_Load;
		att[1].store_op = AttachmentOperation_Store;
		att[1].stencil_load_op = AttachmentOperation_Load;
		att[1].stencil_store_op = AttachmentOperation_Store;
		att[1].format = Format_D24_UNORM_S8_UINT;
		att[1].initial_layout = GPUImageLayout_DepthStencil;
		att[1].layout = GPUImageLayout_DepthStencil;
		att[1].final_layout = GPUImageLayout_DepthStencil;
		att[1].type = AttachmentType_DepthStencil;

		RenderPassDesc desc;
		desc.attachments = att;
		desc.attachment_count = 2;

		SV_CHECK(graphics_renderpass_create(&imrend->gfx.render_pass_depth, &desc));

		desc.attachment_count = 1u;

		SV_CHECK(graphics_renderpass_create(&imrend->gfx.render_pass, &desc));		
	}
	{
		BlendAttachmentDesc att;
		att.blend_enabled = TRUE;
		att.src_color_blend_factor = BlendFactor_SrcAlpha;
		att.dst_color_blend_factor = BlendFactor_OneMinusSrcAlpha;
		att.color_blend_op = BlendOperation_Add;
		att.src_alpha_blend_factor = BlendFactor_One;
		att.dst_alpha_blend_factor = BlendFactor_Zero;
		att.alpha_blend_op = BlendOperation_Add;
		att.color_write_mask = ColorComponent_RGB;
		
		BlendStateDesc desc;
		desc.attachments = &att;
		desc.blend_constants = (v4){0.f, 0.f, 0.f, 0.f};
		desc.attachment_count = 1u;

		SV_CHECK(graphics_blendstate_create(&imrend->gfx.bs_transparent, &desc));
	}

	{
		DepthStencilStateDesc desc;
		desc.depth_compare_op = CompareOperation_Less;
		desc.stencil_test_enabled = FALSE;

		desc.depth_test_enabled = FALSE;
		desc.depth_write_enabled = FALSE;
		SV_CHECK(graphics_depthstencilstate_create(&imrend->gfx.dss_none, &desc));

		desc.depth_test_enabled = FALSE;
		desc.depth_write_enabled = TRUE;
		SV_CHECK(graphics_depthstencilstate_create(&imrend->gfx.dss_write, &desc));

		desc.depth_test_enabled = TRUE;
		desc.depth_write_enabled = FALSE;
		SV_CHECK(graphics_depthstencilstate_create(&imrend->gfx.dss_read, &desc));

		desc.depth_test_enabled = TRUE;
		desc.depth_write_enabled = TRUE;
		SV_CHECK(graphics_depthstencilstate_create(&imrend->gfx.dss_read_write, &desc));

		DepthStencilState* dss_none;
		DepthStencilState* dss_write;
		DepthStencilState* dss_read;
		DepthStencilState* dss_write_read;
	}

	{
		SamplerDesc desc;
		desc.address_mode_u = SamplerAddressMode_Wrap;
		desc.address_mode_v = SamplerAddressMode_Wrap;
		desc.address_mode_w = SamplerAddressMode_Wrap;
		desc.min_filter = SamplerFilter_Linear;
		desc.mag_filter = SamplerFilter_Linear;
		
		SV_CHECK(graphics_sampler_create(&imrend->gfx.sampler_def_linear, &desc));

		desc.min_filter = SamplerFilter_Nearest;
		desc.mag_filter = SamplerFilter_Nearest;
		SV_CHECK(graphics_sampler_create(&imrend->gfx.sampler_def_nearest, &desc));
	}

	foreach(i, GraphicsLimit_CommandList) {

		ImRendState* s = &imrend->state[i];

		s->buffer = buffer_init(2.f);
		s->matrix_stack = array_init(m4, 1.2f);
		s->scissor_stack = array_init(ImRendScissor, 1.2f);

		// Buffers
		{
			GPUBufferDesc desc;

			desc.buffer_type = GPUBufferType_Constant;
			desc.usage = ResourceUsage_Dynamic;
			desc.cpu_access = CPUAccess_Write;
			desc.size = sizeof(ImRendVertex) * 4u;
			desc.data = NULL;

			SV_CHECK(graphics_buffer_create(&s->gfx.cbuffer_primitive, &desc));

			desc.buffer_type = GPUBufferType_Constant;
			desc.usage = ResourceUsage_Dynamic;
			desc.cpu_access = CPUAccess_Write;
			desc.size = sizeof(m4) + sizeof(v4);
			desc.data = NULL;

			SV_CHECK(graphics_buffer_create(&s->gfx.cbuffer_mesh, &desc));
		}
	}

	return TRUE;
}

void imrend_close()
{
	if (imrend) {
	    
		ImRendGfx* gfx = &imrend->gfx;

		// Free gfx objects
		{
			graphics_destroy_struct(gfx, sizeof(ImRendGfx));

			foreach(i, GraphicsLimit_CommandList) {

				ImRendState* state = &imrend->state[i];
	    
				graphics_destroy_struct(&state->gfx, sizeof(state->gfx));
			}
		}

		asset_unload(&imrend->vs_primitive);
		asset_unload(&imrend->ps_primitive);

		memory_free(imrend);
	}
}

#define imrend_write(state, data) buffer_write_back(&(state)->buffer, &(data), sizeof(data))

SV_INLINE void imrend_write_buffer(ImRendState* state, const void* data, u32 size)
{
	imrend_write(state, size);
	buffer_write_back(&state->buffer, data, size);
}

SV_INLINE void imrend_write_text(ImRendState* state, const char* text)
{
	buffer_write_back(&state->buffer, text, string_size(string_validate(text)) + 1u);
}

void* _imrend_read(u8** it, u32 size)
{
	void* ptr = *it;
	*it += size;
	return ptr;
}

#define imrend_read(T, it) *(T*)_imrend_read(&it, sizeof(T))
#define imrend_read_text(it) (const char*)it; it += (string_size((const char*)it) + 1)

SV_INLINE void imrend_read_buffer(u8** it, const void** ptr, u32* size)
{
	*size = imrend_read(u32, *it);
	*ptr = *it;
	*it += *size;
}

SV_INLINE void update_current_matrix(ImRendState* state)
{
	m4 matrix = m4_identity();

	foreach(i, state->matrix_stack.size) {

		m4 m = *(m4*)array_get(&state->matrix_stack, i);
		
		matrix = m4_mul(matrix, m);
	}

	m4 vpm;

	if (state->camera.is_custom) {

		vpm = m4_mul(state->camera.view_matrix, state->camera.projection_matrix);
	}
	else {
			
		switch (state->camera.current)
		{
		case ImRendCamera_Normal:
			vpm = m4_mul(m4_scale(2.f, 2.f, 1.f), m4_translate(-1.f, -1.f, 0.f));
			break;

#if SV_EDITOR
		case ImRendCamera_Editor:
			vpm = dev.camera.view_projection_matrix;
			break;
#endif

		case ImRendCamera_Clip:
		default:
			vpm = m4_identity();
	    
		}
	}

	state->current_matrix = m4_mul(matrix, vpm);
}

SV_INLINE void update_current_scissor(ImRendState* state, CommandList cmd)
{
	v4 s0 = (v4){ 0.5f, 0.5f, 1.f, 1.f };

	u32 begin_index = 0u;

	if (state->scissor_stack.size) {
	    
		for (i32 i = state->scissor_stack.size - 1; i >= 0; --i) {

			ImRendScissor* scissor = (ImRendScissor*)array_get(&state->scissor_stack, i);

			if (!scissor->additive) {
				begin_index = i;
				break;
			}
		}

		for (u32 i = begin_index; i < state->scissor_stack.size; ++i) {

			ImRendScissor* scissor = (ImRendScissor*)array_get(&state->scissor_stack, i);
			v4 s1 = scissor->bounds;

			f32 min0 = s0.x - s0.z * 0.5f;
			f32 max0 = s0.x + s0.z * 0.5f;
	    
			f32 min1 = s1.x - s1.z * 0.5f;
			f32 max1 = s1.x + s1.z * 0.5f;
	    
			f32 min = SV_MAX(min0, min1);
			f32 max = SV_MIN(max0, max1);
		
			if (min >= max) {
				s0 = (v4){0.f, 0.f, 0.f, 0.f};
				break;
			}
	    
			s0.z = max - min;
			s0.x = min + s0.z * 0.5f;
	    
			min0 = s0.y - s0.w * 0.5f;
			max0 = s0.y + s0.w * 0.5f;
	    
			min1 = s1.y - s1.w * 0.5f;
			max1 = s1.y + s1.w * 0.5f;

			min = SV_MAX(min0, min1);
			max = SV_MIN(max0, max1);

			if (min >= max) {
				s0 = (v4){0.f, 0.f, 0.f, 0.f};
				break;
			}

			s0.w = max - min;
			s0.y = min + s0.w * 0.5f;
		}
	}

	const GPUImageInfo* info = graphics_image_info(state->render_target);
	
	Scissor s;
	s.width = (u32)(s0.z * (f32)info->width);
	s.height = (u32)(s0.w * (f32)info->height);
	s.x = (u32)(s0.x * (f32)info->width - s0.z * (f32)info->width * 0.5f);
	s.y = (u32)(s0.y * (f32)info->height - s0.w * (f32)info->height * 0.5f);

	graphics_scissor_set(s, 0u, cmd);
}
    
void imrend_begin_batch(GPUImage* render_target, GPUImage* depth_map, CommandList cmd)
{
	SV_IMREND();

	buffer_reset(&state->buffer);
	state->render_target = render_target;
	state->depth_map = depth_map;
}
    
void imrend_flush(CommandList cmd)
{
	SV_IMREND();
	ImRendGfx* gfx = &imrend->gfx;

	graphics_event_begin("Immediate Rendering", cmd);

	graphics_viewport_set_image(state->render_target, 0u, cmd);
	graphics_scissor_set_image(state->render_target, 0u, cmd);

	state->depth_read = FALSE;
	state->depth_write = FALSE;
	graphics_depthstencilstate_bind(imrend->gfx.dss_none, cmd);
	
	GPUImage* att[2];
	att[0] = state->render_target;
	att[1] = state->depth_map;

	RenderPass* render_pass = (state->depth_map == NULL) ? gfx->render_pass : gfx->render_pass_depth;

	graphics_renderpass_begin(render_pass, att, NULL, 1.f, 0u, cmd);

	state->current_matrix = m4_identity();
	array_reset(&state->matrix_stack);
	array_reset(&state->scissor_stack);
	state->camera.current = ImRendCamera_Clip;
	state->camera.is_custom = FALSE;

	u8* it = (u8*)state->buffer.data;
	u8* end = (u8*)state->buffer.data + state->buffer.size;

	while (it != end)
	{
		ImRendHeader header = imrend_read(ImRendHeader, it);
	    
		switch (header) {

		case ImRendHeader_PushMatrix:
		{
			m4 m = imrend_read(m4, it);
			array_push(&state->matrix_stack, m);
			update_current_matrix(state);
		}
		break;
		
		case ImRendHeader_PopMatrix:
		{
			array_pop(&state->matrix_stack);
			update_current_matrix(state);
		}
		break;

		case ImRendHeader_PushScissor:
		{
			ImRendScissor s;
			s.bounds = imrend_read(v4, it);
			s.additive = imrend_read(b8, it);
		
			array_push(&state->scissor_stack, s);
			update_current_scissor(state, cmd);
		}
		break;
	    
		case ImRendHeader_PopScissor:
		{
			array_pop(&state->scissor_stack);
			update_current_scissor(state, cmd);
		}
		break;

		case ImRendHeader_DepthTest:
		{
			b8 read = imrend_read(b8, it);
			b8 write = imrend_read(b8, it);

			if (read != state->depth_read || write != state->depth_write) {
				state->depth_read = read;
				state->depth_write = write;

				DepthStencilState* dss = imrend->gfx.dss_none;

				if (read && write) dss = imrend->gfx.dss_read_write;
				else if (read) dss = imrend->gfx.dss_read;
				else if (write) dss = imrend->gfx.dss_write;

				graphics_depthstencilstate_bind(dss, cmd);
			}
		}
		break;

		case ImRendHeader_LineWidth:
		{
			f32 width = imrend_read(f32, it);

			graphics_line_width_set(width, cmd);
		}
		break;

		case ImRendHeader_Camera:
		{
			state->camera.current = imrend_read(ImRendCamera, it);
			state->camera.is_custom = FALSE;
			update_current_matrix(state);
		}
		break;

		case ImRendHeader_CustomCamera:
		{
			state->camera.view_matrix = imrend_read(m4, it);
			state->camera.projection_matrix = imrend_read(m4, it);
			state->camera.is_custom = TRUE;
			update_current_matrix(state);
		}
		break;
		
		case ImRendHeader_DrawCall:
		{
			ImRendDrawCall draw_call = imrend_read(ImRendDrawCall, it);

			switch (draw_call) {

			case ImRendDrawCall_Quad:
			case ImRendDrawCall_Sprite:
			case ImRendDrawCall_Triangle:
			case ImRendDrawCall_Line:
			{
				graphics_resource_bind(ResourceType_ConstantBuffer, state->gfx.cbuffer_primitive, 0u, ShaderType_Vertex, cmd);

				graphics_blendstate_bind(gfx->bs_transparent, cmd);
				graphics_inputlayout_reset(0, cmd);
				graphics_rasterizer_set(FALSE, CullMode_None, TRUE, cmd);

				graphics_resource_bind(ResourceType_Sampler, gfx->sampler_def_nearest, 0u, ShaderType_Pixel, cmd);
		    
				graphics_shader_bind_asset(imrend->vs_primitive, cmd);
				graphics_shader_bind_asset(imrend->ps_primitive, cmd);

				if (draw_call == ImRendDrawCall_Quad || draw_call == ImRendDrawCall_Sprite) {

					graphics_topology_set(GraphicsTopology_TriangleStrip, cmd);

					GPUImageLayout layout = GPUImageLayout_ShaderResource;
					v3 position = imrend_read(v3, it);
					v2 size = imrend_read(v2, it);
					Color color = imrend_read(Color, it);
					GPUImage* image = get_white_image();
					v4 tc = (v4){ 0.f, 0.f, 1.f, 1.f };

					if (draw_call == ImRendDrawCall_Sprite) {
						image = imrend_read(GPUImage*, it);
						layout = imrend_read(GPUImageLayout, it);
						tc = imrend_read(v4, it);

						if (image == NULL)
							image = get_white_image();
							
						if (layout != GPUImageLayout_ShaderResource && layout != GPUImageLayout_DepthStencilReadOnly) {
								
							// TEMP
							graphics_renderpass_end(cmd);
								
							GPUBarrier barrier = gpu_barrier_image(image, layout, (layout == GPUImageLayout_DepthStencil) ? GPUImageLayout_DepthStencilReadOnly : GPUImageLayout_ShaderResource);
							graphics_barrier(&barrier, 1u, cmd);
								
							graphics_renderpass_begin(render_pass, att, NULL, 1.f, 0, cmd);
						}
					}

					m4 m = m4_mul(m4_scale(size.x, size.y, 1.f), m4_translate(position.x, position.y, position.z));

					m = m4_mul(m, state->current_matrix);

					v4 p0 = v4_transform(v4_set(-0.5f, 0.5f, 0.f, 1.f), m);
					v4 p1 = v4_transform(v4_set(0.5f, 0.5f, 0.f, 1.f), m);
					v4 p2 = v4_transform(v4_set(-0.5f, -0.5f, 0.f, 1.f), m);
					v4 p3 = v4_transform(v4_set(0.5f, -0.5f, 0.f, 1.f), m);
		    
					ImRendVertex vertices[4u];
					vertices[0u] = (ImRendVertex){ p0, (v2){tc.x, tc.y}, color };
					vertices[1u] = (ImRendVertex){ p1, (v2){tc.z, tc.y}, color };
					vertices[2u] = (ImRendVertex){ p2, (v2){tc.x, tc.w}, color };
					vertices[3u] = (ImRendVertex){ p3, (v2){tc.z, tc.w}, color };

					graphics_resource_bind(ResourceType_ShaderResource, image, 0u, ShaderType_Pixel, cmd);

					graphics_buffer_update(state->gfx.cbuffer_primitive, GPUBufferState_Constant, vertices, sizeof(ImRendVertex) * 4u, 0u, cmd);

					graphics_draw(4u, 1u, 0u, 0u, cmd);

					if (draw_call == ImRendDrawCall_Sprite && layout != GPUImageLayout_ShaderResource && layout != GPUImageLayout_DepthStencilReadOnly) {
						GPUBarrier barrier = gpu_barrier_image(image, (layout == GPUImageLayout_DepthStencil) ? GPUImageLayout_DepthStencilReadOnly : GPUImageLayout_ShaderResource, layout);
						// TEMP
						graphics_renderpass_end(cmd);
						graphics_barrier(&barrier, 1u, cmd);
						graphics_renderpass_begin(render_pass, att, NULL, 1.f, 0, cmd);
					}
				}
				else if (draw_call == ImRendDrawCall_Triangle) {

					graphics_topology_set(GraphicsTopology_TriangleStrip, cmd);
					
					v3 p0 = imrend_read(v3, it);
					v3 p1 = imrend_read(v3, it);
					v3 p2 = imrend_read(v3, it);
					Color color = imrend_read(Color, it);

					m4 m = state->current_matrix;

					v4 c0 = v4_transform(v3_to_v4(p0, 1.f), m);
					v4 c1 = v4_transform(v3_to_v4(p1, 1.f), m);
					v4 c2 = v4_transform(v3_to_v4(p2, 1.f), m);
		    
					ImRendVertex vertices[3u];
					vertices[0u] = (ImRendVertex){ c0, (v2){0.f, 0.f}, color };
					vertices[1u] = (ImRendVertex){ c1, (v2){0.f, 0.f}, color };
					vertices[2u] = (ImRendVertex){ c2, (v2){0.f, 0.f}, color };

					graphics_resource_bind(ResourceType_ShaderResource, get_white_image(), 0u, ShaderType_Pixel, cmd);

					graphics_buffer_update(state->gfx.cbuffer_primitive, GPUBufferState_Constant, vertices, sizeof(ImRendVertex) * 3u, 0u, cmd);

					graphics_draw(3u, 1u, 0u, 0u, cmd);
				}
				else if (draw_call == ImRendDrawCall_Line) {

					graphics_topology_set(GraphicsTopology_Lines, cmd);

					v3 p0 = imrend_read(v3, it);
					v3 p1 = imrend_read(v3, it);
					Color color = imrend_read(Color, it);

					m4 m = state->current_matrix;

					v4 c0 = v4_transform(v3_to_v4(p0, 1.f), m);
					v4 c1 = v4_transform(v3_to_v4(p1, 1.f), m);
		    
					ImRendVertex vertices[2u];
					vertices[0u] = (ImRendVertex){ c0, (v2){0.f, 0.f}, color };
					vertices[1u] = (ImRendVertex){ c1, (v2){0.f, 0.f}, color };

					graphics_resource_bind(ResourceType_ShaderResource, get_white_image(), 0u, ShaderType_Pixel, cmd);

					graphics_buffer_update(state->gfx.cbuffer_primitive, GPUBufferState_Constant, vertices, sizeof(ImRendVertex) * 2u, 0u, cmd);

					graphics_draw(3u, 1u, 0u, 0u, cmd);
				}
		    
			}break;

			/*
			case ImRendDrawCall_MeshWireframe:
			{
				graphics_shader_resource_bind(gfx->image_white, 0u, ShaderType_Pixel, cmd);
				graphics_inputlayoutstate_bind(gfx.ils_mesh, cmd);
				graphics_shader_bind(imgfx.vs_mesh_wireframe, cmd);
				graphics_shader_bind(imgfx.ps_primitive, cmd);
				graphics_topology_set(GraphicsTopology_Triangles, cmd);
				graphics_resource_bind(ResourceType_ConstantBuffer, state.gfx.cbuffer_mesh, 0u, ShaderType_Vertex, cmd);
				graphics_rasterizerstate_bind(gfx.rs_wireframe, cmd);
				graphics_blendstate_bind(gfx.bs_transparent, cmd);

				Mesh* mesh = imrend_read<Mesh*>(it);
				Color color = imrend_read<Color>(it);

				graphics_vertex_buffer_bind(mesh->vbuffer, 0u, 0u, cmd);
				graphics_index_buffer_bind(mesh->ibuffer, 0u, cmd);

				struct {
					XMMATRIX matrix;
					v4 color;
				} data;

				data.matrix = state.current_matrix;
				data.color = color_to_vec4(color);

				graphics_buffer_update(state.gfx.cbuffer_mesh, GPUBufferState_Constant, &data, sizeof(data), 0u, cmd);
				graphics_draw_indexed((u32)mesh->indices.size(), 1u, 0u, 0u, 0u, cmd);
			}
			break;
		
			
			case ImRendDrawCall_TextArea:
			{
				DrawTextAreaDesc desc;
				desc.text = (const char*)it;
				it += string_size(desc.text) + 1u;
				desc.max_width = imrend_read<f32>(it);
				desc.max_lines = imrend_read<u32>(it);
				desc.font_size = imrend_read<f32>(it);
				desc.aspect = imrend_read<f32>(it);
				desc.alignment = imrend_read<TextAlignment>(it);
				desc.line_offset = imrend_read<u32>(it);
				desc.bottom_top = imrend_read<b8>(it);
				desc.font = imrend_read<Font*>(it);
				desc.color = imrend_read<Color>(it);
				desc.transform_matrix = state.current_matrix;

				graphics_renderpass_end(cmd);
				draw_text_area(desc, cmd);
				graphics_renderpass_begin(gfx.renderpass_off, att, cmd);
		    
			}break;
			*/

			case ImRendDrawCall_Text:
			{
				DrawTextDesc desc;
				desc.render_target = state->render_target;
				desc.text = imrend_read_text(it);
				desc.flags = imrend_read(u64, it);
				desc.font = imrend_read(Font*, it);
				desc.max_lines = imrend_read(u32, it);
				desc.alignment = imrend_read(TextAlignment, it);
				desc.aspect = imrend_read(f32, it);
				desc.font_size = imrend_read(f32, it);
				desc.transform_matrix = m4_mul(imrend_read(m4, it), state->current_matrix);
				TextContext ctx = imrend_read(TextContext, it);
				desc.context = &ctx;
				desc.text_default_color = imrend_read(Color, it);
				desc.text_selected_color = imrend_read(Color, it);
				desc.selection_color = imrend_read(Color, it);
				desc.cursor_color = imrend_read(Color, it);
					
				graphics_renderpass_end(cmd);
				draw_text(&desc, cmd);
				graphics_renderpass_begin(render_pass, att, NULL, 1.f, 0, cmd);
		    
			}break;
		    
			}
		}
		break;
		
		case ImRendHeader_CustomDraw:
		{
			ImRendDrawCustomFn fn = imrend_read(ImRendDrawCustomFn, it);

			const void* data;
			u32 size;
			imrend_read_buffer(&it, &data, &size);

			graphics_renderpass_end(cmd);
			
			fn(data, cmd);

			graphics_renderpass_begin(render_pass, att, NULL, 1.f, 0, cmd);
		}
		break;
		
		}
	}

	assert(state->matrix_stack.size == 0);
	assert(state->scissor_stack.size == 0);

	graphics_renderpass_end(cmd);

	graphics_event_end(cmd);
}

void imrend_push_matrix(m4 matrix, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_PushMatrix;
	imrend_write(state, header);
	imrend_write(state, matrix);
}

void imrend_pop_matrix(CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_PopMatrix;
	imrend_write(state, header);
}

void imrend_push_scissor(f32 x, f32 y, f32 width, f32 height, b8 additive, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_PushScissor;
	imrend_write(state, header);
	v4 v = (v4){x, y, width, height};
	imrend_write(state, v);
	imrend_write(state, additive);
}
    
void imrend_pop_scissor(CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_PopScissor;
	imrend_write(state, header);
}

void imrend_depth_test(b8 read, b8 write, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_DepthTest;
	imrend_write(state, header);
	imrend_write(state, read);
	imrend_write(state, write);
}

void imrend_line_width(f32 line_width, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_LineWidth;
	imrend_write(state, header);
	imrend_write(state, line_width);
}

void imrend_camera(ImRendCamera camera, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_Camera;
	imrend_write(state, header);
	imrend_write(state, camera);
}

void imrend_camera_custom(m4 view_matrix, m4 projection_matrix, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_CustomCamera;
	imrend_write(state, header);
	imrend_write(state, view_matrix);
	imrend_write(state, projection_matrix);
}

void imrend_draw_quad(v3 position, v2 size, Color color, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_DrawCall;
	imrend_write(state, header);
	ImRendDrawCall draw_call = ImRendDrawCall_Quad;
	imrend_write(state, draw_call);

	imrend_write(state, position);
	imrend_write(state, size);
	imrend_write(state, color);
}

void imrend_draw_line(v3 p0, v3 p1, Color color, CommandList cmd)
{
	SV_IMREND();
	
	ImRendHeader header = ImRendHeader_DrawCall;
	imrend_write(state, header);
	ImRendDrawCall draw_call = ImRendDrawCall_Line;
	imrend_write(state, draw_call);

	imrend_write(state, p0);
	imrend_write(state, p1);
	imrend_write(state, color);
}

void imrend_draw_triangle(v3 p0, v3 p1, v3 p2, Color color, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_DrawCall;
	imrend_write(state, header);
	ImRendDrawCall draw_call = ImRendDrawCall_Triangle;
	imrend_write(state, draw_call);
	
	imrend_write(state, p0);
	imrend_write(state, p1);
	imrend_write(state, p2);
	imrend_write(state, color);
}

void imrend_draw_sprite(v3 position, v2 size, Color color, GPUImage* image, GPUImageLayout layout, v4 texcoord, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_DrawCall;
	imrend_write(state, header);
	ImRendDrawCall draw_call = ImRendDrawCall_Sprite;
	imrend_write(state, draw_call);

	imrend_write(state, position);
	imrend_write(state, size);
	imrend_write(state, color);
	imrend_write(state, image);
	imrend_write(state, layout);
	imrend_write(state, texcoord);
}

void imrend_draw_custom(ImRendDrawCustomFn fn, const void* data, u32 size, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_CustomDraw;
	imrend_write(state, header);

	imrend_write(state, fn);
	imrend_write_buffer(state, data, size);
}

/*void imrend_draw_mesh_wireframe(Mesh* mesh, Color color, CommandList cmd)
{
	if (mesh == NULL || mesh->vbuffer == NULL || mesh->ibuffer == NULL)
		return;
		
	SV_IMREND();

	imrend_write(state, ImRendHeader_DrawCall);
	imrend_write(state, ImRendDrawCall_MeshWireframe);
	
	imrend_write(state, mesh);
	imrend_write(state, color);
}*/

void imrend_draw_text(const ImRendDrawTextDesc* desc, CommandList cmd)
{
	SV_IMREND();

	ImRendHeader header = ImRendHeader_DrawCall;
	imrend_write(state, header);
	ImRendDrawCall draw_call = ImRendDrawCall_Text;
	imrend_write(state, draw_call);

	imrend_write_text(state, desc->text);
	imrend_write(state, desc->flags);
	imrend_write(state, desc->font);
	imrend_write(state, desc->max_lines);
	imrend_write(state, desc->alignment);
	imrend_write(state, desc->aspect);
	imrend_write(state, desc->font_size);
	imrend_write(state, desc->transform_matrix);
	imrend_write(state, *desc->context);
	imrend_write(state, desc->text_default_color);
	imrend_write(state, desc->text_selected_color);
	imrend_write(state, desc->selection_color);
	imrend_write(state, desc->cursor_color);
}


/*void imrend_draw_text_area(const char* text, size_t text_size, f32 max_width, u32 max_lines, f32 font_size, f32 aspect, TextAlignment alignment, u32 line_offset, b8 bottom_top, Font* font, Color color, CommandList cmd)
{
	SV_IMREND();
		
	imrend_write(state, ImRendHeader_DrawCall);
	imrend_write(state, ImRendDrawCall_TextArea);

	state.buffer.write_back(text, text_size);
	imrend_write(state, '\0');
		
	imrend_write(state, max_width);
	imrend_write(state, max_lines);
	imrend_write(state, font_size);
	imrend_write(state, aspect);
	imrend_write(state, alignment);
	imrend_write(state, line_offset);
	imrend_write(state, bottom_top);
	imrend_write(state, font);
	imrend_write(state, color);
	}*/

void imrend_draw_cube_wireframe(Color color, CommandList cmd)
{
	v3 p0 = (v3){ -0.5f,  0.5f,  0.5f };
	v3 p1 = (v3){  0.5f,  0.5f,  0.5f };
	v3 p2 = (v3){ -0.5f, -0.5f,  0.5f };
	v3 p3 = (v3){  0.5f, -0.5f,  0.5f };
	v3 p4 = (v3){ -0.5f,  0.5f, -0.5f };
	v3 p5 = (v3){  0.5f,  0.5f, -0.5f };
	v3 p6 = (v3){ -0.5f, -0.5f, -0.5f };
	v3 p7 = (v3){  0.5f, -0.5f, -0.5f };
			
	imrend_draw_line(p0, p1, color, cmd);
	imrend_draw_line(p1, p3, color, cmd);
	imrend_draw_line(p3, p2, color, cmd);
	imrend_draw_line(p0, p2, color, cmd);

	imrend_draw_line(p4, p5, color, cmd);
	imrend_draw_line(p5, p7, color, cmd);
	imrend_draw_line(p7, p6, color, cmd);
	imrend_draw_line(p4, p6, color, cmd);

	imrend_draw_line(p0, p4, color, cmd);
	imrend_draw_line(p1, p5, color, cmd);
	imrend_draw_line(p2, p6, color, cmd);
	imrend_draw_line(p3, p7, color, cmd);
}

void imrend_draw_sphere_wireframe(u32 vertical_segments, u32 horizontal_segments, Color color, CommandList cmd)
{
	foreach(x, horizontal_segments) {

		f32 a0 = (x + 1) * PI / (horizontal_segments + 1u);
		f32 a1 = (x + 2) * PI / (horizontal_segments + 1u);
			
		foreach(y, vertical_segments) {

			f32 b0 = (y + 0) * TAU / vertical_segments;
			f32 b1 = (y + 1) * TAU / vertical_segments;

			f32 sin_a0 = sinf(a0);
			f32 sin_a1 = sinf(a1);
			f32 sin_b0 = sinf(b0);
			f32 sin_b1 = sinf(b1);
			f32 cos_a0 = cosf(a0);
			f32 cos_a1 = cosf(a1);
			f32 cos_b0 = cosf(b0);
			f32 cos_b1 = cosf(b1);

			f32 x0 = sin_a0 * cos_b0 * 0.5f;
			f32 y0 = cos_a0 * 0.5f;
			f32 z0 = sin_a0 * sin_b0 * 0.5f;

			f32 x1 = sin_a1 * cos_b0 * 0.5f;
			f32 y1 = cos_a1 * 0.5f;
			f32 z1 = sin_a1 * sin_b0 * 0.5f;

			f32 x2 = sin_a0 * cos_b1 * 0.5f;
			f32 y2 = cos_a0 * 0.5f;
			f32 z2 = sin_a0 * sin_b1 * 0.5f;

			f32 x3 = sin_a1 * cos_b1 * 0.5f;
			f32 y3 = cos_a1 * 0.5f;
			f32 z3 = sin_a1 * sin_b1 * 0.5f;

			imrend_draw_line((v3){ x0, y0, z0 }, (v3){ x1, y1, z1 }, color, cmd);
			imrend_draw_line((v3){ x0, y0, z0 }, (v3){ x2, y2, z2 }, color, cmd);
			imrend_draw_line((v3){ x0, y0, z0 }, (v3){ x3, y3, z3 }, color, cmd);
		}
	}
}
    
void imrend_draw_orthographic_grip(v2 position, v2 offset, v2 size, v2 gridSize, Color color, CommandList cmd)
{
	v2 half_size = v2_div_scalar(size, 2.f);
	
	v2 begin = v2_sub(position, offset);
	begin = v2_sub(begin, half_size);
	
	v2 end = v2_add(begin, size);

	for (f32 y = (i32)(begin.y / gridSize.y) * gridSize.y; y < end.y; y += gridSize.y) {
		imrend_draw_line((v3){ begin.x + offset.x, y + offset.y, 0.f }, (v3){ end.x + offset.x, y + offset.y, 0.f }, color, cmd);
	}
	for (f32 x = (i32)(begin.x / gridSize.x) * gridSize.x; x < end.x; x += gridSize.x) {
		imrend_draw_line((v3){ x + offset.x, begin.y + offset.y, 0.f }, (v3){ x + offset.x, end.y + offset.y, 0.f }, color, cmd);
	}
}

#endif
