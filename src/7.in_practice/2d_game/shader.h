/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#ifndef SHADER_H
#define SHADER_H

class Shader
{
public:
	unsigned int ID;
	Shader() : ID(0) { }
	Shader &Use();
};

#endif
