#include "Hosebase/render_utils.h"

#if SV_GRAPHICS

static const char* TEXT_VERTEX_SHADER = SV_STRING(
#include "core.hlsl"\n

	SV_CONSTANT_BUFFER(buffer, b0) {
	\n
		matrix tm;
}; \n

#shader vertex\n

struct Input {
	\n
		float2 position : Position; \n
		float2 texcoord : Texcoord; \n
		float4 color : Color; \n
}; \n

struct Output {
		\n
			float2 texcoord : FragTexcoord; \n
			float4 color : FragColor; \n
			float4 position : SV_Position; \n
	}; \n

		Output main(Input input)\n
		{ \n
			Output output;
			output.position = mul(float4(input.position, 0.f, 1.f), tm);
			output.color = input.color;
			output.texcoord = input.texcoord;
			return output;
		}\n

			);

static const char* TEXT_PIXEL_SHADER = SV_STRING(
#include "core.hlsl"\n

#shader pixel\n

	struct Input {\n
	float2 texcoord : FragTexcoord; \n
	float4 color : FragColor; \n
}; \n

SV_TEXTURE(tex, t0); \n
SV_SAMPLER(sam, s0); \n

float4 main(Input input) : SV_Target0\n
{ \n
	float4 color;
	f32 char_color = tex.Sample(sam, input.texcoord).r;
	color = char_color * input.color;
	if (color.a < 0.05f) discard;
	color = input.color;
	return color;
}\n
);

#define TEXT_BATCH_SIZE 1000

#pragma pack(push)
#pragma pack(1)

typedef struct {
	v2 position;
	v2 texcoord;
	Color color;
} TextVertex;

#pragma pack(pop)

typedef struct {

	RenderPass* render_pass_text;
	GPUBuffer* vbuffer_text;
	GPUBuffer* cbuffer_text;
	GPUBuffer* ibuffer_text;
	Shader* vs_text;
	Shader* ps_text;
	Sampler* sampler_text;
	InputLayoutState* ils_text;
	BlendState* bs_text;

	GPUImage* white_image;

	TextVertex batch_data[TEXT_BATCH_SIZE * 4];

} RenderUtilsData;

static RenderUtilsData* render = NULL;

b8 imrend_initialize();
void imrend_close();

