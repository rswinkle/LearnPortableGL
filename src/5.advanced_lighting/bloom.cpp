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

struct My_Uniforms
{
	mat4 model;
	mat4 view;
	mat4 projection;

	GLuint tex;
	GLuint bloomBlur;
	vec3 lightPositions[4];
	vec3 lightColors[4];
	vec3 viewPos;
	vec3 lightColor;
	int bloom;
	int horizontal;
	float exposure;
};

void setup_context();
void cleanup();
bool handle_events();
unsigned int loadTexture(const char *path, bool gammaCorrection);
void renderQuad();
void renderCube();

void lighting_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void lighting_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void light_box_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void light_box_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void quad_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void blur_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void bloom_final_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

vec4 toglm(pgl_vec4 v);
void write_mrt(Shader_Builtins* builtins, vec4 color, vec4 bright);

const unsigned int fbo_width = 640;
const unsigned int fbo_height = 480;
unsigned int scr_width = fbo_width;
unsigned int scr_height = fbo_height;
bool bloom = true;
float exposure = 1.0f;

Camera camera(vec3(0.0f, 0.0f, 5.0f));

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

	GLenum smooth8[] = { PGL_SMOOTH3, PGL_SMOOTH3, PGL_SMOOTH2 };
	GLuint shader = pglCreateProgram(lighting_vs, lighting_fs, 8, smooth8, GL_FALSE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	GLuint shaderLight = pglCreateProgram(light_box_vs, light_box_fs, 0, NULL, GL_FALSE);
	glUseProgram(shaderLight);
	pglSetUniform(&uniforms);

	GLenum smooth2[] = { PGL_SMOOTH2 };
	GLuint shaderBlur = pglCreateProgram(quad_vs, blur_fs, 2, smooth2, GL_FALSE);
	glUseProgram(shaderBlur);
	pglSetUniform(&uniforms);

	GLuint shaderBloomFinal = pglCreateProgram(quad_vs, bloom_final_fs, 2, smooth2, GL_FALSE);
	glUseProgram(shaderBloomFinal);
	pglSetUniform(&uniforms);

	unsigned int woodTexture = loadTexture(FileSystem::getPath("resources/textures/wood.png").c_str(), true);
	unsigned int containerTexture = loadTexture(FileSystem::getPath("resources/textures/container2.png").c_str(), true);

	unsigned int hdrFBO;
	glGenFramebuffers(1, &hdrFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	unsigned int colorBuffers[2];
	glGenTextures(2, colorBuffers);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_width, fbo_height, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
	}
	unsigned int rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, fbo_width, fbo_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
	unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, attachments);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	unsigned int pingpongFBO[2];
	unsigned int pingpongColorbuffers[2];
	glGenFramebuffers(2, pingpongFBO);
	glGenTextures(2, pingpongColorbuffers);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
		glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_width, fbo_height, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "Framebuffer not complete!" << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	vec3 lightPositions[] = {
		vec3( 0.0f, 0.5f,  1.5f),
		vec3(-4.0f, 0.5f, -3.0f),
		vec3( 3.0f, 0.5f,  1.0f),
		vec3(-0.8f, 2.4f, -1.0f)
	};
	vec3 lightColors[] = {
		vec3(5.0f,  5.0f,  5.0f),
		vec3(10.0f, 0.0f,  0.0f),
		vec3(0.0f,  0.0f, 15.0f),
		vec3(0.0f,  5.0f,  0.0f)
	};
	for (int i = 0; i < 4; i++) {
		uniforms.lightPositions[i] = lightPositions[i];
		uniforms.lightColors[i] = lightColors[i];
	}
	uniforms.bloom = bloom;
	uniforms.exposure = exposure;

	while (true)
	{
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame)/1000.0f;
		lastFrame = currentFrame;

		if (handle_events())
			break;

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
		glViewport(0, 0, fbo_width, fbo_height);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader);
		uniforms.projection = perspective(radians(camera.Zoom), (float)fbo_width / (float)fbo_height, 0.1f, 100.0f);
		uniforms.view = camera.GetViewMatrix();
		uniforms.viewPos = camera.Position;

		mat4 model = mat4(1.0f);
		glBindTexture(GL_TEXTURE_2D, woodTexture);
		uniforms.tex = woodTexture;
		model = translate(model, vec3(0.0f, -1.0f, 0.0f));
		model = scale(model, vec3(12.5f, 0.5f, 12.5f));
		uniforms.model = model;
		renderCube();

		glBindTexture(GL_TEXTURE_2D, containerTexture);
		uniforms.tex = containerTexture;

		model = mat4(1.0f);
		model = translate(model, vec3(0.0f, 1.5f, 0.0f));
		model = scale(model, vec3(0.5f));
		uniforms.model = model;
		renderCube();

		model = mat4(1.0f);
		model = translate(model, vec3(2.0f, 0.0f, 1.0f));
		model = scale(model, vec3(0.5f));
		uniforms.model = model;
		renderCube();

		model = mat4(1.0f);
		model = translate(model, vec3(-1.0f, -1.0f, 2.0f));
		model = rotate(model, radians(60.0f), normalize(vec3(1.0f, 0.0f, 1.0f)));
		uniforms.model = model;
		renderCube();

		model = mat4(1.0f);
		model = translate(model, vec3(0.0f, 2.7f, 4.0f));
		model = rotate(model, radians(23.0f), normalize(vec3(1.0f, 0.0f, 1.0f)));
		model = scale(model, vec3(1.25f));
		uniforms.model = model;
		renderCube();

		model = mat4(1.0f);
		model = translate(model, vec3(-2.0f, 1.0f, -3.0f));
		model = rotate(model, radians(124.0f), normalize(vec3(1.0f, 0.0f, 1.0f)));
		uniforms.model = model;
		renderCube();

		model = mat4(1.0f);
		model = translate(model, vec3(-3.0f, 0.0f, 0.0f));
		model = scale(model, vec3(0.5f));
		uniforms.model = model;
		renderCube();

		glUseProgram(shaderLight);
		uniforms.projection = perspective(radians(camera.Zoom), (float)fbo_width / (float)fbo_height, 0.1f, 100.0f);
		uniforms.view = camera.GetViewMatrix();
		for (unsigned int i = 0; i < 4; i++)
		{
			model = mat4(1.0f);
			model = translate(model, lightPositions[i]);
			model = scale(model, vec3(0.25f));
			uniforms.model = model;
			uniforms.lightColor = lightColors[i];
			renderCube();
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// original: 10 ping-pong blur passes (5 horizontal + 5 vertical)
		int horizontal = 1, first_iteration = 1;
		const unsigned int amount = 10;
		glDisable(GL_DEPTH_TEST);
		glViewport(0, 0, fbo_width, fbo_height);
		glUseProgram(shaderBlur);
		for (unsigned int i = 0; i < amount; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
			uniforms.horizontal = horizontal;
			uniforms.tex = first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal];
			glBindTexture(GL_TEXTURE_2D, uniforms.tex);
			renderQuad();
			horizontal = !horizontal;
			if (first_iteration)
				first_iteration = 0;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, scr_width, scr_height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shaderBloomFinal);
		uniforms.tex = colorBuffers[0];
		uniforms.bloomBlur = pingpongColorbuffers[!horizontal];
		uniforms.bloom = bloom;
		uniforms.exposure = exposure;
		renderQuad();

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	glDeleteRenderbuffers(1, &rboDepth);
	glDeleteFramebuffers(1, &hdrFBO);
	glDeleteFramebuffers(2, pingpongFBO);
	glDeleteTextures(2, colorBuffers);
	glDeleteTextures(2, pingpongColorbuffers);

	cleanup();
	return 0;
}

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
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

