#include "Hosebase/defines.h"

#if SV_GRAPHICS

#define STB_TRUETYPE_IMPLEMENTATION

#include "Hosebase/external/stb_truetype.h"
#include "Hosebase/font.h"
#include "Hosebase/platform.h"

typedef struct {
	u8* bitmap;
	i32 w, h;
	i32 advance;
	i32 xoff, yoff;
	i32 left_side_bearing;
	v4 texcoord;
	char c;
} TempGlyph;

typedef struct {
	u32 begin_index;
	u32 end_index;
	u32 height;
} GlyphLine;

SV_INLINE void add_glyph(char c, stbtt_fontinfo* info, f32 heightScale, DynamicArray(TempGlyph)* glyphs)
{
	i32 index = stbtt_FindGlyphIndex(info, c);

	TempGlyph glyph;
	glyph.c = c;
	glyph.bitmap = (c == ' ') ? NULL : stbtt_GetGlyphBitmap(info, 0.f, heightScale, index, &glyph.w, &glyph.h, &glyph.xoff, &glyph.yoff);
	stbtt_GetGlyphHMetrics(info, index, &glyph.advance, &glyph.left_side_bearing);
	
	glyph.advance = (i32)((f32)(glyph.advance) * heightScale);
	
	array_push(glyphs, glyph);
}

SV_INLINE b8 glyph_less_than(const TempGlyph* g0, const TempGlyph* g1)
{
	return g0->h > g1->h;
}

