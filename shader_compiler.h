#pragma once

#include "Hosebase/graphics.h"

#if SV_SHADER_COMPILER

typedef struct {
	const char* name;
	const char* value;
} ShaderMacro;

typedef struct {

	GraphicsAPI	      api;
	u32	 	          major_version;
	u32		          minor_version;
	const char*	      entry_point;
	ShaderMacro*      macros;
	u32               macro_count;
	
} ShaderCompileDesc;

b8 shader_compiler_initialize();
b8 shader_compiler_close();

b8 shader_compile(ShaderDesc* result, const ShaderCompileDesc* desc, const char* filepath, b8 save);

#endif

b8 shader_read_binary(ShaderDesc* result, GraphicsAPI api, const char* filepath);