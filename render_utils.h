#pragma once

#include "hosebase/defines.h"

SV_BEGIN_C_HEADER

#if SV_GRAPHICS

#include "Hosebase/font.h"
#include "Hosebase/text_processing.h"

#define DrawTextFlag_LineCentred SV_BIT(0)
#define DrawTextFlag_BottomTop SV_BIT(1)

typedef struct {

	GPUImage* render_target;

	u64 flags;
	const void* text;
	Font* font;
	u32 max_lines;

	f32 aspect;
	TextAlignment alignment;
	f32 font_size;
	m4 transform_matrix;

	TextContext* context;

	Color text_default_color;
	Color text_selected_color;
	Color selection_color;
	Color cursor_color;

} DrawTextDesc;

void draw_text(const DrawTextDesc* desc, CommandList cmd);

b8 render_utils_initialize();
void render_utils_close();

GPUImage* get_white_image();
GPUImage* load_skybox_image(const char* filepath);

// IMMEDIATE MODE RENDERER

typedef enum {
	ImRendCamera_Clip, // Default
	ImRendCamera_Scene,
	ImRendCamera_Normal,

#if SV_EDITOR
	ImRendCamera_Editor,
#endif
} ImRendCamera;

void imrend_begin_batch(GPUImage* render_target, GPUImage* depth_map, CommandList cmd);
void imrend_flush(CommandList cmd);

void imrend_push_matrix(m4 matrix, CommandList cmd);
void imrend_pop_matrix(CommandList cmd);

void imrend_push_scissor(f32 x, f32 y, f32 width, f32 height, b8 additive, CommandList cmd);
void imrend_pop_scissor(CommandList cmd);

void imrend_depth_test(b8 read, b8 write, CommandList cmd);
void imrend_line_width(f32 line_width, CommandList cmd);

void imrend_camera(ImRendCamera camera, CommandList cmd);
void imrend_camera_custom(m4 view_matrix, m4 projection_matrix, CommandList cmd);

// Draw calls

void imrend_draw_quad(v3 position, v2 size, Color color, CommandList cmd);
void imrend_draw_line(v3 p0, v3 p1, Color color, CommandList cmd);
void imrend_draw_triangle(v3 p0, v3 p1, v3 p2, Color color, CommandList cmd);
void imrend_draw_sprite(v3 position, v2 size, Color color, GPUImage* image, GPUImageLayout layout, v4 texcoord, CommandList cmd);

// void imrend_draw_mesh_wireframe(Mesh* mesh, Color color, CommandList cmd);

// TODO: Font, color and alignment in stack

typedef struct {

	const char* text;
	u64 flags;
	Font* font;
	u32 max_lines;
	TextAlignment alignment;
	f32 aspect;
	f32 font_size;
	m4 transform_matrix;
	TextContext* context;
	Color text_default_color;
	Color text_selected_color;
	Color selection_color;
	Color cursor_color;

} ImRendDrawTextDesc;

void imrend_draw_text(const ImRendDrawTextDesc* desc, CommandList cmd);
//void imrend_draw_text_area(const char* text, size_t text_size, f32 max_width, u32 max_lines, f32 font_size, f32 aspect, TextAlignment alignment, u32 line_offset, b8 bottom_top, Font* font, Color color, CommandList cmd);

// High level draw calls

void imrend_draw_cube_wireframe(Color color, CommandList cmd);
void imrend_draw_sphere_wireframe(u32 vertical_segments, u32 horizontal_segments, Color color, CommandList cmd);

void imrend_draw_orthographic_grip(v2 position, v2 offset, v2 size, v2 gridSize, Color color, CommandList cmd);

#endif

SV_END_C_HEADER