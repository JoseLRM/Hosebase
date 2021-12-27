#include "Hosebase/profiler.h"

#if SV_SLOW

#include "Hosebase/imgui.h"

#define FN_NAME_SIZE 100
#define THREAD_FN_MAX 200

#define FUNCTION_CACHE 200

typedef struct {
	u32 calls;
	f64 total_time;
} FunctionFrame;

typedef struct {

	FunctionFrame frames[FUNCTION_CACHE];
	u32 current_frame;
	b8 is_function;

	f64 total_time; // Accumulation of frames

	char name[FN_NAME_SIZE];
	u64 hash;

} FunctionData;

typedef struct {

	FunctionData* functions;
	u32 function_count;
	u32 function_capacity;

	char search_filter[FN_NAME_SIZE];

	Mutex mutex;

} ProfilerData;

static ProfilerData* profiler;

void _profiler_initialize()
{
	profiler = memory_allocate(sizeof(ProfilerData));
	profiler->mutex = mutex_create();
}

void _profiler_reset()
{
	foreach(i, profiler->function_count)
	{
		FunctionData* fn = profiler->functions + i;

		fn->total_time = 0.0;
		fn->current_frame++;

		FunctionFrame* frame = fn->frames + (fn->current_frame % FUNCTION_CACHE);
		frame->calls = 0;
		frame->total_time = 0.0;
	}
}

void _profiler_close()
{
	if (profiler) {

		mutex_destroy(profiler->mutex);

		memory_free(profiler);
	}
}

// Set the name parameter to create if not exists
inline FunctionData* new_function(u64 hash, const char* name)
{
	array_prepare(&profiler->functions, &profiler->function_count,
		&profiler->function_capacity, profiler->function_capacity + 200, 1, sizeof(FunctionData));

	FunctionData* fn = profiler->functions + profiler->function_count++;

	foreach(i, FUNCTION_CACHE) {

		FunctionFrame* frame = fn->frames + i;
		frame->calls = 0;
		frame->total_time = 0.0;
	}
	fn->current_frame = 0;
	fn->total_time = 0.0;

	string_copy(fn->name, name, FN_NAME_SIZE);
	fn->hash = hash;

	return fn;
}

inline FunctionData* find_function(u64 hash)
{
	foreach(i, profiler->function_count) {

		FunctionData* fn = profiler->functions + i;
		if (fn->hash == hash)
			return fn;
	}

	return NULL;
}

void _profiler_save(const char* name, struct _ProfilerChrono res, b8 is_function)
{
	u64 hash = hash_string(name);

	mutex_lock(profiler->mutex);

	FunctionData* fn = find_function(hash);
	if (fn == NULL) fn = new_function(hash, name);

	fn->is_function = is_function;
	
	FunctionFrame* frame = fn->frames + (fn->current_frame % FUNCTION_CACHE);

	f64 ellapsed = res.end - res.begin;

	frame->total_time += ellapsed;
	frame->calls++;

	mutex_unlock(profiler->mutex);
}

static b8 function_less_than(const FunctionData* f0, const FunctionData* f1)
{
	return f0->total_time > f1->total_time;
}

void profiler_gui()
{
	static b8 only_functions = TRUE;
	static b8 sort = TRUE;

	gui_begin_parent("stack");

	Color color = color_gray(100);
	color.a = 200;
	gui_set_background(NULL, v4_zero(), color);

	// Searcher
	{
		GuiStackLayoutData data = gui_stack_layout_get_data();
		data.width = (GuiDimension){ 0.95f, GuiUnit_Relative };
		data.height = (GuiDimension){ 70.f, GuiUnit_Pixel };
		data.padding = (GuiDimension){ 5.f, GuiUnit_Pixel };
		data.margin = (GuiDimension){ 5.f, GuiUnit_Pixel };
		gui_stack_layout_update(data);

		{
			gui_begin_parent("free");

			GuiFreeLayoutData data = gui_free_layout_get_data();
			data.x = (GuiCoord){ 0.5f, GuiUnit_Relative, GuiCoordAlign_Center };
			data.y = (GuiCoord){ 0.f, GuiUnit_Pixel, GuiCoordAlign_InverseTop };
			data.width = (GuiDimension){ 1.f, GuiUnit_Relative };
			data.height = (GuiDimension){ 30.f, GuiUnit_Pixel };

			gui_free_layout_update(data);

			if (gui_text_field(profiler->search_filter, FN_NAME_SIZE, 0)) {
				SV_LOG_INFO("Mod\n");
			}

			data.height = (GuiDimension){ 15.f, GuiUnit_Pixel };
			data.y = (GuiCoord){ 35.f, GuiUnit_Pixel, GuiCoordAlign_InverseTop };
			gui_free_layout_update(data);

			gui_checkbox("Only Functions", &only_functions, 0);

			data.y = (GuiCoord){ 55.f, GuiUnit_Pixel, GuiCoordAlign_InverseTop };
			gui_free_layout_update(data);

			gui_checkbox("Sort", &sort, 0);

			gui_end_parent();
		}
	}

	gui_stack_layout_update_size(0.8f, GuiUnit_Relative, 15.f, GuiUnit_Pixel);

	mutex_lock(profiler->mutex);

	foreach(i, profiler->function_count) {

		FunctionData* fn = profiler->functions + i;

		f64 total_time = 0.0;
		
		u32 add = SV_MIN(20, fn->current_frame);

		for (u32 i = fn->current_frame - add; i <= fn->current_frame; ++i) {

			const FunctionFrame* frame = fn->frames + (i % FUNCTION_CACHE);

			total_time += frame->total_time / (f64)add;
		}

		fn->total_time = total_time;
	}

	if (sort && core.frame_count % 3 == 0)
		array_sort(profiler->functions, profiler->function_count, sizeof(FunctionData), function_less_than);

	foreach(i, profiler->function_count) {

		FunctionData* fn = profiler->functions + i;

		if (only_functions && !fn->is_function)
			continue;

		const FunctionFrame* frame = fn->frames + (fn->current_frame % FUNCTION_CACHE);
		
		u32 frame_calls = frame->calls;
		f64 frame_total_time = frame->total_time * 1000.0;

		f64 total_time = fn->total_time * 1000.0;

		f32 frame_average = frame_calls ? ((f32)(frame_total_time / (f64)frame_calls)) : 0.f;

		// TODO
		if (1) {
			gui_text("%s: %fms", fn->name, (f32)total_time);
		}
		else {
			gui_text("%s", fn->name);
			gui_text("   Calls: %u", frame_calls);
			gui_text("   Total: %fms", (f32)frame_total_time);
			gui_text("   Average: %fms", frame_average);
		}
	}

	mutex_unlock(profiler->mutex);

	gui_end_parent();
}

#endif