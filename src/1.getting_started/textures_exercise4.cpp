#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <learnopengl/filesystem.h>

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

void setup_context();
void cleanup();
bool handle_events();

// settings
unsigned int scr_width = 640;
unsigned int scr_height = 480;

// stores how much we're seeing of either texture
float mixValue = 0.2f;

#define PIX_FORMAT SDL_PIXELFORMAT_ARGB8888

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
u32* bbufpix;

glContext the_Context;

struct My_Uniforms
{
	GLuint tex1;
	GLuint tex2;
	float mixValue;
};

// using PGL's internal vector types and functions in these shaders
void texture_vs(float* vs_output, void* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	builtins->gl_Position = ((vec4*)vertex_attribs)[0];

	vec4 aColor = ((vec4*)vertex_attribs)[1];
	// ourColor = aColor;
	vs_output[0] = aColor.x;
	vs_output[1] = aColor.y;
	vs_output[2] = aColor.z;

	// TexCoord = aTexCoord;
	vs_output[3] = ((vec4*)vertex_attribs)[2].x;
	vs_output[4] = ((vec4*)vertex_attribs)[2].y;

}
void texture_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	// TODO add vector versions of texture mapping functions to PGL
	//
	// FragColor = mix(texture(tex1, TexCoord), texture(tex2, TexCoord), 0.2);
	builtins->gl_FragColor = mix_vec4s(texture2D(u->tex1, fs_input[3], fs_input[4]), texture2D(u->tex2, fs_input[3], fs_input[4]), u->mixValue);
}

int main()
{
	setup_context();

	// build our shader program
	// ------------------------
	GLenum smooth[5] = { SMOOTH, SMOOTH, SMOOTH, SMOOTH, SMOOTH };
	unsigned int ourShader = pglCreateProgram(texture_vs, texture_fs, 5, smooth, GL_FALSE);
	glUseProgram(ourShader);
	My_Uniforms uniforms;
	pglSetUniform(&uniforms);

	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------
	float vertices[] = {
		// positions          // colors           // texture coords
		 0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top right
		 0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // bottom right
		-0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // bottom left
		-0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f  // top left
	};
	unsigned int indices[] = {
		0, 1, 3, // first triangle
		1, 2, 3  // second triangle
	};
	unsigned int VBO, VAO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
	glEnableVertexAttribArray(0);
	// color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 3 * sizeof(float));
	glEnableVertexAttribArray(1);
	// texture coord attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 6 * sizeof(float));
	glEnableVertexAttribArray(2);


	// load and create a texture
	// -------------------------
	unsigned int texture1, texture2;
	// texture 1
	// ---------
	glGenTextures(1, &texture1);
	glBindTexture(GL_TEXTURE_2D, texture1);
	 // set the texture wrapping parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// load image, create texture and generate mipmaps
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
	// The FileSystem::getPath(...) is part of the GitHub repository so we can find files on any IDE/platform; replace it with your own image path.
	//
	// NOTE: PGL currently only supports 32-bit RGBA, so always requset that of stbi_load
	unsigned char *data = stbi_load(FileSystem::getPath("resources/textures/container.jpg").c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);
	// texture 2
	// ---------
	glGenTextures(1, &texture2);
	glBindTexture(GL_TEXTURE_2D, texture2);
	// set the texture wrapping parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// load image, create texture and generate mipmaps
	data = stbi_load(FileSystem::getPath("resources/textures/awesomeface.png").c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);
	if (data)
	{
		// note that the awesomeface.png has transparency and thus an alpha channel, so make sure to tell OpenGL the data type is of GL_RGBA
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);

	// set uniforms (uses handles, not "Active Unit")
	uniforms.tex1 = texture1;
	uniforms.tex2 = texture2;
	uniforms.mixValue = mixValue;


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

		// set the texture mix value in the shader
		uniforms.mixValue = mixValue;

		// render container
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

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
	glDeleteBuffers(1, &EBO);
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
			
			if (sc == SDL_SCANCODE_ESCAPE) {
				return true;
			} else if (sc == SDL_SCANCODE_UP) {
				mixValue += 0.001f; // change for preferences/hardware
				if (mixValue >= 1.0f)
					mixValue = 1.0f;
			} else if (sc == SDL_SCANCODE_DOWN) {
				mixValue -= 0.001f; // change for preferences/hardware
				if (mixValue <= 0.0f)
					mixValue = 0.0f;
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

	window = SDL_CreateWindow("textures_exercise4", 100, 100, scr_width, scr_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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


