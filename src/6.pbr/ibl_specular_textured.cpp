#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>

#include <iostream>
#include <cmath>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

using namespace glm;

#include "ibl_helpers.h"

struct My_Uniforms
{
	mat4 model;
	mat4 view;
	mat4 projection;
	mat3 normalMatrix;

	GLuint albedoMap;
	GLuint normalMap;
	GLuint metallicMap;
	GLuint roughnessMap;
	GLuint aoMap;

	vec3 lightPositions[4];
	vec3 lightColors[4];
	vec3 camPos;

	GLuint hdrTexture;
	GLuint envCubemap;
	GLuint irradianceMap;
	GLuint prefilterMap;
	GLuint brdfLUT;
	float sampleDelta;
	int sampleCount;
	float roughness;
};

void setup_context();
void cleanup();
bool handle_events();

void pbr_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void pbr_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void cubemap_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void equirect_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void irradiance_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void prefilter_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void brdf_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void brdf_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void background_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void background_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

unsigned int scr_width = 640;
unsigned int scr_height = 480;
Camera camera(vec3(0.0f, 0.0f, 3.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;
SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;
My_Uniforms uniforms;

static void bind_material(unsigned int albedo, unsigned int normal, unsigned int metallic, unsigned int roughness, unsigned int ao)
{
	uniforms.albedoMap = albedo;
	uniforms.normalMap = normal;
	uniforms.metallicMap = metallic;
	uniforms.roughnessMap = roughness;
	uniforms.aoMap = ao;
}

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	GLenum smoothPbr[] = { PGL_SMOOTH2, PGL_SMOOTH3, PGL_SMOOTH3, PGL_SMOOTH3 };
	GLuint pbrShader = pglCreateProgram(pbr_vs, pbr_fs, 11, smoothPbr, GL_FALSE);
	glUseProgram(pbrShader);
	pglSetUniform(&uniforms);

	GLenum smooth3[] = { PGL_SMOOTH3 };
	GLuint equirectShader = pglCreateProgram(cubemap_vs, equirect_fs, 3, smooth3, GL_FALSE);
	glUseProgram(equirectShader);
	pglSetUniform(&uniforms);
	GLuint irradianceShader = pglCreateProgram(cubemap_vs, irradiance_fs, 3, smooth3, GL_FALSE);
	glUseProgram(irradianceShader);
	pglSetUniform(&uniforms);
	GLuint prefilterShader = pglCreateProgram(cubemap_vs, prefilter_fs, 3, smooth3, GL_FALSE);
	glUseProgram(prefilterShader);
	pglSetUniform(&uniforms);
	GLuint backgroundShader = pglCreateProgram(background_vs, background_fs, 3, smooth3, GL_FALSE);
	glUseProgram(backgroundShader);
	pglSetUniform(&uniforms);
	GLenum smooth2[] = { PGL_SMOOTH2 };
	GLuint brdfShader = pglCreateProgram(brdf_vs, brdf_fs, 2, smooth2, GL_FALSE);
	glUseProgram(brdfShader);
	pglSetUniform(&uniforms);

	unsigned int ironAlbedo = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/rusted_iron/albedo.png").c_str());
	unsigned int ironNormal = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/rusted_iron/normal.png").c_str());
	unsigned int ironMetallic = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/rusted_iron/metallic.png").c_str());
	unsigned int ironRoughness = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/rusted_iron/roughness.png").c_str());
	unsigned int ironAO = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/rusted_iron/ao.png").c_str());

	unsigned int goldAlbedo = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/gold/albedo.png").c_str());
	unsigned int goldNormal = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/gold/normal.png").c_str());
	unsigned int goldMetallic = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/gold/metallic.png").c_str());
	unsigned int goldRoughness = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/gold/roughness.png").c_str());
	unsigned int goldAO = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/gold/ao.png").c_str());

	unsigned int grassAlbedo = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/grass/albedo.png").c_str());
	unsigned int grassNormal = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/grass/normal.png").c_str());
	unsigned int grassMetallic = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/grass/metallic.png").c_str());
	unsigned int grassRoughness = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/grass/roughness.png").c_str());
	unsigned int grassAO = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/grass/ao.png").c_str());

	unsigned int plasticAlbedo = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/plastic/albedo.png").c_str());
	unsigned int plasticNormal = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/plastic/normal.png").c_str());
	unsigned int plasticMetallic = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/plastic/metallic.png").c_str());
	unsigned int plasticRoughness = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/plastic/roughness.png").c_str());
	unsigned int plasticAO = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/plastic/ao.png").c_str());

	unsigned int wallAlbedo = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/wall/albedo.png").c_str());
	unsigned int wallNormal = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/wall/normal.png").c_str());
	unsigned int wallMetallic = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/wall/metallic.png").c_str());
	unsigned int wallRoughness = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/wall/roughness.png").c_str());
	unsigned int wallAO = loadTextureRGBA(FileSystem::getPath("resources/textures/pbr/wall/ao.png").c_str());

	uniforms.sampleDelta = SAMPLE_DELTA;

	vec3 lightPositions[] = {
		vec3(-10.0f,  10.0f, 10.0f),
		vec3( 10.0f,  10.0f, 10.0f),
		vec3(-10.0f, -10.0f, 10.0f),
		vec3( 10.0f, -10.0f, 10.0f),
	};
	vec3 lightColors[] = {
		vec3(300.0f, 300.0f, 300.0f),
		vec3(300.0f, 300.0f, 300.0f),
		vec3(300.0f, 300.0f, 300.0f),
		vec3(300.0f, 300.0f, 300.0f)
	};
	for (int i = 0; i < 4; i++) {
		uniforms.lightPositions[i] = lightPositions[i];
		uniforms.lightColors[i] = lightColors[i];
	}

	unsigned int captureFBO, captureRBO;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, ENV_SIZE, ENV_SIZE);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	stbi_set_flip_vertically_on_load(true);
	int width, height, nrComponents;
	float *hdr = stbi_loadf(FileSystem::getPath("resources/textures/hdr/newport_loft.hdr").c_str(), &width, &height, &nrComponents, 4);
	unsigned int hdrTexture = 0;
	if (hdr)
	{
		glGenTextures(1, &hdrTexture);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, hdr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(hdr);
	}
	else
		std::cout << "Failed to load HDR image." << std::endl;
	uniforms.hdrTexture = hdrTexture;
	stbi_set_flip_vertically_on_load(false);

	uniforms.envCubemap = make_float_cubemap(ENV_SIZE, GL_LINEAR_MIPMAP_LINEAR);
	uniforms.irradianceMap = make_float_cubemap(IRR_SIZE, GL_LINEAR);
	uniforms.prefilterMap = make_float_cubemap(PREFILTER_SIZE, GL_LINEAR_MIPMAP_LINEAR);
	glBindTexture(GL_TEXTURE_CUBE_MAP, uniforms.prefilterMap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	mat4 captureProjection = perspective(radians(90.0f), 1.0f, 0.1f, 10.0f);
	mat4 captureViews[6];
	fill_capture_views(captureViews);

	std::cout << "converting equirectangular HDR to cubemap (" << ENV_SIZE << ")..." << std::endl;
	glUseProgram(equirectShader);
	uniforms.projection = captureProjection;
	glViewport(0, 0, ENV_SIZE, ENV_SIZE);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		uniforms.view = captureViews[i];
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, uniforms.envCubemap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		renderCube();
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, uniforms.envCubemap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	std::cout << "convolving irradiance (" << IRR_SIZE << ")..." << std::endl;
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, IRR_SIZE, IRR_SIZE);
	glUseProgram(irradianceShader);
	glViewport(0, 0, IRR_SIZE, IRR_SIZE);
	for (unsigned int i = 0; i < 6; ++i)
	{
		std::cout << "  irr face " << i << std::endl;
		uniforms.view = captureViews[i];
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, uniforms.irradianceMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		renderCube();
	}

	std::cout << "prefiltering env (" << PREFILTER_SIZE << ")..." << std::endl;
	glUseProgram(prefilterShader);
	uniforms.sampleCount = PREFILTER_SAMPLES;
	for (unsigned int mip = 0; mip < MAX_PREFILTER_MIPS; ++mip)
	{
		unsigned int mipSize = (unsigned int)(PREFILTER_SIZE * std::pow(0.5, mip));
		if (mipSize < 1) mipSize = 1;
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
		glViewport(0, 0, mipSize, mipSize);
		uniforms.roughness = (float)mip / (float)(MAX_PREFILTER_MIPS - 1);
		std::cout << "  mip " << mip << " (" << mipSize << ")" << std::endl;
		for (unsigned int i = 0; i < 6; ++i)
		{
			uniforms.view = captureViews[i];
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, uniforms.prefilterMap, mip);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			renderCube();
		}
	}

	std::cout << "integrating BRDF LUT (" << BRDF_SIZE << ")..." << std::endl;
	glGenTextures(1, &uniforms.brdfLUT);
	glBindTexture(GL_TEXTURE_2D, uniforms.brdfLUT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, BRDF_SIZE, BRDF_SIZE, 0, GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, BRDF_SIZE, BRDF_SIZE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, uniforms.brdfLUT, 0);
	glViewport(0, 0, BRDF_SIZE, BRDF_SIZE);
	glUseProgram(brdfShader);
	uniforms.sampleCount = BRDF_SAMPLES;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	renderQuad();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, scr_width, scr_height);
	std::cout << "done." << std::endl;

	while (true)
	{
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (handle_events())
			break;

		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(pbrShader);
		mat4 view = camera.GetViewMatrix();
		uniforms.view = view;
		uniforms.camPos = camera.Position;
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);

		mat4 model;

		bind_material(ironAlbedo, ironNormal, ironMetallic, ironRoughness, ironAO);
		model = translate(mat4(1.0f), vec3(-5.0f, 0.0f, 2.0f));
		uniforms.model = model;
		uniforms.normalMatrix = transpose(inverse(mat3(model)));
		renderSphere();

		bind_material(goldAlbedo, goldNormal, goldMetallic, goldRoughness, goldAO);
		model = translate(mat4(1.0f), vec3(-3.0f, 0.0f, 2.0f));
		uniforms.model = model;
		uniforms.normalMatrix = transpose(inverse(mat3(model)));
		renderSphere();

		bind_material(grassAlbedo, grassNormal, grassMetallic, grassRoughness, grassAO);
		model = translate(mat4(1.0f), vec3(-1.0f, 0.0f, 2.0f));
		uniforms.model = model;
		uniforms.normalMatrix = transpose(inverse(mat3(model)));
		renderSphere();

		bind_material(plasticAlbedo, plasticNormal, plasticMetallic, plasticRoughness, plasticAO);
		model = translate(mat4(1.0f), vec3(1.0f, 0.0f, 2.0f));
		uniforms.model = model;
		uniforms.normalMatrix = transpose(inverse(mat3(model)));
		renderSphere();

		bind_material(wallAlbedo, wallNormal, wallMetallic, wallRoughness, wallAO);
		model = translate(mat4(1.0f), vec3(3.0f, 0.0f, 2.0f));
		uniforms.model = model;
		uniforms.normalMatrix = transpose(inverse(mat3(model)));
		renderSphere();

		for (int i = 0; i < 4; ++i)
		{
			model = translate(mat4(1.0f), lightPositions[i]);
			model = scale(model, vec3(0.5f));
			uniforms.model = model;
			uniforms.normalMatrix = transpose(inverse(mat3(model)));
			renderSphere();
		}

		glUseProgram(backgroundShader);
		uniforms.view = view;
		renderCube();

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