b8 render_utils_initialize()
{
	render = memory_allocate(sizeof(RenderUtilsData));

	SV_CHECK(compile_shader(&render->vs_text, TEXT_VERTEX_SHADER));
	SV_CHECK(compile_shader(&render->ps_text, TEXT_PIXEL_SHADER));

	{
		GPUBufferDesc desc;
		desc.buffer_type = GPUBufferType_Vertex;
		desc.usage = ResourceUsage_Default;
		desc.cpu_access = CPUAccess_Write;
		desc.size = sizeof(TextVertex) * TEXT_BATCH_SIZE * 4u;
		desc.data = NULL;

		SV_CHECK(graphics_buffer_create(&render->vbuffer_text, &desc));

		desc.buffer_type = GPUBufferType_Constant;
		desc.usage = ResourceUsage_Default;
		desc.cpu_access = CPUAccess_Write;
		desc.size = sizeof(m4);
		desc.data = NULL;

		SV_CHECK(graphics_buffer_create(&render->cbuffer_text, &desc));

		u16 d[TEXT_BATCH_SIZE * 6u];

		foreach(i, TEXT_BATCH_SIZE) {

			u32 v = i * 4;
			u32 s = i * 6;

			d[s + 0] = v + 0;
			d[s + 1] = v + 1;
			d[s + 2] = v + 2;

			d[s + 3] = v + 1;
			d[s + 4] = v + 3;
			d[s + 5] = v + 2;
		}

		desc.usage = ResourceUsage_Static;
		desc.cpu_access = CPUAccess_None;
		desc.buffer_type = GPUBufferType_Index;
		desc.index_type = IndexType_16;
		desc.size = sizeof(u16) * TEXT_BATCH_SIZE * 6u;
		desc.data = d;

		SV_CHECK(graphics_buffer_create(&render->ibuffer_text, &desc));
	}
	{
		SamplerDesc desc;
		desc.address_mode_u = SamplerAddressMode_Wrap;
		desc.address_mode_v = SamplerAddressMode_Wrap;
		desc.address_mode_w = SamplerAddressMode_Wrap;
		desc.min_filter = SamplerFilter_Linear;
		desc.mag_filter = SamplerFilter_Linear;

		SV_CHECK(graphics_sampler_create(&render->sampler_text, &desc));
	}
	{
		InputElementDesc elements[3];
		elements[0].name = "Position";
		elements[0].index = 0u;
		elements[0].input_slot = 0u;
		elements[0].offset = 0u;
		elements[0].format = Format_R32G32_FLOAT;

		elements[1].name = "Texcoord";
		elements[1].index = 0u;
		elements[1].input_slot = 0u;
		elements[1].offset = 2 * sizeof(f32);
		elements[1].format = Format_R32G32_FLOAT;

		elements[2].name = "Color";
		elements[2].index = 0u;
		elements[2].input_slot = 0u;
		elements[2].offset = 4 * sizeof(f32);
		elements[2].format = Format_R8G8B8A8_UNORM;

		InputSlotDesc slot;
		slot.slot = 0;
		slot.stride = sizeof(TextVertex);
		slot.instanced = FALSE;

		InputLayoutStateDesc desc;
		desc.slot_count = 1u;
		desc.slots = &slot;
		desc.element_count = 3u;
		desc.elements = elements;
		SV_CHECK(graphics_inputlayoutstate_create(&render->ils_text, &desc));
	}
	{
		// TODO: Transparent
		BlendAttachmentDesc att[1];
		att[0].blend_enabled = FALSE;
		att[0].src_color_blend_factor = BlendFactor_One;
		att[0].dst_color_blend_factor = BlendFactor_One;
		att[0].color_blend_op = BlendOperation_Add;
		att[0].src_alpha_blend_factor = BlendFactor_One;
		att[0].dst_alpha_blend_factor = BlendFactor_One;
		att[0].alpha_blend_op = BlendOperation_Add;
		att[0].color_write_mask = ColorComponent_All;

		BlendStateDesc desc;
		desc.attachment_count = 1u;
		desc.attachments = att;
		desc.blend_constants = v4_set(0.f, 0.f, 0.f, 0.f);

		SV_CHECK(graphics_blendstate_create(&render->bs_text, &desc));
	}
	{
		AttachmentDesc att[1];
		att[0].load_op = AttachmentOperation_Load;
		att[0].store_op = AttachmentOperation_Store;
		att[0].format = Format_R32G32B32A32_FLOAT;
		att[0].initial_layout = GPUImageLayout_RenderTarget;
		att[0].layout = GPUImageLayout_RenderTarget;
		att[0].final_layout = GPUImageLayout_RenderTarget;
		att[0].type = AttachmentType_RenderTarget;

		RenderPassDesc desc;
		desc.attachments = att;
		desc.attachment_count = 1u;

		SV_CHECK(graphics_renderpass_create(&render->render_pass_text, &desc));
	}

	{
		GPUImageDesc desc;

		u8 bytes[4];
		foreach(i, 4) bytes[i] = 255u;

		desc.data = bytes;
		desc.size = sizeof(u8) * 4u;
		desc.format = Format_R8G8B8A8_UNORM;
		desc.layout = GPUImageLayout_ShaderResource;
		desc.type = GPUImageType_ShaderResource;
		desc.usage = ResourceUsage_Static;
		desc.cpu_access = CPUAccess_None;
		desc.width = 1u;
		desc.height = 1u;

		SV_CHECK(graphics_image_create(&render->white_image, &desc));
	}

	SV_CHECK(imrend_initialize());

	return TRUE;
}

void render_utils_close()
{
	if (render) {

		imrend_close();

		graphics_destroy(render->render_pass_text);
		graphics_destroy(render->vbuffer_text);
		graphics_destroy(render->cbuffer_text);
		graphics_destroy(render->ibuffer_text);
		graphics_destroy(render->vs_text);
		graphics_destroy(render->ps_text);
		graphics_destroy(render->sampler_text);
		graphics_destroy(render->ils_text);
		graphics_destroy(render->bs_text);

		graphics_destroy(render->white_image);

		memory_free(render);
	}
}

