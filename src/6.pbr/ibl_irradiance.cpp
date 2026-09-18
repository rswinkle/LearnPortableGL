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
#include <fps_log.h>

using namespace glm;

#include "ibl_helpers.h"

struct My_Uniforms
{
	mat4 model;
	mat4 view;
	mat4 projection;
	mat3 normalMatrix;

	vec3 albedo;
	float metallic;
	float roughness;
	float ao;

	vec3 lightPositions[4];
	vec3 lightColors[4];
	vec3 camPos;

	GLuint hdrTexture;
	GLuint envCubemap;
	GLuint irradianceMap;
	float sampleDelta;
};

void setup_context();
void cleanup();
bool handle_events();

void pbr_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void pbr_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void cubemap_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void equirect_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void irradiance_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
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

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	GLenum smoothPbr[] = { PGL_SMOOTH2, PGL_SMOOTH3, PGL_SMOOTH3 };
	GLuint pbrShader = pglCreateProgram(pbr_vs, pbr_fs, 8, smoothPbr, GL_FALSE);
	glUseProgram(pbrShader);
	pglSetUniform(&uniforms);

	GLenum smooth3[] = { PGL_SMOOTH3 };
	GLuint equirectShader = pglCreateProgram(cubemap_vs, equirect_fs, 3, smooth3, GL_FALSE);
	glUseProgram(equirectShader);
	pglSetUniform(&uniforms);

	GLuint irradianceShader = pglCreateProgram(cubemap_vs, irradiance_fs, 3, smooth3, GL_FALSE);
	glUseProgram(irradianceShader);
	pglSetUniform(&uniforms);

	GLuint backgroundShader = pglCreateProgram(background_vs, background_fs, 3, smooth3, GL_FALSE);
	glUseProgram(backgroundShader);
	pglSetUniform(&uniforms);

	uniforms.albedo = vec3(0.5f, 0.0f, 0.0f);
	uniforms.ao = 1.0f;
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

	int nrRows = 7;
	int nrColumns = 7;
	float spacing = 2.5f;

	unsigned int captureFBO, captureRBO;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, ENV_SIZE, ENV_SIZE);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	stbi_set_flip_vertically_on_load(true);
	int width, height, nrComponents;
	float *data = stbi_loadf(FileSystem::getPath("resources/textures/hdr/newport_loft.hdr").c_str(), &width, &height, &nrComponents, 4);
	unsigned int hdrTexture = 0;
	if (data)
	{
		glGenTextures(1, &hdrTexture);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(data);
	}
	else
		std::cout << "Failed to load HDR image." << std::endl;
	uniforms.hdrTexture = hdrTexture;

	uniforms.envCubemap = make_float_cubemap(ENV_SIZE, GL_LINEAR);
	uniforms.irradianceMap = make_float_cubemap(IRR_SIZE, GL_LINEAR);

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

	std::cout << "convolving irradiance (" << IRR_SIZE << ", delta=" << SAMPLE_DELTA << ")..." << std::endl;
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, IRR_SIZE, IRR_SIZE);
	glUseProgram(irradianceShader);
	uniforms.projection = captureProjection;
	glViewport(0, 0, IRR_SIZE, IRR_SIZE);
	for (unsigned int i = 0; i < 6; ++i)
	{
		std::cout << "  face " << i << std::endl;
		uniforms.view = captureViews[i];
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, uniforms.irradianceMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		renderCube();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, scr_width, scr_height);
	std::cout << "done." << std::endl;

	while (true)
	{
		fps_log_tick();

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

		for (int row = 0; row < nrRows; ++row)
		{
			uniforms.metallic = (float)row / (float)nrRows;
			for (int col = 0; col < nrColumns; ++col)
			{
				uniforms.roughness = clamp((float)col / (float)nrColumns, 0.05f, 1.0f);
				mat4 model = mat4(1.0f);
				model = translate(model, vec3(
					(float)(col - (nrColumns / 2)) * spacing,
					(float)(row - (nrRows / 2)) * spacing,
					-2.0f
				));
				uniforms.model = model;
				uniforms.normalMatrix = transpose(inverse(mat3(model)));
				renderSphere();
			}
		}

		for (int i = 0; i < 4; ++i)
		{
			mat4 model = mat4(1.0f);
			model = translate(model, lightPositions[i]);
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
	vec3 aNormal = vec3(((vec4*)vertex_attribs)[1]);
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[2]);
	vec3 WorldPos = vec3(u->model * aPos);
	*(vec2*)&vs_output[0] = aTexCoords;
	*(vec3*)&vs_output[2] = WorldPos;
	*(vec3*)&vs_output[5] = u->normalMatrix * aNormal;
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

void pbr_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 WorldPos = *(vec3*)&fs_input[2];
	vec3 N = *(vec3*)&fs_input[5];
	vec3 V = normalize(u->camPos - WorldPos);
	vec3 F0 = mix(vec3(0.04f), u->albedo, u->metallic);

	vec3 Lo = vec3(0.0f);
	for (int i = 0; i < 4; ++i)
	{
		vec3 L = normalize(u->lightPositions[i] - WorldPos);
		vec3 H = normalize(V + L);
		float distance = length(u->lightPositions[i] - WorldPos);
		vec3 radiance = u->lightColors[i] * (1.0f / (distance * distance));
		vec3 F = fresnelSchlick(max(dot(H, V), 0.0f), F0);
		vec3 specular = (DistributionGGX(N, H, u->roughness) * GeometrySmith(N, V, L, u->roughness) * F) /
			(4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f);
		vec3 kD = (vec3(1.0f) - F) * (1.0f - u->metallic);
		Lo += (kD * u->albedo / PI + specular) * radiance * max(dot(N, L), 0.0f);
	}

	vec3 kS = fresnelSchlick(max(dot(N, V), 0.0f), F0);
	vec3 kD = (vec3(1.0f) - kS) * (1.0f - u->metallic);
	vec3 irradiance = sample_cubemap(u->irradianceMap, N);
	vec3 ambient = (kD * irradiance * u->albedo) * u->ao;
	vec3 color = ambient + Lo;
	color = color / (color + vec3(1.0f));
	color = pow(color, vec3(1.0f / 2.2f));
	*(vec4*)&builtins->gl_FragColor = vec4(color, 1.0f);
}

void equirect_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 v = normalize(*(vec3*)&fs_input[0]);
	const vec2 invAtan = vec2(0.1591f, 0.3183f);
	vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0f, 1.0f)));
	uv = uv * invAtan + 0.5f;
	*(vec4*)&builtins->gl_FragColor = vec4(vec3(toglm(texture2D(u->hdrTexture, uv.x, uv.y))), 1.0f);
}

void irradiance_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 N = normalize(*(vec3*)&fs_input[0]);
	vec3 up = vec3(0.0f, 1.0f, 0.0f);
	vec3 right = normalize(cross(up, N));
	up = normalize(cross(N, right));

	vec3 irradiance = vec3(0.0f);
	float nrSamples = 0.0f;
	float sampleDelta = u->sampleDelta;
	for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
	{
		for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
		{
			vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
			irradiance += sample_cubemap(u->envCubemap, sampleVec) * cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	irradiance = PI * irradiance * (1.0f / nrSamples);
	*(vec4*)&builtins->gl_FragColor = vec4(irradiance, 1.0f);
}

void background_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 envColor = sample_cubemap(u->envCubemap, *(vec3*)&fs_input[0]);
	envColor = envColor / (envColor + vec3(1.0f));
	envColor = pow(envColor, vec3(1.0f / 2.2f));
	*(vec4*)&builtins->gl_FragColor = vec4(envColor, 1.0f);
}
