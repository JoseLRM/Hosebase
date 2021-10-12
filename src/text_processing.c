#include "text_processing.h"

#include "Hosebase/memory_manager.h"

v2 text_pos(const void* text, u32 cursor, Font* font, f32 font_size)
{
	const char* it = (const char*)text;
	const char* end = it + cursor;

	v2 pos = v2_zero();

	const char* new = it;

	do {
		it = new;
		new = text_advance(it, it == end, &pos, font, font_size);
	} while (it != new);

	return pos;
}

u32 text_lines(void* buffer)
{
	char* text = (char*)buffer;
	if (*text == '\0') return 0;

	u32 lines = 1u;

	while (*text != '\0') {

		if (*text == '\n') {
			++lines;
		}

		++text;
	}

	return lines;
}

f32 text_cursor_width(Font* font, f32 font_size)
{
	Glyph* g = font_get(font, 'a');

	if (g == NULL) return 0.f;

	return g->advance * (font_size / window_aspect());
}

u32 text_jump_count(void* buffer, u32 cursor)
{
	char* text = buffer;

	if (cursor == 0) return 0;

	u32 begin_line = cursor - 1;
	u32 ignore_count = 0u;

	while (begin_line && text[begin_line] != '\n') {

		--begin_line;
	}

	if (text[begin_line] == '\n') ++begin_line;

	return cursor - begin_line - ignore_count;
}

u32 text_line_chars(void* buffer, u32 cursor)
{
	u32 count = text_jump_count(buffer, cursor);

	char* text = (char*)buffer;
	while (text[cursor] != '\0' && text[cursor] != '\n') {

		switch (text[cursor]) {

		default:
			++count;

		}

		++cursor;
	}

	if (text[cursor] == '\n') ++count;

	return count;
}

inline void _text_erase(void* buffer, u32 cursor, u32 count)
{
	char* text = (char*)buffer;

	foreach(i, count) {
		if (text[cursor + i] == '\0') {
			count = i;
			break;
		}
	}

	u32 i = 0u;
	while (1) {

		text[cursor + i] = text[cursor + i + count];
		if (text[cursor + i] == '\0')
			break;

		++i;
	}
}

u32 text_erase(void* buffer, u32 cursor, b8 right)
{
	if (right)
		_text_erase(buffer, cursor, 1);
	else if (cursor) {

		--cursor;
		_text_erase(buffer, cursor, 1);
	}

	return cursor;
}

u32 text_erase_word(void* buffer, u32 cursor, b8 right)
{
	char* text = (char*)buffer;

	if (right) {

		u32 end = text_move_right_word(buffer, cursor);
		_text_erase(buffer, cursor, end - cursor);
	}
	else if (cursor) {

		u32 begin = text_move_left_word(buffer, cursor);
		_text_erase(buffer, begin, cursor - begin);
		cursor = begin;
	}

	return cursor;
}

u32 text_erase_range(void* buffer, u32 c0, u32 c1)
{
	if (c0 < c1) {

		_text_erase(buffer, c0, c1 - c0);
		return c0;
	}
	else {

		_text_erase(buffer, c1, c0 - c1);
		return c1;
	}
}

u32 text_insert_static(void* buffer, u32 cursor, u32 buffer_size, const char* src)
{
	char* text = (char*)buffer;
	u32 size = string_size(src);

	u32 end = cursor;
	while (text[end] != '\0')
		++end;

	if (end + size > buffer_size) {

		size = buffer_size - size;
	}

	if (size == 0) return cursor;

	for (i32 i = end + size; i >= cursor + size; --i) {

		text[i] = text[i - size];
	}
	for (u32 i = 0; i < size; ++i)
		text[cursor + i] = src[i];

	return cursor + size;
}

u32 text_insert_dynamic(Buffer* buffer, u32 cursor, const char* src)
{
	u32 size = string_size(src);

	if (size == 0) return cursor;

	if (buffer->size == 0) {

		buffer_write_back(buffer, src, size + 1);
		return cursor + size;
	}
	else {

		char* text = (char*)buffer->data;

		u32 end = cursor;
		while (text[end] != '\0')
			++end;

		if (end + size + 1 > buffer->size) {

			// TODO: Optimize
			buffer_resize(buffer, end + size + 1);
		}

		text = (char*)buffer->data;

		for (i32 i = end + size; i >= cursor + size; --i) {

			text[i] = text[i - size];
		}
		for (u32 i = 0; i < size; ++i)
			text[cursor + i] = src[i];

		return cursor + size;
	}
}

