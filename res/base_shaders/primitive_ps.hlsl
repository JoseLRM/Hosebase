#include "core.hlsl"

#shader pixel

struct Input {
	float2 texcoord : FragTexcoord;
	float4 color : FragColor;
};

SV_TEXTURE(tex, t0);
SV_SAMPLER(sam, s0);

float4 main(Input input) : SV_Target0
{
	float4 color = tex.Sample(sam, input.texcoord) * input.color;
	if (color.a < 0.0001f)
		discard;
	return color;
}