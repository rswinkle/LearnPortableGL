#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

void setup_context();
void cleanup();
bool handle_events();

// settings
unsigned int scr_width = 640;
unsigned int scr_height = 480;

#define PIX_FORMAT SDL_PIXELFORMAT_ARGB8888

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
u32* bbufpix;

glContext the_Context;

struct My_Uniforms
{
	vec4 ourColor;
};

// using PGL's internal vector types and functions in these shaders
void basic_vs(float* vs_output, void* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	builtins->gl_Position = ((vec4*)vertex_attribs)[0];
}
void uniform_color_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	builtins->gl_FragColor = u->ourColor;
}

int main()
{
	// Might as well make main a little cleaner
	setup_context();

	// build our shader program (and set uniform pointer)
	// ------------------------
	unsigned int shaderProgram = pglCreateProgram(basic_vs, uniform_color_fs, 0, NULL, GL_FALSE);

	// It's a good idea with PGL to set the uniform immediately if you use them
	// which requires setting the shader to active (like binding a buffer)
	//
	// Once set, it never needs to be modified again, you can change the contents
	// of the structure, ie update the uniforms as much as you like
	glUseProgram(shaderProgram);
	My_Uniforms uniforms;
	pglSetUniform(&uniforms);

	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------
	float vertices[] = {
		-0.5f, -0.5f, 0.0f, // left
		 0.5f, -0.5f, 0.0f, // right
		 0.0f,  0.5f, 0.0f  // top
	};

	unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
	glEnableVertexAttribArray(0);

	// You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
	// VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
	// glBindVertexArray(0);

	// bind the VAO (it was already bound, but just to demonstrate): seeing as we only have a single VAO we can 
	// just bind it beforehand before rendering the respective triangle; this is another approach.
	glBindVertexArray(VAO);


	// render loop
	// -----------
	while (true)
	{
		// input
		// -----
		if (handle_events())
			break;

		// render
		// ------
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// be sure to activate the shader before any calls to glUniform
		glUseProgram(shaderProgram);

		float timeValue = SDL_GetTicks()/1000.0f;
		float greenValue = sin(timeValue) / 2.0f + 0.5f;
		uniforms.ourColor = make_vec4(0.0f, greenValue, 0.0f, 1.0f);

		// render the triangle
		glDrawArrays(GL_TRIANGLES, 0, 3);

		// SDL2: Update SDL_Texture to latest rendered frame, then blit to screen
		// ----------------------------------------------------------------------
		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(u32));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteProgram(shaderProgram);

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
			
			if (sc == SDL_SCANCODE_ESCAPE) {
				return true;
			}
			break;

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
				scr_width = event.window.data1;
				scr_height = event.window.data2;

				bbufpix = (u32*)pglResizeFramebuffer(scr_width, scr_height);

				glViewport(0, 0, scr_width, scr_height);
				SDL_DestroyTexture(tex);
				tex = SDL_CreateTexture(ren, PIX_FORMAT, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
				break;
			}
			break;
		}
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

	window = SDL_CreateWindow("hello_triangle", 100, 100, scr_width, scr_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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


