#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <learnopengl/entity.h>
#include <uniforms.h>

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

using namespace glm;

void setup_context();
void cleanup();
bool handle_events();
void model_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void model_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

vec4 toglm(pgl_vec4 v) { return vec4(v.x, v.y, v.z, v.w); }

unsigned int scr_width = 640, scr_height = 480;
Camera camera(vec3(0.0f, 0.0f, 3.0f));
float deltaTime = 0.0f, lastFrame = 0.0f;
SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;
Model_Uniforms uniforms;

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);
	stbi_set_flip_vertically_on_load(true);
	glEnable(GL_DEPTH_TEST);

	GLenum smooth2[] = { PGL_SMOOTH2 };
	GLuint shader = pglCreateProgram(model_vs, model_fs, 2, smooth2, GL_FALSE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	Model model = Model(FileSystem::getPath("resources/objects/planet/planet.obj"));
	Entity ourEntity(model);
	ourEntity.transform.setLocalPosition({ 10, 0, 0 });
	const float scale = 0.75f;
	ourEntity.transform.setLocalScale({ scale, scale, scale });
	{
		Entity* lastEntity = &ourEntity;
		for (unsigned int i = 0; i < 10; ++i)
		{
			lastEntity->addChild(model);
			lastEntity = lastEntity->children.back().get();
			lastEntity->transform.setLocalPosition({ 10, 0, 0 });
			lastEntity->transform.setLocalScale({ scale, scale, scale });
		}
	}
	ourEntity.updateSelfAndChild();

	while (true)
	{
		fps_log_tick();
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (handle_events())
			break;

		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader);
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);
		uniforms.view = camera.GetViewMatrix();

		Entity* lastEntity = &ourEntity;
		while (lastEntity->children.size())
		{
			uniforms.model = lastEntity->transform.getModelMatrix();
			lastEntity->pModel->Draw(shader, &uniforms);
			lastEntity = lastEntity->children.back().get();
		}

		ourEntity.transform.setLocalRotation({ 0.f, ourEntity.transform.getLocalRotation().y + 20 * deltaTime, 0.f });
		ourEntity.updateSelfAndChild();

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

void model_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	Model_Uniforms* u = (Model_Uniforms*)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec2 aTex = vec2(((vec4*)vertex_attribs)[2]);
	*(vec2*)&vs_output[0] = aTex;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * aPos;
}

void model_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	Model_Uniforms* u = (Model_Uniforms*)uniforms;
	vec2 uv = *(vec2*)&fs_input[0];
	*(vec4*)&builtins->gl_FragColor = toglm(texture2D(u->texture_diffuse[0], uv.x, uv.y));
}

bool handle_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT: return true;
		case SDL_KEYDOWN:
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) return true;
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				scr_width = event.window.data1; scr_height = event.window.data2;
				pglResizeFramebuffer(scr_width, scr_height);
				bbufpix = (pix_t*)pglGetBackBuffer();
				glViewport(0, 0, scr_width, scr_height);
				SDL_DestroyTexture(tex);
				tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
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
