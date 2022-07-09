#include "core.hlsl"

SV_CONSTANT_BUFFER(buffer, b0) {
	matrix tm;
};

#shader vertex

struct Input {
	float2 position : Position;
    float2 texcoord : Texcoord;
	float4 color : Color;
};

struct Output {
	float2 texcoord : FragTexcoord;
	float4 color : FragColor;
	float4 position : SV_Position;
};

Output main(Input input)
{
	Output output;
	output.position = mul(float4(input.position, 0.f, 1.f), tm);
	output.color = input.color;
	output.texcoord = input.texcoord;
	return output;
}
