/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <map>
#include <string>

#include "pgl_gl.h"
#include "texture.h"
#include "shader.h"

class ResourceManager
{
public:
	static std::map<std::string, Shader>    Shaders;
	static std::map<std::string, Texture2D> Textures;
	static Shader    LoadShader(vert_func vs, frag_func fs, int n, GLenum *smooth, GLboolean discard, std::string name);
	static Shader&   GetShader(std::string name);
	static Texture2D LoadTexture(const char *file, bool alpha, std::string name);
	static Texture2D& GetTexture(std::string name);
	static void      Clear();
private:
	ResourceManager() { }
	static Texture2D loadTextureFromFile(const char *file, bool alpha);
};

#endif
