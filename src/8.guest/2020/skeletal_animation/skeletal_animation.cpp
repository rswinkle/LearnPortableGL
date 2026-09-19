#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>
#include <learnopengl/animator.h>
#include <learnopengl/model_animation.h>
#include <uniforms.h>

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

using namespace glm;

void setup_context();
void cleanup();
bool handle_events();
void anim_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void anim_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

vec4 toglm(pgl_vec4 v) { return vec4(v.x, v.y, v.z, v.w); }

const int MAX_BONES = 100;

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
	GLuint shader = pglCreateProgram(anim_vs, anim_fs, 2, smooth2, GL_FALSE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	Model ourModel(FileSystem::getPath("resources/objects/vampire/dancing_vampire.dae"));
	Animation danceAnimation(FileSystem::getPath("resources/objects/vampire/dancing_vampire.dae"), &ourModel);
	Animator animator(&danceAnimation);

	while (true)
	{
		fps_log_tick();
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (deltaTime > 0.05f) deltaTime = 0.05f;
		if (handle_events())
			break;
		animator.UpdateAnimation(deltaTime);

		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader);
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);
		uniforms.view = camera.GetViewMatrix();
		auto transforms = animator.GetFinalBoneMatrices();
		for (int i = 0; i < (int)transforms.size() && i < MAX_BONES; ++i)
			uniforms.finalBonesMatrices[i] = transforms[i];

		mat4 model = mat4(1.0f);
		model = translate(model, vec3(0.0f, -0.4f, 0.0f));
		model = scale(model, vec3(.5f, .5f, .5f));
		uniforms.model = model;
		ourModel.Draw(shader, &uniforms);

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

void anim_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	Model_Uniforms* u = (Model_Uniforms*)uniforms;
	vec4 pos = ((vec4*)vertex_attribs)[0];
	vec2 tex = vec2(((vec4*)vertex_attribs)[2]);
	vec4 boneIds = ((vec4*)vertex_attribs)[5];
	vec4 weights = ((vec4*)vertex_attribs)[6];

	vec4 totalPosition(0.0f);
	float ids[4] = { boneIds.x, boneIds.y, boneIds.z, boneIds.w };
	float ws[4] = { weights.x, weights.y, weights.z, weights.w };
	for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
	{
		int id = (int)ids[i];
		if (id < 0)
			continue;
		if (id >= MAX_BONES)
		{
			totalPosition = pos;
			break;
		}
		totalPosition += (u->finalBonesMatrices[id] * vec4(vec3(pos), 1.0f)) * ws[i];
	}
	*(vec2*)&vs_output[0] = tex;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * totalPosition;
}

void anim_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
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