unsigned int quadVAO = 0;
unsigned int quadVBO = 0;
void renderQuad()
{
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
			switch (sc) {
			case SDL_SCANCODE_ESCAPE:
				return true;
			case SDL_SCANCODE_SPACE:
				if (!event.key.repeat) {
					bloom = !bloom;
					uniforms.bloom = bloom;
					std::cout << "bloom: " << (bloom ? "on" : "off") << "| exposure: " << exposure << std::endl;
				}
				break;
			default:
				;
			}
			break;

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
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
		{
			float dx = event.motion.xrel;
			float dy = -event.motion.yrel;
			camera.ProcessMouseMovement(dx, dy);
		} break;

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

	if (state[SDL_SCANCODE_Q]) {
		if (exposure > 0.0f)
			exposure -= 0.001f;
		else
			exposure = 0.0f;
	} else if (state[SDL_SCANCODE_E]) {
		exposure += 0.001f;
	}

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

void lighting_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec3 aNormal = vec3(((vec4*)vertex_attribs)[1]);
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[2]);

	vec3 FragPos = vec3(u->model * aPos);
	mat3 normalMatrix = transpose(inverse(mat3(u->model)));

	*(vec3*)&vs_output[0] = FragPos;
	*(vec3*)&vs_output[3] = normalize(normalMatrix * aNormal);
	*(vec2*)&vs_output[6] = aTexCoords;

	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * aPos;
}

vec4 toglm(pgl_vec4 v)
{
	return vec4(v.x, v.y, v.z, v.w);
}

