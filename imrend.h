#pragma once

b8 imrend_initialize();
void imrend_close();

// IMMEDIATE MODE RENDERER

typedef enum {
	ImRendCamera_Clip, // Default
	ImRendCamera_Scene,
	ImRendCamera_Normal,

#if SV_EDITOR
	ImRendCamera_Editor,
#endif
} ImRendCamera;

void imrend_begin_batch(GPUImage* render_target, CommandList cmd);
void imrend_flush(CommandList cmd);

void imrend_push_matrix(Mat4 matrix, CommandList cmd);
void imrend_pop_matrix(CommandList cmd);

void imrend_push_scissor(f32 x, f32 y, f32 width, f32 height, b8 additive, CommandList cmd);
void imrend_pop_scissor(CommandList cmd);

void imrend_camera(ImRendCamera camera, CommandList cmd);
void imrend_camera_custom(Mat4 view_matrix, Mat4 projection_matrix, CommandList cmd);

// Draw calls

void imrend_draw_quad(v3 position, v2 size, Color color, CommandList cmd);
void imrend_draw_line(v3 p0, v3 p1, Color color, CommandList cmd);
void imrend_draw_triangle(v3 p0, v3 p1, v3 p2, Color color, CommandList cmd);
void imrend_draw_sprite(v3 position, v2 size, Color color, GPUImage* image, GPUImageLayout layout, v4 texcoord, CommandList cmd);

// void imrend_draw_mesh_wireframe(Mesh* mesh, Color color, CommandList cmd);

// TODO: Font, color and alignment in stack
	
/*void imrend_draw_text(const char* text, f32 font_size, f32 aspect, Font* font, Color color, CommandList cmd);
void imrend_draw_text_bounds(const char* text, f32 max_width, u32 max_lines, f32 font_size, f32 aspect, TextAlignment alignment, Font* font, Color color, CommandList cmd);
void imrend_draw_text_area(const char* text, size_t text_size, f32 max_width, u32 max_lines, f32 font_size, f32 aspect, TextAlignment alignment, u32 line_offset, b8 bottom_top, Font* font, Color color, CommandList cmd);*/

// High level draw calls

void imrend_draw_cube_wireframe(Color color, CommandList cmd);
void imrend_draw_sphere_wireframe(u32 vertical_segments, u32 horizontal_segments, Color color, CommandList cmd);

void imrend_draw_orthographic_grip(v2 position, v2 offset, v2 size, v2 gridSize, Color color, CommandList cmd);
