#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>

#include <iostream>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

using namespace glm;

struct My_Uniforms { mat4 model, view, projection; };

void setup_context();
void cleanup();
bool handle_events();
void height_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void height_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

unsigned int scr_width = 640, scr_height = 480;
Camera camera(vec3(67.0f, 627.5f, 169.9f), vec3(0.0f, 1.0f, 0.0f), -128.1f, -42.4f);
float deltaTime = 0.0f, lastFrame = 0.0f;
int useWireframe = 0;
int displayGrayscale = 1;

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
	camera.MovementSpeed = 50.0f;

	GLenum smooth1[] = { PGL_SMOOTH };
	GLuint shader = pglCreateProgram(height_vs, height_fs, 1, smooth1, GL_FALSE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	stbi_set_flip_vertically_on_load(true);
	int width, height, nrChannels;
	unsigned char *data = stbi_load(FileSystem::getPath("resources/textures/heightmaps/iceland_heightmap.png").c_str(), &width, &height, &nrChannels, 0);
	if (data)
		std::cout << "Loaded heightmap of size " << height << " x " << width << std::endl;
	else
		std::cout << "Failed to load texture" << std::endl;

#ifndef NDEBUG
	int rez = 12;
#else
	int rez = 6;
#endif
	std::vector<float> vertices;
	float yScale = 64.0f / 256.0f, yShift = 16.0f;
	int bytePerPixel = nrChannels;
	int gw = 0, gh = 0;
	for (int i = 0; i < height; i += rez) {
		gw = 0;
		for (int j = 0; j < width; j += rez) {
			unsigned char* pixelOffset = data + (j + width * i) * bytePerPixel;
			unsigned char y = pixelOffset[0];
			vertices.push_back(-height / 2.0f + height * i / (float)height);
			vertices.push_back((int)y * yScale - yShift);
			vertices.push_back(-width / 2.0f + width * j / (float)width);
			gw++;
		}
		gh++;
	}
	stbi_image_free(data);
	std::cout << "No tessellation shaders in PGL; CPU mesh. Subsampled grid " << gw << " x " << gh << " (rez=" << rez << ")" << std::endl;

	std::vector<unsigned int> indices;
	for (int y = 0; y < gh - 1; y++) {
		for (int x = 0; x < gw - 1; x++) {
			unsigned int i0 = y * gw + x;
			unsigned int i1 = y * gw + x + 1;
			unsigned int i2 = (y + 1) * gw + x;
			unsigned int i3 = (y + 1) * gw + x + 1;
			indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
			indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
		}
	}
	std::cout << "Created " << indices.size() / 3 << " triangles" << std::endl;

	unsigned int terrainVAO, terrainVBO, terrainIBO;
	glGenVertexArrays(1, &terrainVAO);
	glBindVertexArray(terrainVAO);
	glGenBuffers(1, &terrainVBO);
	glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
	glGenBuffers(1, &terrainIBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

	while (true)
	{
		fps_log_tick();
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (handle_events())
			break;

		glPolygonMode(GL_FRONT_AND_BACK, useWireframe ? GL_LINE : GL_FILL);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader);
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100000.0f);
		uniforms.view = camera.GetViewMatrix();
		uniforms.model = mat4(1.0f);
		glBindVertexArray(terrainVAO);
		glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

void height_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 aPos = vec3(((vec4*)vertex_attribs)[0]);
	vs_output[0] = aPos.y;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * vec4(aPos, 1.0f);
}

void height_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	float h = (fs_input[0] + 16.0f) / 32.0f;
	*(vec4*)&builtins->gl_FragColor = vec4(h, h, h, 1.0f);
}

bool handle_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT: return true;
		case SDL_KEYDOWN:
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) return true;
			if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) useWireframe = !useWireframe;
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				scr_width = event.window.data1;
				scr_height = event.window.data2;
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
