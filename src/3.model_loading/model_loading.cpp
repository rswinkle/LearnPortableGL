#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <uniforms.h>

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

using namespace glm;

void setup_context();
void cleanup();
bool handle_events();

// settings
unsigned int scr_width = 640;
unsigned int scr_height = 480;

// camera
Camera camera(vec3(0.0f, 0.0f, 3.0f));
float lastX = scr_width / 2.0f;
float lastY = scr_height / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

#define PIX_FORMAT SDL_PIXELFORMAT_ARGB8888

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
u32* bbufpix;

glContext the_Context;


void texture_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	Model_Uniforms* u = (Model_Uniforms*)uniforms;

	mat4 model = u->model;
	mat4 view = u->view;
	mat4 projection = u->projection;

	// TODO just use mvp uniform?
	*(vec4*)&builtins->gl_Position = projection * view * model * ((vec4*)vertex_attribs)[0];

	// TexCoord = aTexCoord;
	vs_output[0] = vertex_attribs[2].x;
	vs_output[1] = vertex_attribs[2].y;

}
void texture_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	Model_Uniforms* u = (Model_Uniforms*)uniforms;

	builtins->gl_FragColor = texture2D(u->texture_diffuse[0], fs_input[0], fs_input[1]);
}

int main()
{
	setup_context();

	// tell SDL to capture our mouse
	SDL_SetRelativeMouseMode(SDL_TRUE);

	// tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
	stbi_set_flip_vertically_on_load(true);

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);


	// build our shader program
	// ------------------------
	GLenum smooth[2] = { PGL_SMOOTH2 };
	unsigned int ourShader = pglCreateProgram(texture_vs, texture_fs, 2, smooth, GL_FALSE);
	glUseProgram(ourShader);
	Model_Uniforms uniforms;
	pglSetUniform(&uniforms);

	// load models
	// -----------
	Model ourModel(FileSystem::getPath("resources/objects/backpack/backpack.obj"));



	// render loop
	// -----------
	while (true)
	{
		// per-frame time logic
		// --------------------
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame)/1000.0f;
		lastFrame = currentFrame;

		// input
		// -----
		if (handle_events())
			break;

		// render
		// ------
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// view/projection transformations
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);
		uniforms.view = camera.GetViewMatrix();

		// render the loaded model
		mat4 model = mat4(1.0f);
		model = translate(model, vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
		uniforms.model = scale(model, vec3(1.0f, 1.0f, 1.0f)); // it's a bit too big for our scene, so scale it down
		ourModel.Draw(ourShader, &uniforms);

		// SDL2: Update SDL_Texture to latest rendered frame, then blit to screen
		// ----------------------------------------------------------------------
		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(u32));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------
	glDeleteProgram(ourShader);

	cleanup();

	return 0;
}

// process all input: Poll for events reacting to ones we care about
// ---------------------------------------------------------------------------------------------------------
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

			/*
			case SDL_SCANCODE_W:
				camera.ProcessKeyboard(FORWARD, deltaTime);
				break;
			case SDL_SCANCODE_S:
				camera.ProcessKeyboard(BACKWARD, deltaTime);
				break;
			case SDL_SCANCODE_A:
				camera.ProcessKeyboard(LEFT, deltaTime);
				break;
			case SDL_SCANCODE_D:
				camera.ProcessKeyboard(RIGHT, deltaTime);
				break;
				*/
			default:
				;
			}

			break; //sdl_keydown

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
				scr_width = event.window.data1;
				scr_height = event.window.data2;

				bbufpix = (u32*)pglResizeFramebuffer(scr_width, scr_height);
				glViewport(0, 0, scr_width, scr_height);
				SDL_DestroyTexture(tex);
				tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
				break;
			}
			break;

		case SDL_MOUSEMOTION:
		{
			float dx = event.motion.xrel;
			float dy = -event.motion.yrel; // reversed since y coordinates go from bottom to top
			camera.ProcessMouseMovement(dx, dy);
		} break;

		case SDL_MOUSEWHEEL:
			camera.ProcessMouseScroll(event.wheel.y);
			break;
		}
	}

	const Uint8 *state = SDL_GetKeyboardState(NULL);
	
	if (state[SDL_SCANCODE_W]) {
		camera.ProcessKeyboard(FORWARD, deltaTime);
	}
	if (state[SDL_SCANCODE_S]) {
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	}
	if (state[SDL_SCANCODE_A]) {
		camera.ProcessKeyboard(LEFT, deltaTime);
	}
	if (state[SDL_SCANCODE_D]) {
		camera.ProcessKeyboard(RIGHT, deltaTime);
	}

	return false;
}


void setup_context()
{
	// Initialize SDL2 and create window
	// ------------------------------
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO)) {
		std::cout << "SDL_Init error: " << SDL_GetError() << "\n";
		exit(0);
	}

	window = SDL_CreateWindow("LearnPortableGL", 100, 100, scr_width, scr_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (!window) {
		std::cerr << "Failed to create window\n";
		SDL_Quit();
		exit(0);
	}

	// Create software renderer and texture
	ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	tex = SDL_CreateTexture(ren, PIX_FORMAT, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);

	// Initialize and set PGL context
	if (!init_glContext(&the_Context, &bbufpix, scr_width, scr_height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)) {
		puts("Failed to initialize glContext");
		exit(0);
	}
}

void cleanup()
{
	free_glContext(&the_Context);

	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(window);

	SDL_Quit();
}


