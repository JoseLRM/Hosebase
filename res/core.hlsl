#ifdef SV_API_VULKAN

// VULKAN API
#ifdef SV_VERTEX_SHADER
#define SV_VK_RESOUCE_SET space0
#elif SV_PIXEL_SHADER
#define SV_VK_RESOUCE_SET space1
#elif SV_GEOMETRY_SHADER
#define SV_VK_RESOUCE_SET space2
#elif SV_COMPUTE_SHADER
#define SV_VK_RESOUCE_SET space0
#else
#error "SHADER NOT SUPPORTED"
#endif

#define SV_CONSTANT_BUFFER(name, binding) cbuffer name : register(binding, SV_VK_RESOUCE_SET)
#define SV_BUFFER(name, temp, binding) Buffer<temp> name : register(binding, SV_VK_RESOUCE_SET)
#define SV_TEXTURE(name, binding) Texture2D name : register(binding, SV_VK_RESOUCE_SET)
#define SV_UAV_TEXTURE(name, temp, binding) RWTexture2D<temp> name : register(binding, SV_VK_RESOUCE_SET)
#define SV_CUBE_TEXTURE(name, binding) TextureCube name : register(binding, SV_VK_RESOUCE_SET)
#define SV_SAMPLER(name, binding) SamplerState name : register(binding, SV_VK_RESOUCE_SET)

#else
#error "API NOT SUPPORTED"
#endif

typedef unsigned int    u32;
typedef int             i32;
typedef float           f32;
typedef double          f64;
typedef matrix          Mat4;
typedef float2          v2;
typedef float3          v3;
typedef float4          v4;

// Macros

#define foreach(_it, _end) for (u32 _it = 0u; _it < _end; ++_it)
#define SV_BIT(x) (1ULL << x) 
#define SV_GLOBAL static const

SV_GLOBAL u32 u32_max = 0xFFFFFFFF;

// Helper functions

float3 compute_fragment_position(f32 depth, float2 texcoord, matrix ipm)
{
	float4 pos = float4(texcoord.x * 2.f - 1.f, texcoord.y * 2.f - 1.f, depth, 1.f);
	pos = mul(pos, ipm);
	return pos.xyz / pos.w;
}