void* text_jump_lines(void* text, u32 lines)
{
	char* it = (char*)text;

	while (lines && *it != '\0') {
		if (*it == '\n')
			--lines;
		++it;
	}

	if (lines != 0)
		return NULL;

	return it;
}

u32 text_move_right(void* buffer, u32 cursor)
{
	char* text = (char*)buffer;

	if (text[cursor] == '\n' || text[cursor] == '\0') return cursor;

	return ++cursor;
}

inline u8 char_mode(u32 c)
{
	if (char_is_number(c)) {
		return 1;
	}
	else if (char_is_lower_case(c)) {
		return 2;
	}
	else if (char_is_capital(c)) {
		return 3;
	}

	return 0;
}

u32 text_move_right_word(void* buffer, u32 cursor)
{
	char* text = (char*)buffer;

	if (text[cursor] == '\0')
		return cursor;

	u8 first_mode = char_mode(text[cursor]);

	++cursor;

	while (text[cursor] != '\0' && !char_is_letter(text[cursor]) && !char_is_number(text[cursor])) {
		++cursor;
	}

	if (text[cursor] == '\0')
		return cursor;

	u8 mode = char_mode(text[cursor]);
	assert(mode != 0);

	u8 m;

	while (1) {

		m = char_mode(text[cursor]);

		if (m != mode && m != 1)
			break;

		++cursor;
	}

	if (first_mode <= 1 && (mode == 2 || mode == 3) && (m == 2 || m == 3))
		--cursor;

	return cursor;
}

u32 text_move_left(void* buffer, u32 cursor)
{
	char* text = (char*)buffer;

	if (cursor == 0)
		return cursor;

	b8 next;

	do {

		next = FALSE;
		--cursor;

		switch (text[cursor]) {

		case '\n':
			++cursor;
			break;

		}
	} while (cursor && next);

	return cursor;
}

u32 text_move_left_word(void* buffer, u32 cursor)
{
	char* text = (char*)buffer;

	if (cursor == 0)
		return cursor;

	--cursor;

	while (cursor && !char_is_letter(text[cursor]) && !char_is_number(text[cursor])) {
		--cursor;
	}

	if (cursor == 0)
		return cursor;

	u8 mode = char_mode(text[cursor]);
	assert(mode != 0);

	u8 m;

	while (cursor) {

		m = char_mode(text[cursor]);

		if (mode == 1 && m > 1) {
			mode = m;
		}

		if (m != mode && m != 1) {
			break;
		}

		--cursor;
	}

	if (cursor == 0)
		m = char_mode(text[cursor]);

	++cursor;

	if ((mode == 2 || mode == 3) && (m == 2 || m == 3))
		--cursor;

	return cursor;
}

u32 text_move_down(void* buffer, u32 cursor)
{
	char* text = (char*)buffer;

	u32 next_cursor = cursor;

	while (text[next_cursor] != '\0' && text[next_cursor] != '\n') {
		++next_cursor;
	}

	if (text[next_cursor] == '\0') return cursor;
	++next_cursor;

	u32 jump_count = text_jump_count(buffer, cursor);

	while (jump_count--) {

		u32 last = next_cursor;
		next_cursor = text_move_right(buffer, next_cursor);
		if (last == next_cursor)
			break;
	}

	return next_cursor;
}

u32 text_move_up(void* buffer, u32 cursor)
{
	char* text = (char*)buffer;

	if (cursor == 0) return cursor;

	u32 next_cursor = cursor - 1;

	while (next_cursor && text[next_cursor] != '\n') {
		--next_cursor;
	}

	if (next_cursor == 0) return cursor;

	i32 jump_count = text_jump_count(buffer, cursor);
	u32 chars = text_line_chars(buffer, next_cursor);

	jump_count = (i32)chars - jump_count - 1;

	while (jump_count-- > 0) {

		u32 last = next_cursor;
		next_cursor = text_move_left(buffer, next_cursor);
		if (last == next_cursor)
			break;
	}

	return next_cursor;
}

u32 text_move_begin(void* buffer, u32 cursor)
{
	char* text = (char*)buffer;

	u32 count = text_jump_count(text, cursor);

	while (count--) {

		u32 last = cursor;
		cursor = text_move_left(text, cursor);
		if (last == cursor) break;
	}

	return cursor;
}

