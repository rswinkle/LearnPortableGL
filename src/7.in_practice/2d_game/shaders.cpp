#include "shaders.h"
#include "game_uniforms.h"
#include <cmath>

static glm::vec4 toglm(pgl_vec4 v)
{
	return glm::vec4(v.x, v.y, v.z, v.w);
}

void sprite_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	GameUniforms* u = (GameUniforms*)uniforms;
	glm::vec4 vertex = ((glm::vec4*)vertex_attribs)[0];
	*(glm::vec2*)&vs_output[0] = glm::vec2(vertex.z, vertex.w);
	*(glm::vec4*)&builtins->gl_Position = u->projection * u->model * glm::vec4(vertex.x, vertex.y, 0.0f, 1.0f);
}

void sprite_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	GameUniforms* u = (GameUniforms*)uniforms;
	glm::vec2 TexCoords = *(glm::vec2*)&fs_input[0];
	glm::vec4 sampled = toglm(texture2D(u->tex, TexCoords.x, TexCoords.y));
	*(glm::vec4*)&builtins->gl_FragColor = glm::vec4(u->spriteColor, 1.0f) * sampled;
}

void particle_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	GameUniforms* u = (GameUniforms*)uniforms;
	glm::vec4 vertex = ((glm::vec4*)vertex_attribs)[0];
	float scale = 10.0f;
	*(glm::vec2*)&vs_output[0] = glm::vec2(vertex.z, vertex.w);
	*(glm::vec4*)&vs_output[2] = u->particleColor;
	*(glm::vec4*)&builtins->gl_Position = u->projection * glm::vec4((glm::vec2(vertex.x, vertex.y) * scale) + u->offset, 0.0f, 1.0f);
}

void particle_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	GameUniforms* u = (GameUniforms*)uniforms;
	glm::vec2 TexCoords = *(glm::vec2*)&fs_input[0];
	glm::vec4 ParticleColor = *(glm::vec4*)&fs_input[2];
	*(glm::vec4*)&builtins->gl_FragColor = toglm(texture2D(u->tex, TexCoords.x, TexCoords.y)) * ParticleColor;
}

void post_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	GameUniforms* u = (GameUniforms*)uniforms;
	glm::vec4 vertex = ((glm::vec4*)vertex_attribs)[0];
	glm::vec4 pos = glm::vec4(vertex.x, vertex.y, 0.0f, 1.0f);
	glm::vec2 texture = glm::vec2(vertex.z, vertex.w);
	if (u->chaos)
	{
		float strength = 0.3f;
		texture = glm::vec2(texture.x + sin(u->time) * strength, texture.y + cos(u->time) * strength);
	}
	else if (u->confuse)
		texture = glm::vec2(1.0f - texture.x, 1.0f - texture.y);
	if (u->shake)
	{
		float strength = 0.01f;
		pos.x += cos(u->time * 10.0f) * strength;
		pos.y += cos(u->time * 15.0f) * strength;
	}
	*(glm::vec2*)&vs_output[0] = texture;
	*(glm::vec4*)&builtins->gl_Position = pos;
}

void post_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	GameUniforms* u = (GameUniforms*)uniforms;
	glm::vec2 TexCoords = *(glm::vec2*)&fs_input[0];
	glm::vec4 color(0.0f);
	if (u->chaos || u->shake)
	{
		glm::vec3 sample[9];
		for (int i = 0; i < 9; i++)
		{
			glm::vec2 uv = TexCoords + u->offsets[i];
			sample[i] = glm::vec3(toglm(texture2D(u->tex, uv.x, uv.y)));
		}
		if (u->chaos)
		{
			for (int i = 0; i < 9; i++)
				color += glm::vec4(sample[i] * (float)u->edge_kernel[i], 0.0f);
			color.a = 1.0f;
		}
		else
		{
			for (int i = 0; i < 9; i++)
				color += glm::vec4(sample[i] * u->blur_kernel[i], 0.0f);
			color.a = 1.0f;
		}
	}
	else if (u->confuse)
	{
		glm::vec3 rgb = glm::vec3(toglm(texture2D(u->tex, TexCoords.x, TexCoords.y)));
		color = glm::vec4(1.0f - rgb, 1.0f);
	}
	else
		color = toglm(texture2D(u->tex, TexCoords.x, TexCoords.y));
	*(glm::vec4*)&builtins->gl_FragColor = color;
}

void text_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	GameUniforms* u = (GameUniforms*)uniforms;
	glm::vec4 vertex = ((glm::vec4*)vertex_attribs)[0];
	*(glm::vec2*)&vs_output[0] = glm::vec2(vertex.z, vertex.w);
	*(glm::vec4*)&builtins->gl_Position = u->projection * glm::vec4(vertex.x, vertex.y, 0.0f, 1.0f);
}

void text_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	GameUniforms* u = (GameUniforms*)uniforms;
	glm::vec2 TexCoords = *(glm::vec2*)&fs_input[0];
	float a = toglm(texture2D(u->tex, TexCoords.x, TexCoords.y)).r;
	*(glm::vec4*)&builtins->gl_FragColor = glm::vec4(u->textColor, 1.0f) * glm::vec4(1.0f, 1.0f, 1.0f, a);
}
