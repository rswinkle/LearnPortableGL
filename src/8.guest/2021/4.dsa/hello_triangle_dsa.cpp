#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

bool handle_events();
void setup_context();
void cleanup();

void basic_vs(float* vs_output, vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void basic_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

unsigned int scr_width = 640;
unsigned int scr_height = 480;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;

int main()
{
	setup_context();

	unsigned int shaderProgram = pglCreateProgram(basic_vs, basic_fs, 0, NULL, GL_FALSE);

	float vertices[] = {
		 0.5f,  0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		-0.5f, -0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f
	};
	unsigned int indices[] = {
		0, 1, 3,
		1, 2, 3
	};

	// PGL: glCreateBuffers is glGenBuffers. No glNamedBufferStorage /
	// glCreateVertexArrays / glVertexArrayAttrib* / glVertexArrayVertexBuffer.
	GLuint vbo = 0;
	glCreateBuffers(1, &vbo);
	glNamedBufferData(vbo, sizeof(vertices), vertices, GL_STATIC_DRAW);

	GLuint ebo = 0;
	glCreateBuffers(1, &ebo);
	glNamedBufferData(ebo, sizeof(indices), indices, GL_STATIC_DRAW);

	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBindVertexArray(0);

	while (true)
	{
		fps_log_tick();
		if (handle_events())
			break;

		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(shaderProgram);
		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	glDeleteBuffers(1, &ebo);
	glDeleteProgram(shaderProgram);
	cleanup();
	return 0;
}

void basic_vs(float* vs_output, vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)vs_output; (void)uniforms;
	builtins->gl_Position = vertex_attribs[0];
}

void basic_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)fs_input; (void)uniforms;
	builtins->gl_FragColor = make_v4(1.0f, 0.5f, 0.2f, 1.0f);
}

bool handle_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT)
			return true;
		if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
			return true;
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