u32 text_move_end(void* buffer, u32 cursor)
{
	char* text = (char*)buffer;

	while (text[cursor] != '\0' && text[cursor] != '\n') {
		++cursor;
	}

	return cursor;
}

const void* text_advance(const void* buffer, b8 last, v2* pos, Font* font, f32 font_size)
{
	Glyph* g = NULL;

	const char* it = (const char*)buffer;

	if (*it == '\n') {

		if (!last) {
			pos->x = 0.f;
			pos->y -= font_size;
		}
		else g = font_get(font, 'a');
	}
	else if (*it == '\t') {

		if (!last) {
			g = font_get(font, ' ');
			assert(g);
			if (g) pos->x += g->advance * 4 * (font_size / window_aspect());
			g = NULL;
		}
		else g = font_get(font, ' ');
	}
	else if (*it == '~') {

		++it;

		b8 corrupt = FALSE;

		switch (*it) {

		case '~':
			g = font_get(font, '~');
			break;

		case 'c':

			++it;

			foreach(i, 8) {

				if (*it == '\0') {
					corrupt = TRUE;
					break;
				}

				++it;
			}
			break;

		default:
			corrupt = TRUE;

		}

		if (corrupt) return it;
	}
	else if (*it == '\0') g = font_get(font, 'a');
	else g = font_get(font, *it);

	if (g) {
		pos->x += g->advance * (font_size / window_aspect());
	}

	return (*it == '\0' || last) ? it : (it + 1);
}

const void* text_advance_line(const void* buffer, v2* pos, Font* font, f32 font_size)
{
	Glyph* g = NULL;

	const char* it = (const char*)buffer;

	while (*it != '\0') {

		if (*it == '\n') {
			pos->x = 0.f;
			pos->y -= font_size;
			return it + 1;
		}

		++it;
	}

	return buffer;
}

const void* text_advance_end(const void* buffer, v2* pos, Font* font, f32 font_size)
{
	Glyph* g = NULL;

	const char* it = (const char*)buffer;

	while (*it != '\0' && *it != '\n') {

		if (*it == '\t') {

			g = font_get(font, ' ');

			if (g) pos->x += g->advance * 4 * (font_size / window_aspect());
		}
		else {
			g = font_get(font, *it);

			if (g) {
				pos->x += g->advance * (font_size / window_aspect());
			}
		}

		++it;
	}

	if (*it == '\n') {

		g = font_get(font, 'a');

		if (g) pos->x += g->advance * (font_size / window_aspect());
	}

	return it;
}

inline u32 text_pick(void* buffer, v4 bounds, Font* font, f32 font_size, v2 pick)
{
	const char* it = (const char*)buffer;

	v2 size = v2_set(text_cursor_width(font, font_size), font_size + font->vertical_offset * font_size);
	v2 pos = v2_zero();

	pick.x = pick.x - (bounds.x - bounds.z * 0.5f);
	pick.y = pick.y - (bounds.y + bounds.w * 0.5f) + size.y * 0.5f;

	const char* new;

	while (1) {

		if (fabs(pos.y - pick.y) < size.y * 0.5f) {

			while (pos.x < pick.x) {

				new = text_advance(it, FALSE, &pos, font, font_size);

				if (*it == '\n' || new == it) {
					++it;
					break;
				}

				it = new;
			}

			--it;
			break;
		}

		new = text_advance_line(it, &pos, font, font_size);

		if (new == it) break;
		it = new;
	}

	return it - (const char*)buffer;
}

inline void _text_write(char* text, u32 buffer_size, Buffer* buffer, u32* c0, u32* c1, const char* src)
{
	if (*c0 != *c1) {

		*c0 = text_erase_range(text, *c0, *c1);
		*c1 = *c0;
	}

	if (buffer)* c0 = text_insert_dynamic(buffer, *c0, src);
	else *c0 = text_insert_static(text, *c0, buffer_size, src);
	*c1 = *c0;
}

