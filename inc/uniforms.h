#ifndef UNIFORMS_H
#define UNIFORMS_H

#include <glm/glm.hpp>


struct Material
{
	GLuint diffuse;
	GLuint specular;
	float shininess;
};

struct DirLight
{
	glm::vec3 direction;

	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;

	float constant;
	float linear;
	float quadratic;
};

struct PointLight
{
	glm::vec3 position;

	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;

	float constant;
	float linear;
	float quadratic;
};

struct SpotLight
{
	glm::vec3 position;
	glm::vec3 direction;

	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;

	float constant;
	float linear;
	float quadratic;

	float cutOff;
	float outerCutOff;
};

struct ADS_Uniforms
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;

	Material material;

	DirLight dirLight;
	PointLight pointLights[4];
	SpotLight spotLight;

	glm::vec3 viewPos;
};

struct Model_Uniforms
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
	glm::mat4 mvp;

	GLuint texture_diffuse[4];
	GLuint texture_specular[4];
	GLuint texture_normal[4];
	GLuint texture_height[4];
};

#endif
