#pragma once

#include <glm/glm.hpp>
#include "pgl_gl.h"

using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

struct GameUniforms
{
	mat4 model;
	mat4 projection;
	GLuint tex;
	vec3 spriteColor;
	vec2 offset;
	vec4 particleColor;
	vec2 offsets[9];
	int edge_kernel[9];
	float blur_kernel[9];
	int chaos;
	int confuse;
	int shake;
	float time;
	vec3 textColor;
};

extern GameUniforms uniforms;
