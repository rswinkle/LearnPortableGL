// Shared IBL geometry, float cubemap setup, and Cook-Torrance helpers for ch6.
// Env / irradiance / prefilter are GL_TEXTURE_CUBE_MAP + GL_RGBA16F (no RGB16F).
#pragma once

#include <vector>
#include <cmath>
#include <iostream>

const float PI = 3.14159265359f;

#ifndef NDEBUG
const unsigned int ENV_SIZE = 256;
const unsigned int IRR_SIZE = 32;
const unsigned int PREFILTER_SIZE = 64;
const unsigned int BRDF_SIZE = 256;
const unsigned int X_SEGMENTS = 32;
const unsigned int Y_SEGMENTS = 32;
const int PREFILTER_SAMPLES = 64;
const int BRDF_SAMPLES = 128;
const float SAMPLE_DELTA = 0.05f;
#else
const unsigned int ENV_SIZE = 512;
const unsigned int IRR_SIZE = 32;
const unsigned int PREFILTER_SIZE = 128;
const unsigned int BRDF_SIZE = 512;
const unsigned int X_SEGMENTS = 64;
const unsigned int Y_SEGMENTS = 64;
const int PREFILTER_SAMPLES = 128;
const int BRDF_SAMPLES = 256;
const float SAMPLE_DELTA = 0.025f;
#endif
const unsigned int MAX_PREFILTER_MIPS = 5;

static vec4 toglm(pgl_vec4 v)
{
	return vec4(v.x, v.y, v.z, v.w);
}

static unsigned int make_float_cubemap(unsigned int size, GLenum minFilter)
{
	unsigned int t;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_CUBE_MAP, t);
	for (unsigned int i = 0; i < 6; ++i)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F, size, size, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return t;
}

static vec3 sample_cubemap(GLuint cube, vec3 v)
{
	return vec3(toglm(texture_cubemap(cube, v.x, v.y, v.z)));
}

static vec3 sample_cubemap_lod(GLuint cube, vec3 v, float lod)
{
	return vec3(toglm(texture_cubemapLod(cube, v.x, v.y, v.z, lod)));
}

static float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0f);
	float NdotH2 = NdotH * NdotH;
	float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
	denom = PI * denom * denom;
	return a2 / denom;
}

static float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r * r) / 8.0f;
	return NdotV / (NdotV * (1.0f - k) + k);
}

static float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	return GeometrySchlickGGX(max(dot(N, V), 0.0f), roughness) *
	       GeometrySchlickGGX(max(dot(N, L), 0.0f), roughness);
}

static float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
	float a = roughness;
	float k = (a * a) / 2.0f;
	return NdotV / (NdotV * (1.0f - k) + k);
}

static float GeometrySmith_IBL(vec3 N, vec3 V, vec3 L, float roughness)
{
	return GeometrySchlickGGX_IBL(max(dot(N, V), 0.0f), roughness) *
	       GeometrySchlickGGX_IBL(max(dot(N, L), 0.0f), roughness);
}

static vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

static vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0f - roughness), F0) - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

static float RadicalInverse_VdC(unsigned int bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f;
}

static vec2 Hammersley(unsigned int i, unsigned int N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

static vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;
	float phi = 2.0f * PI * Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	vec3 H(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	vec3 up = abs(N.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

static unsigned int loadTextureRGBA(const char *path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	int width, height, nrComponents;
	unsigned char *data = stbi_load(path, &width, &height, &nrComponents, STBI_rgb_alpha);
	if (data)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}
	return textureID;
}

static void renderSphere()
{
	static unsigned int sphereVAO = 0;
	static unsigned int indexCount = 0;
	if (sphereVAO == 0)
	{
		glGenVertexArrays(1, &sphereVAO);
		unsigned int vbo, ebo;
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);

		std::vector<vec3> positions, normals, tangents;
		std::vector<vec2> uv;
		std::vector<unsigned int> indices;

		for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
		{
			for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
			{
				float xSegment = (float)x / (float)X_SEGMENTS;
				float ySegment = (float)y / (float)Y_SEGMENTS;
				float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
				float yPos = std::cos(ySegment * PI);
				float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
				vec3 n(xPos, yPos, zPos);
				vec3 up = (fabs(n.y) < 0.999f) ? vec3(0.0f, 1.0f, 0.0f) : vec3(1.0f, 0.0f, 0.0f);
				positions.push_back(n);
				uv.push_back(vec2(xSegment, ySegment));
				normals.push_back(n);
				tangents.push_back(normalize(cross(up, n)));
			}
		}
		for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
		{
			for (unsigned int x = 0; x < X_SEGMENTS; ++x)
			{
				unsigned int i0 = y * (X_SEGMENTS + 1) + x;
				unsigned int i1 = (y + 1) * (X_SEGMENTS + 1) + x;
				unsigned int i2 = y * (X_SEGMENTS + 1) + (x + 1);
				unsigned int i3 = (y + 1) * (X_SEGMENTS + 1) + (x + 1);
				indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
				indices.push_back(i2); indices.push_back(i1); indices.push_back(i3);
			}
		}
		indexCount = (unsigned int)indices.size();

		std::vector<float> data;
		for (unsigned int i = 0; i < positions.size(); ++i)
		{
			data.push_back(positions[i].x); data.push_back(positions[i].y); data.push_back(positions[i].z);
			data.push_back(normals[i].x); data.push_back(normals[i].y); data.push_back(normals[i].z);
			data.push_back(uv[i].x); data.push_back(uv[i].y);
			data.push_back(tangents[i].x); data.push_back(tangents[i].y); data.push_back(tangents[i].z);
		}
		glBindVertexArray(sphereVAO);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
		unsigned int stride = 11 * sizeof(float);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
		glEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, 3 * sizeof(float));
		glEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, 6 * sizeof(float));
		glEnableVertexAttribArray(3);
		pglVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, 8 * sizeof(float));
	}
	glBindVertexArray(sphereVAO);
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

static void renderCube()
{
	static unsigned int cubeVAO = 0;
	static unsigned int cubeVBO = 0;
	if (cubeVAO == 0)
	{
		float vertices[] = {
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
			 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
			 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
			 1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
			 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
			-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
			 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
			 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
			-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
		glEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 3 * sizeof(float));
		glEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 6 * sizeof(float));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

static void renderQuad()
{
	static unsigned int quadVAO = 0;
	static unsigned int quadVBO = 0;
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f
		};
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
		glEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 3 * sizeof(float));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

static void fill_capture_views(mat4 views[6])
{
	views[0] = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3( 1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f));
	views[1] = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f));
	views[2] = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f));
	views[3] = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f));
	views[4] = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f));
	views[5] = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3( 0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f));
}
