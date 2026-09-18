/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#include "shader.h"
#include "game_uniforms.h"

Shader &Shader::Use()
{
	glUseProgram(this->ID);
	pglSetUniform(&uniforms);
	return *this;
}