inline void draw_text_batch(GPUImage* image, TextAlignment alignment, f32 font_size, u32 batch_count, u64 flags, CommandList cmd)
{
	if (batch_count < 4) return;

	/*if (flags & DrawTextFlag_BottomTop) {

		f32 add = (bounds.y - bounds.w * 0.5f) - min_height;

		foreach(i, batch_count)
			render->batch_data[i].position.y += add;
	}*/

	if (alignment != TextAlignment_Left) {

		f32 current_height = render->batch_data[0].position.y;
		f32 begin_x = render->batch_data[0].position.x;
		u32 begin = 0;

		for (u32 i = 0; i < batch_count / 4; ++i) {

			u32 j = i * 4 + 1;

			f32 y = render->batch_data[j].position.y;
			if (current_height - y >= font_size * 0.7f) {

				f32 add;

				if (alignment == TextAlignment_Center) {
					f32 pos = begin_x + (render->batch_data[j].position.x - begin_x) * 0.5f;
					add = 0.5f - pos;
				}
				else {
					f32 pos = render->batch_data[j].position.x;
					add = 1.f - pos;
				}

				for (u32 w = begin; w <= j + 3; ++w) {
					render->batch_data[w].position.x += add;
				}

				current_height = y;
				begin = j - 1;
				begin_x = render->batch_data[j - 1].position.x;
			}
		}

		f32 add;

		if (alignment == TextAlignment_Center) {
			f32 pos = begin_x + (render->batch_data[batch_count - 1].position.x - begin_x) * 0.5f;
			add = 0.5f - pos;
		}
		else {
			f32 pos = render->batch_data[batch_count - 1].position.x;
			add = 1.f - pos;
		}

		for (u32 w = begin; w < batch_count; ++w) {
			render->batch_data[w].position.x += add;
		}
	}

	graphics_buffer_update(render->vbuffer_text, GPUBufferState_Vertex, render->batch_data, sizeof(TextVertex) * batch_count, 0, cmd);

	GPUImage* att[1];
	att[0] = image;

	graphics_renderpass_begin(render->render_pass_text, att, NULL, 1.f, 0, cmd);
	graphics_draw_indexed((batch_count / 4) * 6, 1, 0, 0, 0, cmd);
	graphics_renderpass_end(cmd);
}