bool handle_events()
{
	SDL_Event event;
	SDL_Scancode sc;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			return true;
		case SDL_KEYDOWN:
			sc = event.key.keysym.scancode;
			if (sc == SDL_SCANCODE_ESCAPE)
				return true;
			break;
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				SDL_SetRelativeMouseMode(SDL_TRUE);
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				SDL_SetRelativeMouseMode(SDL_FALSE);
				break;
			case SDL_WINDOWEVENT_RESIZED:
				scr_width = event.window.data1;
				scr_height = event.window.data2;
				pglResizeFramebuffer(scr_width, scr_height);
				bbufpix = (pix_t*)pglGetBackBuffer();
				glViewport(0, 0, scr_width, scr_height);
				SDL_DestroyTexture(tex);
				tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
				break;
			}
			break;
		case SDL_MOUSEMOTION:
			camera.ProcessMouseMovement((float)event.motion.xrel, -(float)event.motion.yrel);
			break;
		case SDL_MOUSEWHEEL:
			camera.ProcessMouseScroll(event.wheel.y);
			break;
		}
	}
	const Uint8 *state = SDL_GetKeyboardState(NULL);
	if (state[SDL_SCANCODE_W]) camera.ProcessKeyboard(FORWARD, deltaTime);
	if (state[SDL_SCANCODE_S]) camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (state[SDL_SCANCODE_A]) camera.ProcessKeyboard(LEFT, deltaTime);
	if (state[SDL_SCANCODE_D]) camera.ProcessKeyboard(RIGHT, deltaTime);
	return false;
}

