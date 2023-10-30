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


// using PGL's internal vector types and functions in these shaders
void interpolation_vs(float* vs_output, vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	((vec4*)vs_output)[0] = vertex_attribs[1];

	builtins->gl_Position = vertex_attribs[0];
}
void interpolation_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	// several ways to do this.
	builtins->gl_FragColor = make_vec4(fs_input[0], fs_input[1], fs_input[2], 1.0f);
}

int main()
{
	// Might as well make main a little cleaner
	setup_context();

	// build our shader program
	// ------------------------
	
	// The array controls the interpolation of the attributes passed between the
	// vertex and fragment shaders on an element basis. You can use PGL_SMOOTH
	// PGL_FLAT, or PGL_NOPERSPECTIVE for each element, or convenience macros
	// for 2, 3, and 4 of each.  So here PGL_SMOOTH3 expands to
	// PGL_SMOOTH, PGL_SMOOTH, PGL_SMOOTH which is what we need to interpolate
	// the r, g, and b components of the color
	GLenum smooth[3] = { PGL_SMOOTH3 };
	unsigned int shaderProgram = pglCreateProgram(interpolation_vs, interpolation_fs, 3, smooth, GL_FALSE);

	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------
	float vertices[] = {
		// positions         // colors
		 0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // bottom right
		-0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // bottom left
		 0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // top 
	};

	unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
	glEnableVertexAttribArray(0);
    // color attribute
    pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 3 * sizeof(float));
    glEnableVertexAttribArray(1);

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    // glBindVertexArray(0);

    // as we only have a single shader, we could also just activate our shader once beforehand if we want to 
    glUseProgram(shaderProgram);

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