b8 font_create(Font* font, const char* filepath, f32 pixel_height, FontFlags flags)
{
	u8* file_data;
	u32 file_size;
	SV_CHECK(file_read_binary(FilepathType_Asset, filepath, &file_data, &file_size));

	stbtt_fontinfo info;
	stbtt_InitFont(&info, file_data, 0);

	font->glyphs = (Glyph*)memory_allocate(sizeof(Glyph) * FONT_CHAR_COUNT);
		
	// TODO: Resize
	DynamicArray(TempGlyph) glyphs = array_init(TempGlyph, 1.7f);
	DynamicArray(GlyphLine) lines = array_init(GlyphLine, 1.7f);

	f32 height_scale = stbtt_ScaleForPixelHeight(&info, pixel_height);

	// Get Font metrix
	f32 line_height;
	{
		i32 ascent, descent, line_gap;
		stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

		line_height = (f32)(ascent - descent + line_gap) * height_scale;
	}

	for (char c = 'A'; c <= 'Z'; ++c) {
		add_glyph(c, &info, height_scale, &glyphs);
	}
	for (char c = 'a'; c <= 'z'; ++c) {
		add_glyph(c, &info, height_scale, &glyphs);
	}
	for (char c = '0'; c <= '9'; ++c) {
		add_glyph(c, &info, height_scale, &glyphs);
	}

	add_glyph('%', &info, height_scale, &glyphs);
	add_glyph('+', &info, height_scale, &glyphs);
	add_glyph('*', &info, height_scale, &glyphs);
	add_glyph('-', &info, height_scale, &glyphs);
	add_glyph('/', &info, height_scale, &glyphs);
	add_glyph('.', &info, height_scale, &glyphs);
	add_glyph(',', &info, height_scale, &glyphs);
	add_glyph('-', &info, height_scale, &glyphs);
	add_glyph('_', &info, height_scale, &glyphs);
	add_glyph('(', &info, height_scale, &glyphs);
	add_glyph(')', &info, height_scale, &glyphs);
	add_glyph('?', &info, height_scale, &glyphs);
	add_glyph('!', &info, height_scale, &glyphs);
	add_glyph('#', &info, height_scale, &glyphs);
	add_glyph('@', &info, height_scale, &glyphs);
	add_glyph('<', &info, height_scale, &glyphs);
	add_glyph('>', &info, height_scale, &glyphs);
	add_glyph(':', &info, height_scale, &glyphs);
	add_glyph(';', &info, height_scale, &glyphs);
	add_glyph('[', &info, height_scale, &glyphs);
	add_glyph(']', &info, height_scale, &glyphs);
	add_glyph('{', &info, height_scale, &glyphs);
	add_glyph('}', &info, height_scale, &glyphs);
	add_glyph('=', &info, height_scale, &glyphs);
	add_glyph('\"', &info, height_scale, &glyphs);
	add_glyph('\'', &info, height_scale, &glyphs);
	add_glyph('\\', &info, height_scale, &glyphs);
	add_glyph('|', &info, height_scale, &glyphs);
	add_glyph('~', &info, height_scale, &glyphs);
	add_glyph('$', &info, height_scale, &glyphs);
	add_glyph('&', &info, height_scale, &glyphs);
	add_glyph(' ', &info, height_scale, &glyphs);

	// Sort the glyphs by their heights
	array_sort(glyphs.data, glyphs.size, sizeof(TempGlyph), glyph_less_than);

	// Calculate num of lines and atlas resolution
	u32 atlas_width;
	u32 atlas_height;
	u32 atlas_spacing = 5u;//u32(0.035f * pixel_height);

	{
		atlas_width = (u32)(pixel_height * 6.f);

		u32 xoffset = 0u;

		GlyphLine* current_line = array_add(&lines);
		current_line->begin_index = 0u;

		foreach (i, glyphs.size) {

			TempGlyph* g = (TempGlyph*)array_get(&glyphs, i);
			if (g->bitmap == NULL) continue;

			// Next line
			if (xoffset + g->w + atlas_spacing >= atlas_width) {
					
				xoffset = 0u;

				TempGlyph* g0 = (TempGlyph*)array_get(&glyphs, current_line->begin_index);
				current_line->height = g0->h + atlas_spacing;
				current_line->end_index = i;

				current_line = array_add(&lines);

				current_line->begin_index = i;
			}
				
			xoffset += g->w + atlas_spacing;
		}

		u32 back = lines.size - 1;

		GlyphLine* line = (GlyphLine*)array_get(&lines, back);
		line->end_index = glyphs.size;

		TempGlyph* g0 = (TempGlyph*)array_get(&glyphs, line->begin_index);
		line->height = g0->h + atlas_spacing;

		// Calculate atlas height
		atlas_height = 0u;
		foreach(i, lines.size) {
			GlyphLine* line = (GlyphLine*)array_get(&lines, i);
			atlas_height += line->height;
		}
	}

	// Draw font atlas and set texcoords
	u8* atlas = (u8*)memory_allocate(atlas_width * atlas_height);

	size_t xoffset = 0u;
	size_t yoffset = 0u;

	foreach(j, lines.size) {
		
		GlyphLine line = *(GlyphLine*)array_get(&lines, j);

		for (u32 i = line.begin_index; i < line.end_index; ++i) {
				
			TempGlyph* g = array_get(&glyphs, i);
			if (g->bitmap == NULL) continue;

			// Compute texcoord
			g->texcoord.x = (f32)(xoffset) / (f32)atlas_width;
			g->texcoord.y = (f32)(yoffset) / (f32)atlas_height;
			g->texcoord.z = g->texcoord.x + (f32)g->w / (f32)atlas_width;
			g->texcoord.w = g->texcoord.y + (f32)g->h / (f32)atlas_height;

			foreach(y, g->h) {

				size_t bitmap_offset = y * g->w;

				foreach(x, g->w) {

					atlas[x + xoffset + (y + yoffset) * atlas_width] = g->bitmap[x + bitmap_offset];
				}
			}

			xoffset += g->w + atlas_spacing;
		}

		xoffset = 0u;
		yoffset += line.height;
	}

	// Free bitmaps and set data to the font
	foreach(i, glyphs.size) {

		TempGlyph g0 = *(TempGlyph*)array_get(&glyphs, i);
		
		stbtt_FreeBitmap(g0.bitmap, 0);

		Glyph* g = font_get(font, (u32)(g0.c));

		if (g) {

			g->advance = g0.advance / line_height;
			g->left_side_bearing = g0.left_side_bearing / line_height;
			g->xoff = g0.xoff / line_height;
			g->w = g0.w / line_height;
			g->h = g0.h / line_height;

			// Top - bottom to bottom - top
			g->yoff = ((f32)(-g0.yoff) - (f32)(g0.h)) / line_height;
			font->vertical_offset = SV_MIN(g->yoff, font->vertical_offset);

			g->texcoord.x = g0.texcoord.x;
			g->texcoord.y = g0.texcoord.w;
			g->texcoord.z = g0.texcoord.z;
			g->texcoord.w = g0.texcoord.y;
		}
	}

	font->vertical_offset = -font->vertical_offset;

	GPUImageDesc desc;
	desc.data = atlas;
	desc.size = sizeof(u8) * atlas_width * atlas_height;
	desc.format = Format_R8_UNORM;
	desc.layout = GPUImageLayout_ShaderResource;
	desc.type = GPUImageType_ShaderResource;
	desc.usage = ResourceUsage_Static;
	desc.cpu_access = CPUAccess_None;
	desc.width = atlas_width;
	desc.height = atlas_height;
	
	// TODO: Handle error
	SV_CHECK(graphics_image_create(&font->image, &desc));
	
	font->pixel_height = pixel_height;

	memory_free(atlas);
	if (file_data) memory_free(file_data);

	return TRUE;
}

void font_destroy(Font* font)
{
	memory_free(font->glyphs);
	graphics_destroy(font->image);
}

#endif