void setup_context()
{
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO)) {
		std::cout << "SDL_Init error: " << SDL_GetError() << "\n";
		exit(0);
	}
	window = SDL_CreateWindow("LearnPortablGL", 100, 100, scr_width, scr_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (!window) {
		std::cerr << "Failed to create window\n";
		SDL_Quit();
		exit(0);
	}
	ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
	if (!init_glContext(&the_Context, &bbufpix, scr_width, scr_height)) {
		puts("Failed to initialize glContext");
		exit(0);
	}
	set_glContext(&the_Context);
}

void cleanup()
{
	free_glContext(&the_Context);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void pbr_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec3 WorldPos = vec3(u->model * aPos);
	*(vec2*)&vs_output[0] = vec2(((vec4*)vertex_attribs)[2]);
	*(vec3*)&vs_output[2] = WorldPos;
	*(vec3*)&vs_output[5] = u->normalMatrix * vec3(((vec4*)vertex_attribs)[1]);
	*(vec3*)&vs_output[8] = u->normalMatrix * vec3(((vec4*)vertex_attribs)[3]);
	*(vec4*)&builtins->gl_Position = u->projection * u->view * vec4(WorldPos, 1.0f);
}

void cubemap_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 WorldPos = vec3(((vec4*)vertex_attribs)[0]);
	*(vec3*)&vs_output[0] = WorldPos;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * vec4(WorldPos, 1.0f);
}

