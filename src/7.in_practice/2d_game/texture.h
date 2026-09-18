/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#ifndef TEXTURE_H
#define TEXTURE_H

#include "pgl_gl.h"

class Texture2D
{
public:
	unsigned int ID;
	unsigned int Width, Height;
	unsigned int Internal_Format;
	unsigned int Image_Format;
	unsigned int Wrap_S;
	unsigned int Wrap_T;
	unsigned int Filter_Min;
	unsigned int Filter_Max;
	Texture2D();
	void Generate(unsigned int width, unsigned int height, unsigned char* data);
	void Bind() const;
};

#endif
