#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <cmath>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

using namespace glm;

struct My_Uniforms { GLuint tex; };

void setup_context();
void cleanup();
bool handle_events();
void renderQuad();
void compute_fill(std::vector<float>& pixels, int w, int h, float t);

void screen_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void screen_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

vec4 toglm(pgl_vec4 v) { return vec4(v.x, v.y, v.z, v.w); }

#ifndef NDEBUG
const unsigned int TEX_W = 128, TEX_H = 128;
#else
const unsigned int TEX_W = 256, TEX_H = 256;
#endif

unsigned int scr_width = 640;
unsigned int scr_height = 480;
SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;
My_Uniforms uniforms;

int main()
{
	setup_context();

	std::cout << "PGL has no compute shaders / glDispatchCompute / imageStore.\n";
	std::cout << "Filling a " << TEX_W << "x" << TEX_H << " RGBA32F texture on the CPU with the original CS formula.\n";

	GLenum smooth2[] = { PGL_SMOOTH2 };
	GLuint screenQuad = pglCreateProgram(screen_vs, screen_fs, 2, smooth2, GL_FALSE);
	glUseProgram(screenQuad);
	pglSetUniform(&uniforms);

	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	std::vector<float> pixels(TEX_W * TEX_H * 4, 0.0f);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEX_W, TEX_H, 0, GL_RGBA, GL_FLOAT, pixels.data());
	uniforms.tex = texture;

	while (true)
	{
		fps_log_tick();
		if (handle_events())
			break;

		float t = SDL_GetTicks() / 1000.0f;
		compute_fill(pixels, TEX_W, TEX_H, t);
		glBindTexture(GL_TEXTURE_2D, texture);
		// glTexSubImage2D is U8-only; re-upload the float image each frame.
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TEX_W, TEX_H, 0, GL_RGBA, GL_FLOAT, pixels.data());

		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(screenQuad);
		renderQuad();

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

void compute_fill(std::vector<float>& pixels, int w, int h, float t)
{
	float speed = 100.0f;
	float width = (float)w;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			float xv = fmod((float)x + t * speed, width) / width;
			if (xv < 0.0f) xv += 1.0f;
			float yv = (float)y / (float)h;
			int i = (y * w + x) * 4;
			pixels[i+0] = xv;
			pixels[i+1] = yv;
			pixels[i+2] = 0.0f;
			pixels[i+3] = 1.0f;
		}
	}
}

unsigned int quadVAO = 0, quadVBO = 0;
void renderQuad()
{
	if (quadVAO == 0) {
		float quadVertices[] = {
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f
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
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void screen_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	vec4 pos = ((vec4*)vertex_attribs)[0];
	vec2 uv = vec2(((vec4*)vertex_attribs)[1]);
	*(vec2*)&vs_output[0] = uv;
	*(vec4*)&builtins->gl_Position = vec4(vec3(pos), 1.0f);
}

void screen_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec2 uv = *(vec2*)&fs_input[0];
	*(vec4*)&builtins->gl_FragColor = vec4(vec3(toglm(texture2D(u->tex, uv.x, uv.y))), 1.0f);
}

bool handle_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) return true;
		if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) return true;
		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
			scr_width = event.window.data1;
			scr_height = event.window.data2;
			pglResizeFramebuffer(scr_width, scr_height);
			bbufpix = (pix_t*)pglGetBackBuffer();
			glViewport(0, 0, scr_width, scr_height);
			SDL_DestroyTexture(tex);
			tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
		}
	}
	return false;
}

void setup_context()
{
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO)) { std::cout << "SDL_Init error: " << SDL_GetError() << "\n"; exit(0); }
	window = SDL_CreateWindow("LearnPortablGL", 100, 100, scr_width, scr_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (!window) { std::cerr << "Failed to create window\n"; SDL_Quit(); exit(0); }
	ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
	if (!init_glContext(&the_Context, &bbufpix, scr_width, scr_height)) { puts("Failed to initialize glContext"); exit(0); }
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