void background_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 WorldPos = vec3(((vec4*)vertex_attribs)[0]);
	*(vec3*)&vs_output[0] = WorldPos;
	mat4 rotView = mat4(mat3(u->view));
	vec4 clipPos = u->projection * rotView * vec4(WorldPos, 1.0f);
	clipPos.z = clipPos.w;
	*(vec4*)&builtins->gl_Position = clipPos;
}

void brdf_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	*(vec2*)&vs_output[0] = vec2(((vec4*)vertex_attribs)[1]);
	*(vec4*)&builtins->gl_Position = ((vec4*)vertex_attribs)[0];
}

void pbr_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];
	vec3 WorldPos = *(vec3*)&fs_input[2];
	vec3 Normal = *(vec3*)&fs_input[5];
	vec3 Tangent = *(vec3*)&fs_input[8];

	vec3 albedo = pow(vec3(toglm(texture2D(u->albedoMap, TexCoords.x, TexCoords.y))), vec3(2.2f));
	float metallic = toglm(texture2D(u->metallicMap, TexCoords.x, TexCoords.y)).r;
	float roughness = toglm(texture2D(u->roughnessMap, TexCoords.x, TexCoords.y)).r;
	float ao = toglm(texture2D(u->aoMap, TexCoords.x, TexCoords.y)).r;

	vec3 tangentNormal = vec3(toglm(texture2D(u->normalMap, TexCoords.x, TexCoords.y))) * 2.0f - 1.0f;
	vec3 N = normalize(Normal);
	vec3 T = normalize(Tangent);
	T = normalize(T - N * dot(N, T));
	vec3 B = cross(N, T);
	N = normalize(mat3(T, B, N) * tangentNormal);

	vec3 V = normalize(u->camPos - WorldPos);
	vec3 R = reflect(-V, N);
	vec3 F0 = mix(vec3(0.04f), albedo, metallic);

	vec3 Lo = vec3(0.0f);
	for (int i = 0; i < 4; ++i)
	{
		vec3 L = normalize(u->lightPositions[i] - WorldPos);
		vec3 H = normalize(V + L);
		float distance = length(u->lightPositions[i] - WorldPos);
		vec3 radiance = u->lightColors[i] * (1.0f / (distance * distance));
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0f), F0);
		vec3 specular = (DistributionGGX(N, H, roughness) * GeometrySmith(N, V, L, roughness) * F) /
			(4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f);
		vec3 kD = (vec3(1.0f) - F) * (1.0f - metallic);
		Lo += (kD * albedo / PI + specular) * radiance * max(dot(N, L), 0.0f);
	}

	vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0f), F0, roughness);
	vec3 kD = (vec3(1.0f) - F) * (1.0f - metallic);
	vec3 irradiance = sample_cubemap(u->irradianceMap, N);
	vec3 diffuse = irradiance * albedo;
	vec3 prefilteredColor = sample_cubemap_lod(u->prefilterMap, R, roughness * 4.0f);
	vec2 brdf = vec2(toglm(texture2D(u->brdfLUT, max(dot(N, V), 0.0f), roughness)));
	vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
	vec3 color = (kD * diffuse + specular) * ao + Lo;
	color = color / (color + vec3(1.0f));
	color = pow(color, vec3(1.0f / 2.2f));
	*(vec4*)&builtins->gl_FragColor = vec4(color, 1.0f);
}