void draw_text(const DrawTextDesc* desc, CommandList cmd)
{
	void* buffer = desc->text;

	char* text = (char*)buffer;

	TextContext* ctx = desc->context;

	i32 line_offset = ctx ? ctx->vertical_offset : 0;

	if (!(desc->flags & DrawTextFlag_BottomTop)) {
		text = (char*)text_jump_lines(buffer, line_offset);
	}

	if (text == NULL) return;

	Font* font = desc->font;
	GPUImage* render_target = desc->render_target;
	TextAlignment alignment = desc->alignment;

	u32 c0 = 0, c1 = 0;
	u32 c = 0;

	if (ctx) {

		c = ctx->cursor0;

		if (ctx->cursor0 < ctx->cursor1) {
			c0 = ctx->cursor0;
			c1 = ctx->cursor1;
		}
		else {
			c1 = ctx->cursor0;
			c0 = ctx->cursor1;
		}
	}

	u32 batch_count = 0u;

	f32 xmult = desc->font_size / desc->aspect;
	f32 ymult = desc->font_size;

	f32 line_height = ymult;

	f32 xoff = 0.f;
	f32 yoff = 1.f - line_height;

	f32 vertical_offset = font->vertical_offset * ymult;

	if (desc->flags & DrawTextFlag_LineCentred) {
		yoff += vertical_offset;
	}
	f32 min_height = 0.f;
	if (desc->flags & DrawTextFlag_BottomTop) {

		// TODO
		// u32 lines = SV_MAX((i32)text_lines(text) - line_offset, 0);
		// min_height = (bounds.y + bounds.w * 0.5f) - ((f32)(lines)-1.f) * line_height - vertical_offset;
	}

	// TODO: Draw selection
	/*if (ctx) {

		f32 cursor_width = text_cursor_width(font, line_height);
		v2 p0 = text_pos(buffer, c0, font, line_height);
		v2 p1 = text_pos(buffer, c1, font, line_height);

		if (c0 != c1) {

			// Same line
			if (fabs(p0.y - p1.y) < 0.00001f) {

				f32 width = p1.x - p0.x;

				renderer_draw_quad(xbounds + p0.x + width * 0.5f - cursor_width, ybounds + p0.y - vertical_offset - line_height * 0.5f, width, line_height, desc->selection_color, cmd);
			}
			else {

				v2 p = p0;
				const char* b = (const char*)buffer + c0;

				p.x -= char_width(' ', font, line_height);

				{
					b = text_advance_end(b, &p, font, line_height);

					f32 width = p.x - p0.x;
					renderer_draw_quad(xbounds + p0.x + width * 0.5f - cursor_width, ybounds + p0.y - vertical_offset - line_height * 0.5f, width, line_height, desc->selection_color, cmd);
				}

				b = text_advance_line(b, &p, font, line_height);

				while (fabs(p.y - p1.y) > 0.00001f) {

					v2 begin = p;
					v2 end = p;

					b = text_advance_end(b, &end, font, line_height);

					f32 width = end.x - begin.x;
					renderer_draw_quad(xbounds + begin.x + width * 0.5f - cursor_width, ybounds + begin.y - vertical_offset - line_height * 0.5f, width, line_height, desc->selection_color, cmd);
					p = end;

					b = text_advance_line(b, &p, font, line_height);
				}

				{
					v2 begin = p;
					v2 end = p1;

					f32 width = end.x - begin.x;
					renderer_draw_quad(xbounds + begin.x + width * 0.5f - cursor_width, ybounds + begin.y - vertical_offset - line_height * 0.5f, width, line_height, desc->selection_color, cmd);
				}
			}
		}

		v2 p = (c == c0) ? p0 : p1;
		renderer_draw_quad(xbounds + p.x - cursor_width * 0.5f, ybounds + p.y - vertical_offset - line_height * 0.5f, cursor_width, line_height, desc->cursor_color, cmd);
	}*/

	// Prepare gfx pipeline
	{
		graphics_topology_set(GraphicsTopology_Triangles, cmd);

		graphics_vertex_buffer_bind(render->vbuffer_text, 0, 0, cmd);
		graphics_index_buffer_bind(render->ibuffer_text, 0, cmd);

		graphics_inputlayoutstate_bind(render->ils_text, cmd);
		graphics_blendstate_bind(render->bs_text, cmd);
		graphics_rasterizerstate_unbind(cmd);
		graphics_depthstencilstate_unbind(cmd);

		graphics_constant_buffer_bind(render->cbuffer_text, 0u, ShaderType_Vertex, cmd);
		graphics_buffer_update(render->cbuffer_text, GPUBufferState_Constant, &desc->transform_matrix, sizeof(m4), 0, cmd);

		graphics_shader_resource_bind_image(font->image, 0u, ShaderType_Pixel, cmd);
		graphics_sampler_bind(render->sampler_text, 0u, ShaderType_Pixel, cmd);

		graphics_shader_bind(render->vs_text, cmd);
		graphics_shader_bind(render->ps_text, cmd);
	}

	Color format_color = desc->text_default_color;

	u32 line_count = 1;

	while (*text != '\0' && line_count <= desc->max_lines) {

		if (*text == '\n') {
			xoff = 0.f;
			yoff -= line_height;
			++text;
			++line_count;

			// TODO: Optimize for bottom - top
			/*if (!(desc->flags & DrawTextFlag_BottomTop) && yoff + ybounds + line_height < bounds.y - bounds.w * 0.5f)
				break;*/

			continue;
		}

		if (*text == '\t') {

			Glyph* g = font_get(font, ' ');
			if (g) xoff += g->advance * xmult * 4;
			assert(g);
		}

		// READ CHAR OR FORMAT

		Glyph* glyph = NULL;

		// Reading format
		if (*text == '~') {

			++text;

			if (*text != '\0') {

				b8 valid = TRUE;

				switch (*text) {

				case '\0':
					valid = FALSE;
					break;

				case 'c':
				{
					++text;

					foreach(i, 8) {
						if (*(text + 1) == '\0') {
							valid = FALSE;
							break;
						}
					}

					if (valid) {
						char str_value[3];
						u32 value;
						Color color;

						// Red
						str_value[0] = *text;
						++text;
						str_value[1] = *text;
						++text;
						str_value[2] = '\0';

						if (string_to_u32_hexadecimal(&value, str_value)) {
							color.r = (u8)value;
						}
						else valid = FALSE;

						// Green
						str_value[0] = *text;
						++text;
						str_value[1] = *text;
						++text;
						str_value[2] = '\0';

						if (string_to_u32_hexadecimal(&value, str_value)) {
							color.g = (u8)value;
						}
						else valid = FALSE;

						// Blue
						str_value[0] = *text;
						++text;
						str_value[1] = *text;
						++text;
						str_value[2] = '\0';

						if (string_to_u32_hexadecimal(&value, str_value)) {
							color.b = (u8)value;
						}
						else valid = FALSE;

						// Alpha
						str_value[0] = *text;
						++text;
						str_value[1] = *text;
						++text;
						str_value[2] = '\0';

						if (string_to_u32_hexadecimal(&value, str_value)) {
							color.a = (u8)value;
						}
						else valid = FALSE;

						if (valid) {
							format_color = color;
						}
						else {
							SV_LOG_ERROR("Invalid text format");
						}
					}
				}
				break;

				case '~':
				{
					glyph = font_get(font, '~');
				}
				break;

				}

				if (!valid) {
					break;
				}
			}
		}
		else glyph = font_get(font, *text);

		if (glyph == NULL) {
			++text;
			continue;
		}

		Color color = format_color;
		u32 cursor = text - (const char*)desc->text;

		if (ctx) {

			if (c == c0 && c0 != c1) {

				if (cursor >= c0 && cursor < c1) {
					color = desc->text_selected_color;
				}
			}
			else {

				if (cursor >= c0 && cursor <= c1) {
					color = desc->text_selected_color;
				}
			}
		}

		// Add glyph to vertex buffer
		{
			f32 advance = glyph->advance * xmult;

			if (*text != ' ') {
				f32 xpos = xoff + glyph->xoff * xmult;
				f32 ypos = yoff + glyph->yoff * ymult;
				f32 width = glyph->w * xmult;
				f32 height = glyph->h * ymult;

				TextVertex p0, p1, p2, p3;

				p0.position = (v2){ xpos, ypos + height };
				p0.texcoord = (v2){ glyph->texcoord.x, glyph->texcoord.w };
				p0.color = color;

				p1.position = (v2){ xpos + width, ypos + height };
				p1.texcoord = (v2){ glyph->texcoord.z, glyph->texcoord.w };
				p1.color = color;

				p2.position = (v2){ xpos, ypos };
				p2.texcoord = (v2){ glyph->texcoord.x, glyph->texcoord.y };
				p2.color = color;

				p3.position = (v2){ xpos + width, ypos };
				p3.texcoord = (v2){ glyph->texcoord.z, glyph->texcoord.y };
				p3.color = color;

				render->batch_data[batch_count++] = p0;
				render->batch_data[batch_count++] = p1;
				render->batch_data[batch_count++] = p2;
				render->batch_data[batch_count++] = p3;
			}

			xoff += advance;
		}

		// Draw if the buffer are filled
		if (batch_count == TEXT_BATCH_SIZE * 4) {

			draw_text_batch(render_target, alignment, line_height, batch_count, desc->flags, cmd);
			batch_count = 0u;
		}

		++text;
	}

	if (batch_count) {

		draw_text_batch(render_target, alignment, line_height, batch_count, desc->flags, cmd);
	}
}

