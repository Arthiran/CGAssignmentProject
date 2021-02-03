#version 440

layout (location = 0) in vec2 inUV;

out vec4 frag_colour;

layout (binding = 0) uniform sampler2D s_screenTex;

uniform float u_Threshold;
uniform bool u_ApplyBloom = false;

//WIP Bloom
void main() {
	vec3 source = texture2D(s_screenTex, inUV).rgb;

	float luminance = dot(source, vec3(0.299, 0.587, 0.114));

	if (u_ApplyBloom)
	{
		frag_colour.rgb = source * step(u_Threshold, luminance); 
		frag_colour.a = 1.0;

	}
	else
	{
		frag_colour.rgb = source;
		frag_colour.a = 1.0;
	}
}