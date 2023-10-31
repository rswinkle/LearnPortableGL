#define PGL_MANGLE_TYPES
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

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

using namespace glm;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
uint32_t rmask = 0xff000000;
uint32_t gmask = 0x00ff0000;
uint32_t bmask = 0x0000ff00;
uint32_t amask = 0x000000ff;
#define PIX_FORMAT SDL_PIXELFORMAT_RGBA8888
#else
uint32_t rmask = 0x000000ff;
uint32_t gmask = 0x0000ff00;
uint32_t bmask = 0x00ff0000;
uint32_t amask = 0xff000000;
#define PIX_FORMAT SDL_PIXELFORMAT_ABGR8888
#endif


struct My_Uniforms
{
	mat4 model;
	mat4 view;
	mat4 projection;

	GLuint tex;
};

void setup_context();
void cleanup();
bool handle_events();
unsigned int loadTexture(const char *path);

void texture_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void texture_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void screen_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void screen_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);


// settings
unsigned int scr_width = 640;
unsigned int scr_height = 480;

// camera
Camera camera(vec3(0.0f, 0.0f, 3.0f));

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;

u32* bbufpix;

glContext the_Context;

My_Uniforms uniforms;

unsigned int textureColorbuffer;

int main()
{
	// SDL2 and PortableGL: initialize and configure
	// ------------------------------
	setup_context();

	// tell SDL to capture our mouse
	SDL_SetRelativeMouseMode(SDL_TRUE);

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);

	// Create our shader programs and set uniform pointer for each
	// -----------------------------------------------------------
	GLenum smooth[] = { PGL_SMOOTH2 };
	GLuint shader = pglCreateProgram(texture_vs, texture_fs, 2, smooth, GL_FALSE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	GLuint screenShader = pglCreateProgram(screen_vs, screen_fs, 2, smooth, GL_FALSE);
	glUseProgram(screenShader);
	pglSetUniform(&uniforms);

	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------
	float cubeVertices[] = {
		// positions          // texture Coords
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
	};
	float planeVertices[] = {
		// positions          // texture Coords (note we set these higher than 1 (together with GL_REPEAT as texture wrapping mode). this will cause the floor texture to repeat)
		 5.0f, -0.5f,  5.0f,  2.0f, 0.0f,
		-5.0f, -0.5f,  5.0f,  0.0f, 0.0f,
		-5.0f, -0.5f, -5.0f,  0.0f, 2.0f,

		 5.0f, -0.5f,  5.0f,  2.0f, 0.0f,
		-5.0f, -0.5f, -5.0f,  0.0f, 2.0f,
		 5.0f, -0.5f, -5.0f,  2.0f, 2.0f
	};

	// I fixed the texture coordinates because the LearnOpenGL code/tutorial
	// is wrong. OpenGL does *not* "expect the y texture coordinate 0 to be
	// the bottom of the image", it just treats the first row of texture data
	// as y = 0 so if the image is rightside up in memory y = 0 will be the
	// top, if it's upside down y = 0 will be the bottom (but it could also
	// be sideways in which case y = 0 is really the left or right side.
	//
	// People are so tied to the idea that y increases up that everyone seems
	// to make this mistake even though it has nothing to do with up/down or top/bottom.
	float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
		// positions   // texCoords
		-1.0f,  1.0f,  0.0f, 0.0f,
		-1.0f, -1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 1.0f,

		-1.0f,  1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 1.0f,
		 1.0f,  1.0f,  1.0f, 0.0f
	};
	// cube VAO
	unsigned int cubeVAO, cubeVBO;
	glGenVertexArrays(1, &cubeVAO);
	glGenBuffers(1, &cubeVBO);
	glBindVertexArray(cubeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
	glEnableVertexAttribArray(1);
	pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 3 * sizeof(float));
	glBindVertexArray(0);
	// plane VAO
	unsigned int planeVAO, planeVBO;
	glGenVertexArrays(1, &planeVAO);
	glGenBuffers(1, &planeVBO);
	glBindVertexArray(planeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), &planeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
	glEnableVertexAttribArray(1);
	pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 3 * sizeof(float));

	// screen quad VAO
	unsigned int quadVAO, quadVBO;
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	glEnableVertexAttribArray(1);
	pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 2 * sizeof(float));


	// load textures
	// -------------
	unsigned int cubeTexture  = loadTexture(FileSystem::getPath("resources/textures/container.jpg").c_str());
	unsigned int floorTexture = loadTexture(FileSystem::getPath("resources/textures/metal.png").c_str());

	// framebuffer configuration
	// -------------------------
	// create a color attachment texture
	glGenTextures(1, &textureColorbuffer);
	glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scr_width, scr_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// draw as wireframe
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);


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
		glEnable(GL_DEPTH_TEST);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shader);
		mat4 model = mat4(1.0f);
		uniforms.view = camera.GetViewMatrix();
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);

		// cubes
		glBindVertexArray(cubeVAO);
		uniforms.tex = cubeTexture;
		uniforms.model = translate(model, vec3(-1.0f, 0.0f, -1.0f));
		glDrawArrays(GL_TRIANGLES, 0, 36);
		model = mat4(1.0f);
		uniforms.model = translate(model, vec3(2.0f, 0.0f, 0.0f));
		glDrawArrays(GL_TRIANGLES, 0, 36);
		// floor
		glBindVertexArray(planeVAO);
		uniforms.tex = floorTexture;
		uniforms.model = mat4(1.0f);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		// Copy framebuffer to textureColorbuffer (because PGL doesn't support
		// framebuffer/renderbuffer objects or render to texture in general yet)
		glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, scr_width, scr_height, GL_RGBA, GL_UNSIGNED_BYTE, bbufpix);
		uniforms.tex = textureColorbuffer;

		// now draw a quad plane with the attached framebuffer color texture
		glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
		// clear all relevant buffers
		//glClearColor(1.0f, 1.0f, 1.0f, 1.0f); // set clear color to white (not really necessary actually, since we won't be able to see behind the quad anyways)
		//glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(screenShader);
		glBindVertexArray(quadVAO);
		glBindTexture(GL_TEXTURE_2D, textureColorbuffer); // use the color attachment texture as the texture of the quad plane
		glDrawArrays(GL_TRIANGLES, 0, 6);


		// SDL2: blit texture (ie latest rendered frame) to screen
		// -------------------------------------------------------------------------------
		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(u32));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------
	glDeleteVertexArrays(1, &cubeVAO);
	glDeleteVertexArrays(1, &planeVAO);
	glDeleteBuffers(1, &cubeVBO);
	glDeleteBuffers(1, &planeVBO);

	// SDL and PortableGL cleanup
	// ------------------------------------------------------------------
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
				tex = SDL_CreateTexture(ren, PIX_FORMAT, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);

				glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scr_width, scr_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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

	// Create Software Renderer and texture
	ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);

	// Initialize and set PGL context
	if (!init_glContext(&the_Context, &bbufpix, scr_width, scr_height, 32, rmask, gmask, bmask, amask)) {
		puts("Failed to initialize glContext");
		exit(0);
	}
	set_glContext(&the_Context);
}

void texture_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	mat4 model = u->model;
	mat4 view = u->view;
	mat4 projection = u->projection;

	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[1]);

	// Texcoords = aTexCoord
	*(vec2*)&vs_output[0] = aTexCoords;

	*(vec4*)&builtins->gl_Position = projection * view * model * aPos;
}

void texture_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];

	builtins->gl_FragColor = texture2D(u->tex, TexCoords.x, TexCoords.y);
}

void screen_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	// z and w will be 0, 1 according to spec so might as well use them
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[1]);

	// Texcoords = aTexCoord
	*(vec2*)&vs_output[0] = aTexCoords;

	//print_vec4(((pgl_vec4*)vertex_attribs)[0], "\n");

	*(vec4*)&builtins->gl_Position = aPos;
}

// I guess I didn't need this since it's identical to texture_fs
void screen_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];

	builtins->gl_FragColor = texture2D(u->tex, TexCoords.x, TexCoords.y);
	//print_vec4(builtins->gl_FragColor
}

void cleanup()
{
	free_glContext(&the_Context);

	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(window);

	SDL_Quit();
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char *data = stbi_load(path, &width, &height, &nrComponents, STBI_rgb_alpha);
	if (data)
	{
		GLenum format;
		/*
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;
		*/
		format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
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