GPUImage* get_white_image()
{
	return render->white_image;
}

GPUImage* load_skybox_image(const char* filepath)
{
	Color* data;
	u32 w, h;
	if (load_image(filepath, (void**)&data, &w, &h) == FALSE)
		return NULL;

	u32 image_width = w / 4u;
	u32 image_height = h / 3u;

	Color* images[6u];
	Color* mem = (Color*)memory_allocate(image_width * image_height * 4u * 6u);

	foreach(i, 6u) {

		images[i] = mem + image_width * image_height * i;

		u32 xoff;
		u32 yoff;

		switch (i)
		{
		case 0:
			xoff = image_width;
			yoff = image_height;
			break;
		case 1:
			xoff = image_width * 3u;
			yoff = image_height;
			break;
		case 2:
			xoff = image_width;
			yoff = 0u;
			break;
		case 3:
			xoff = image_width;
			yoff = image_height * 2u;
			break;
		case 4:
			xoff = image_width * 2u;
			yoff = image_height;
			break;
		default:
			xoff = 0u;
			yoff = image_height;
			break;
		}

		for (u32 y = yoff; y < yoff + image_height; ++y) {

			Color* src = data + xoff + y * w;

			Color* dst = images[i] + (y - yoff) * image_width;
			Color* dst_end = dst + image_width;

			while (dst != dst_end) {

				*dst = *src;

				++src;
				++dst;
			}
		}
	}

	GPUImageDesc desc;
	desc.data = images;
	desc.size = image_width * image_height * 4u;
	desc.format = Format_R8G8B8A8_UNORM;
	desc.layout = GPUImageLayout_ShaderResource;
	desc.type = GPUImageType_ShaderResource | GPUImageType_CubeMap;
	desc.usage = ResourceUsage_Static;
	desc.cpu_access = CPUAccess_None;
	desc.width = image_width;
	desc.height = image_height;

	GPUImage* image;
	b8 res = graphics_image_create(&image, &desc);

	memory_free(mem);
	memory_free(data);

	return res ? image : NULL;
}

#endif