void equirect_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 v = normalize(*(vec3*)&fs_input[0]);
	const vec2 invAtan = vec2(0.1591f, 0.3183f);
	vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0f, 1.0f))) * invAtan + 0.5f;
	*(vec4*)&builtins->gl_FragColor = vec4(vec3(toglm(texture2D(u->hdrTexture, uv.x, uv.y))), 1.0f);
}

void irradiance_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 N = normalize(*(vec3*)&fs_input[0]);
	vec3 up(0.0f, 1.0f, 0.0f);
	vec3 right = normalize(cross(up, N));
	up = normalize(cross(N, right));
	vec3 irradiance = vec3(0.0f);
	float nrSamples = 0.0f;
	for (float phi = 0.0f; phi < 2.0f * PI; phi += u->sampleDelta)
	{
		for (float theta = 0.0f; theta < 0.5f * PI; theta += u->sampleDelta)
		{
			vec3 tangentSample(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
			irradiance += sample_cubemap(u->envCubemap, sampleVec) * cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	*(vec4*)&builtins->gl_FragColor = vec4(PI * irradiance / nrSamples, 1.0f);
}

void prefilter_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 N = normalize(*(vec3*)&fs_input[0]);
	vec3 V = N;
	vec3 prefilteredColor = vec3(0.0f);
	float totalWeight = 0.0f;
	unsigned int SAMPLE_COUNT = (unsigned int)u->sampleCount;
	for (unsigned int i = 0; i < SAMPLE_COUNT; ++i)
	{
		vec3 H = ImportanceSampleGGX(Hammersley(i, SAMPLE_COUNT), N, u->roughness);
		vec3 L = normalize(2.0f * dot(V, H) * H - V);
		float NdotL = max(dot(N, L), 0.0f);
		if (NdotL > 0.0f)
		{
			float D = DistributionGGX(N, H, u->roughness);
			float NdotH = max(dot(N, H), 0.0f);
			float HdotV = max(dot(H, V), 0.0f);
			float pdf = D * NdotH / (4.0f * HdotV) + 0.0001f;
			float saTexel = 4.0f * PI / (6.0f * (float)ENV_SIZE * (float)ENV_SIZE);
			float saSample = 1.0f / (float(SAMPLE_COUNT) * pdf + 0.0001f);
			float mipLevel = u->roughness == 0.0f ? 0.0f : 0.5f * log2(saSample / saTexel);
			prefilteredColor += sample_cubemap_lod(u->envCubemap, L, mipLevel) * NdotL;
			totalWeight += NdotL;
		}
	}
	*(vec4*)&builtins->gl_FragColor = vec4(prefilteredColor / totalWeight, 1.0f);
}

void brdf_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];
	float NdotV = TexCoords.x;
	float roughness = TexCoords.y;
	vec3 V(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
	vec3 N(0.0f, 0.0f, 1.0f);
	float A = 0.0f, B = 0.0f;
	unsigned int SAMPLE_COUNT = (unsigned int)u->sampleCount;
	for (unsigned int i = 0; i < SAMPLE_COUNT; ++i)
	{
		vec3 H = ImportanceSampleGGX(Hammersley(i, SAMPLE_COUNT), N, roughness);
		vec3 L = normalize(2.0f * dot(V, H) * H - V);
		float NdotL = max(L.z, 0.0f);
		float NdotH = max(H.z, 0.0f);
		float VdotH = max(dot(V, H), 0.0f);
		if (NdotL > 0.0f)
		{
			float G_Vis = (GeometrySmith_IBL(N, V, L, roughness) * VdotH) / (NdotH * NdotV);
			float Fc = pow(1.0f - VdotH, 5.0f);
			A += (1.0f - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}
	*(vec4*)&builtins->gl_FragColor = vec4(A / float(SAMPLE_COUNT), B / float(SAMPLE_COUNT), 0.0f, 1.0f);
}

void background_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 envColor = sample_cubemap(u->envCubemap, *(vec3*)&fs_input[0]);
	envColor = envColor / (envColor + vec3(1.0f));
	envColor = pow(envColor, vec3(1.0f / 2.2f));
	*(vec4*)&builtins->gl_FragColor = vec4(envColor, 1.0f);
}
