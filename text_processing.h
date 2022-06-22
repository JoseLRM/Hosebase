#pragma once

#include "Hosebase/graphics.h"
#include "Hosebase/font.h"
#include "Hosebase/math.h"
#include "Hosebase/os.h"

// --- TEXT BUFFER FORMAT ---
// - char size (1 for now)
// - ~ to start format
// - Format attached to the left of a char
// - after the format select the extension of the format (c(char), w(word), l(line), a(all))
//
// Char ~        : ~~
// Color         : ~cFFFFFFFFw
// Default Color : ~dw
//
// Ex: Hello ~cFF2000FFwWorld

v2 text_pos(const void* text, u32 cursor, Font* font, f32 font_size);
u32 text_lines(void* buffer);

// The font may be monospaced
f32 text_cursor_width(Font* font, f32 font_size);

// Valid chars jumps from the begining of the line to the cursor
u32 text_jump_count(void* buffer, u32 cursor);

// Valid chars in the whole line of the cursor
u32 text_line_chars(void* buffer, u32 cursor);

// Return the char count or u32_max if overflows
u32 text_jump_lines(const void* text, u32 lines);

// Edit text

u32 text_erase(void* buffer, u32 cursor, b8 right);
u32 text_erase_word(void* buffer, u32 cursor, b8 right);
u32 text_erase_range(void* buffer, u32 c0, u32 c1);
u32 text_insert_static(void* buffer, u32 cursor, u32 buffer_size, const char* text);
u32 text_insert_dynamic(Buffer* buffer, u32 cursor, const char* text);

// Move cursor

u32 text_move_right(void* buffer, u32 cursor);
u32 text_move_right_word(void* buffer, u32 cursor);
u32 text_move_left(void* buffer, u32 cursor);
u32 text_move_left_word(void* buffer, u32 cursor);
u32 text_move_down(void* buffer, u32 cursor);
u32 text_move_up(void* buffer, u32 cursor);
u32 text_move_begin(void* buffer, u32 cursor);
u32 text_move_end(void* buffer, u32 cursor);

// Advance coords

inline f32 char_width(u32 c, Font* font, f32 font_size)
{
	Glyph* g = font_get(font, ' ');
	assert(g);
	if (g == NULL) return 0.f;

	if (c == '\t')
		return g->advance * 4 * (font_size / window_aspect());

	return g->advance * (font_size / window_aspect());
}

const void* text_advance(const void* buffer, b8 last, v2* pos, Font* font, f32 font_size);
const void* text_advance_line(const void* buffer, v2* pos, Font* font, f32 font_size);
const void* text_advance_end(const void* buffer, v2* pos, Font* font, f32 font_size);

#define TextProcessInFlag_NoNextLine SV_BIT(0)
#define TextProcessInFlag_DynamicBuffer SV_BIT(1)

#define TextProcessOutFlag_Enter SV_BIT(0)

typedef struct {
	u32 cursor0;
	u32 cursor1;
	u32 cursor_offset;
	i32 vertical_offset;
	i32 horizontal_offset;

	v2 mouse_pressed;
	b8 mouse_dragging;
} TextContext;

typedef struct {
	void* buffer;
	u32 buffer_size;
	TextContext* context;
	v4 bounds;
	Font* font;
	f32 font_size;
	u64 in_flags;
	u64* out_flags;
} TextProcessDesc;

void text_process(const TextProcessDesc* desc);