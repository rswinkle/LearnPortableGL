/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#include "resource_manager.h"

#include <iostream>
#include <stb_image.h>

std::map<std::string, Texture2D> ResourceManager::Textures;
std::map<std::string, Shader>    ResourceManager::Shaders;

Shader ResourceManager::LoadShader(vert_func vs, frag_func fs, int n, GLenum *smooth, GLboolean discard, std::string name)
{
	Shader shader;
	shader.ID = pglCreateProgram(vs, fs, n, smooth, discard);
	Shaders[name] = shader;
	return shader;
}

Shader& ResourceManager::GetShader(std::string name)
{
	return Shaders[name];
}

Texture2D ResourceManager::LoadTexture(const char *file, bool alpha, std::string name)
{
	Textures[name] = loadTextureFromFile(file, alpha);
	return Textures[name];
}

Texture2D& ResourceManager::GetTexture(std::string name)
{
	return Textures[name];
}

void ResourceManager::Clear()
{
	for (auto iter : Shaders)
		glDeleteProgram(iter.second.ID);
	for (auto iter : Textures)
		glDeleteTextures(1, &iter.second.ID);
}

Texture2D ResourceManager::loadTextureFromFile(const char *file, bool alpha)
{
	(void)alpha;
	Texture2D texture;
	texture.Internal_Format = GL_RGBA;
	texture.Image_Format = GL_RGBA;
	int width, height, nrChannels;
	unsigned char* data = stbi_load(file, &width, &height, &nrChannels, STBI_rgb_alpha);
	if (!data)
		std::cout << "Texture failed to load at path: " << file << std::endl;
	texture.Generate(width, height, data);
	stbi_image_free(data);
	return texture;
}