void write_mrt(Shader_Builtins* builtins, vec4 color, vec4 bright)
{
	*(vec4*)&builtins->gl_FragColor = color;
	*(vec4*)&builtins->gl_FragData[0] = color;
	*(vec4*)&builtins->gl_FragData[1] = bright;
}

void lighting_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	vec3 FragPos = *(vec3*)&fs_input[0];
	vec3 Normal = *(vec3*)&fs_input[3];
	vec2 TexCoords = *(vec2*)&fs_input[6];

	vec3 color = vec3(toglm(texture2D(u->tex, TexCoords.x, TexCoords.y)));
	vec3 normal = normalize(Normal);
	vec3 ambient = 0.0f * color;
	vec3 lighting = vec3(0.0f);
	for (int i = 0; i < 4; i++)
	{
		vec3 lightDir = normalize(u->lightPositions[i] - FragPos);
		float diff = max(dot(lightDir, normal), 0.0f);
		vec3 result = u->lightColors[i] * diff * color;
		float distance = length(FragPos - u->lightPositions[i]);
		result *= 1.0f / (distance * distance);
		lighting += result;
	}
	vec3 result = ambient + lighting;
	float brightness = dot(result, vec3(0.2126f, 0.7152f, 0.0722f));
	vec4 brightColor = brightness > 1.0f ? vec4(result, 1.0f) : vec4(0.0f, 0.0f, 0.0f, 1.0f);
	write_mrt(builtins, vec4(result, 1.0f), brightColor);
}

void light_box_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)vs_output;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * aPos;
}

void light_box_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)fs_input;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec4 frag = vec4(u->lightColor, 1.0f);
	float brightness = dot(u->lightColor, vec3(0.2126f, 0.7152f, 0.0722f));
	vec4 bright = brightness > 1.0f ? frag : vec4(0.0f, 0.0f, 0.0f, 1.0f);
	write_mrt(builtins, frag, bright);
}

void quad_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[1]);
	*(vec2*)&vs_output[0] = aTexCoords;
	*(vec4*)&builtins->gl_Position = aPos;
}

void blur_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];

	static const float weight[5] = { 0.2270270270f, 0.1945945946f, 0.1216216216f, 0.0540540541f, 0.0162162162f };
	pgl_ivec3 ts = textureSize(u->tex, 0);
	vec2 tex_offset = vec2(1.0f / (float)ts.x, 1.0f / (float)ts.y);
	vec3 result = vec3(toglm(texture2D(u->tex, TexCoords.x, TexCoords.y))) * weight[0];
	if (u->horizontal)
	{
		for (int i = 1; i < 5; ++i)
		{
			result += vec3(toglm(texture2D(u->tex, TexCoords.x + tex_offset.x * i, TexCoords.y))) * weight[i];
			result += vec3(toglm(texture2D(u->tex, TexCoords.x - tex_offset.x * i, TexCoords.y))) * weight[i];
		}
	}
	else
	{
		for (int i = 1; i < 5; ++i)
		{
			result += vec3(toglm(texture2D(u->tex, TexCoords.x, TexCoords.y + tex_offset.y * i))) * weight[i];
			result += vec3(toglm(texture2D(u->tex, TexCoords.x, TexCoords.y - tex_offset.y * i))) * weight[i];
		}
	}
	*(vec4*)&builtins->gl_FragColor = vec4(result, 1.0f);
}

void bloom_final_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];

	const float gamma = 2.2f;
	vec3 hdrColor = vec3(toglm(texture2D(u->tex, TexCoords.x, TexCoords.y)));
	vec3 bloomColor = vec3(toglm(texture2D(u->bloomBlur, TexCoords.x, TexCoords.y)));
	if (u->bloom)
		hdrColor += bloomColor;
	vec3 result = vec3(1.0f) - exp(-hdrColor * u->exposure);
	result = pow(result, vec3(1.0f / gamma));
	*(vec4*)&builtins->gl_FragColor = vec4(result, 1.0f);
}

void cleanup()
{
	free_glContext(&the_Context);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

unsigned int loadTexture(char const * path, bool gammaCorrection)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char *data = stbi_load(path, &width, &height, &nrComponents, STBI_rgb_alpha);
	if (data)
	{
		// PGL has no GL_SRGB textures. Decode sRGB into 8-bit linear when requested.
		if (gammaCorrection) {
			int n = width * height * 4;
			for (int i = 0; i < n; i += 4) {
				for (int c = 0; c < 3; ++c) {
					float s = data[i + c] / 255.0f;
					float lin = (s <= 0.04045f) ? (s / 12.92f) : powf((s + 0.055f) / 1.055f, 2.4f);
					data[i + c] = (unsigned char)(lin * 255.0f + 0.5f);
				}
			}
		}

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