void text_process(const TextProcessDesc* desc)
{
	TextContext* ctx = desc->context;
	u32 buffer_size = desc->buffer_size;
	u32 c0 = ctx->cursor0;
	u32 c1 = ctx->cursor1;

	Buffer* buffer;
	char* text;

	Font* font = desc->font;
	f32 font_size = desc->font_size;

	if (desc->in_flags & TextProcessInFlag_DynamicBuffer) {
		buffer = (Buffer*)desc->buffer;
		text = (char*)buffer->data;
	}
	else {
		buffer = NULL;
		text = (char*)desc->buffer;
	}

	if (desc->out_flags)* desc->out_flags = 0;

	b8 shift = input_key(Key_Shift, InputState_Any);
	b8 control = input_key(Key_Control, InputState_Any);

	const char* user_text = input_text();

	if (user_text[0] != '\0') {

		_text_write(text, buffer_size, buffer, &c0, &c1, user_text);
	}

	u32 command_count = input_text_command_count();

	foreach(i, command_count) {

		TextCommand c = input_text_command(i);

		switch (c) {

		case TextCommand_Begin:
		{
			c0 = text_move_begin(text, c0);
			if (!shift)
				c1 = c0;
		}
		break;

		case TextCommand_End:
		{
			c0 = text_move_end(text, c0);
			if (!shift)
				c1 = c0;
		}
		break;

		case TextCommand_Enter:
		{
			if (!(desc->in_flags & TextProcessInFlag_NoNextLine)) {

				_text_write(text, buffer_size, buffer, &c0, &c1, "\n");
			}

			if (desc->out_flags)* desc->out_flags |= TextProcessOutFlag_Enter;
		}
		break;

		case TextCommand_DeleteLeft:
		{
			if (c0 != c1) {

				c0 = text_erase_range(text, c0, c1);
				c1 = c0;
			}
			else if (c0) {

				if (control)
					c0 = text_erase_word(text, c0, FALSE);
				else
					c0 = text_erase(text, c0, FALSE);

				c1 = c0;
			}
		}
		break;

		case TextCommand_Left:
		{
			if (control)
				c0 = text_move_left_word(text, c0);
			else
				c0 = text_move_left(text, c0);

			if (!shift)
				c1 = c0;
		}
		break;

		case TextCommand_Right:
		{
			if (control)
				c0 = text_move_right_word(text, c0);
			else
				c0 = text_move_right(text, c0);

			if (!shift)
				c1 = c0;
		}
		break;

		case TextCommand_Down:
		{
			c0 = text_move_down(text, c0);
			if (!shift)
				c1 = c0;
		}
		break;

		case TextCommand_Up:
		{
			c0 = text_move_up(text, c0);
			if (!shift)
				c1 = c0;
		}
		break;

		case TextCommand_Paste:
		{
			const char* paste = clipboard_read_ansi();

			_text_write(text, buffer_size, buffer, &c0, &c1, paste);
		}
		break;

		case TextCommand_Copy:
		case TextCommand_Cut:
		{
			if (c0 != c1) {

				u32 begin, end;
				if (c0 < c1) {
					begin = c0;
					end = c1;
				}
				else {
					begin = c1;
					end = c0;
				}

				u32 size = end - begin;

				char* buffer = (char*)memory_allocate(size + 1);
				memory_copy(buffer, text + begin, size);

				clipboard_write_ansi(buffer);

				memory_free(buffer);
			}
		}
		break;

		}
	}

	v2 mouse = input_mouse_position();
	mouse = v2_add_scalar(mouse, 0.5f);

	if (shift) ctx->mouse_dragging = FALSE;

	if (input_mouse_button(MouseButton_Left, shift ? InputState_Any : InputState_Pressed)) {

		u32 c = text_pick(text, desc->bounds, font, font_size, mouse);

		if (shift) {
			c0 = c;
		}
		else {
			c0 = c;
			c1 = c;
		}

		if (!shift) {

			ctx->mouse_pressed = mouse;
		}
	}

	if (!shift && input_mouse_button(MouseButton_Left, InputState_Any)) {

		f32 dist = v2_distance(mouse, ctx->mouse_pressed);
		if (dist > 0.01f) {

			ctx->mouse_dragging = TRUE;
		}
	}

	if (ctx->mouse_dragging) {

		if (!input_mouse_button(MouseButton_Left, InputState_Any)) {
			ctx->mouse_dragging = FALSE;
		}
		else {

			u32 c = text_pick(text, desc->bounds, font, font_size, mouse);
			c0 = c;
		}
	}

	ctx->cursor0 = c0;
	ctx->cursor1 = c1;
}
