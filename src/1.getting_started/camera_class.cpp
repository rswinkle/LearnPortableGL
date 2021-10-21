#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define MANGLE_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

struct My_Uniforms
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
	GLuint tex1;
	GLuint tex2;
	
};

void setup_context();
void cleanup();
bool handle_events();

void camera_vs(float* vs_output, void* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void camera_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);


// settings
const unsigned int SCR_WIDTH = 640;
const unsigned int SCR_HEIGHT = 480;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;

u32* bbufpix;

glContext the_Context;

GLuint ourShader;
My_Uniforms the_uniforms;

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

	// Create our shader program
	// ------------------------------------
	GLenum smooth[2] = { SMOOTH, SMOOTH };
	ourShader = pglCreateProgram(camera_vs, camera_fs, 2, smooth, GL_FALSE);
	glUseProgram(ourShader);
	pglSetUniform(&the_uniforms);

	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------
	float vertices[] = {
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
	// world space positions of our cubes
	glm::vec3 cubePositions[] = {
		glm::vec3( 0.0f,  0.0f,  0.0f),
		glm::vec3( 2.0f,  5.0f, -15.0f),
		glm::vec3(-1.5f, -2.2f, -2.5f),
		glm::vec3(-3.8f, -2.0f, -12.3f),
		glm::vec3( 2.4f, -0.4f, -3.5f),
		glm::vec3(-1.7f,  3.0f, -7.5f),
		glm::vec3( 1.3f, -2.0f, -2.5f),
		glm::vec3( 1.5f,  2.0f, -2.5f),
		glm::vec3( 1.5f,  0.2f, -1.5f),
		glm::vec3(-1.3f,  1.0f, -1.5f)
	};
	unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// position attribute TODO change type of parameter to match OpenGL's stupidity?
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
	glEnableVertexAttribArray(0);
	// texture coord attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 3 * sizeof(float));
	glEnableVertexAttribArray(1);


	// load and create a texture 
	// -------------------------
	unsigned int texture1, texture2;
	// texture 1
	// ---------
	glGenTextures(1, &texture1);
	glBindTexture(GL_TEXTURE_2D, texture1);
	// set the texture wrapping parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// load image, create texture and generate mipmaps
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
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

	// Set the uniforms to texture handles (no active textures/texture units in PortableGL)
	// -------------------------------------------------------------------------------------------
	glUseProgram(ourShader);
	pglSetUniform(&the_uniforms);
	the_uniforms.tex1 = texture1;
	the_uniforms.tex2 = texture2;


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
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

		// bind textures on corresponding texture units
		//glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, texture1);
		//glActiveTexture(GL_TEXTURE1);
		//glBindTexture(GL_TEXTURE_2D, texture2);

		// activate shader TODO (we never change shaders so why call this?)
		glUseProgram(ourShader);

		// pass projection matrix to shader (note that in this case it could change every frame)
		the_uniforms.projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

		// camera/view transformation
		the_uniforms.view = camera.GetViewMatrix();

		// render boxes
		glBindVertexArray(VAO);
		for (unsigned int i = 0; i < 10; i++)
		{
			// calculate the model matrix for each object and pass it to shader before drawing
			glm::mat4 model = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
			model = glm::translate(model, cubePositions[i]);
			float angle = 20.0f * i;
			the_uniforms.model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));

			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		// SDL2: blit texture (ie latest rendered frame) to screen
		// -------------------------------------------------------------------------------
		SDL_UpdateTexture(tex, NULL, bbufpix, SCR_WIDTH * sizeof(u32));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);

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

		/*
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
				printf("window size %d x %d\n", event.window.data1, event.window.data2);
				width = event.window.data1;
				height = event.window.data2;
				mousex = width/2;
				mousey = height/2;

				remake_projection = true;

			pglResizeFramebuffer(width, height);
				glViewport(0, 0, width, height);
				SDL_DestroyTexture(tex);
				tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
				break;
			}
			break;
			*/

		case SDL_MOUSEMOTION:
		{
			float dx = event.motion.xrel;
			float dy = event.motion.yrel;
			camera.ProcessMouseMovement(dx, -dy);
		} break;

		case SDL_MOUSEWHEEL:
			camera.ProcessMouseScroll(event.wheel.y);
			break;
		}
	}

	const Uint8 *state = SDL_GetKeyboardState(NULL);
	
	if (state[SDL_SCANCODE_A]) {
		camera.ProcessKeyboard(LEFT, deltaTime);
	}
	if (state[SDL_SCANCODE_D]) {
		camera.ProcessKeyboard(RIGHT, deltaTime);
	}
	if (state[SDL_SCANCODE_W]) {
		camera.ProcessKeyboard(FORWARD, deltaTime);
	}
	if (state[SDL_SCANCODE_S]) {
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	}

	return false;
}

/*
// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}
*/



void setup_context()
{
	// Initialize SDL2 and create window
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO)) {
		std::cout << "SDL_Init error: " << SDL_GetError() << "\n";
		exit(0);
	}

	window = SDL_CreateWindow("camera_class", 100, 100, SCR_WIDTH, SCR_HEIGHT, SDL_WINDOW_SHOWN);
	if (!window) {
		std::cerr << "Failed to create window\n";
		SDL_Quit();
		exit(0);
	}

	// Create Software Renderer and texture
	ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCR_WIDTH, SCR_HEIGHT);

	// Initialize and set PGL context
	if (!init_glContext(&the_Context, &bbufpix, SCR_WIDTH, SCR_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)) {
		puts("Failed to initialize glContext");
		exit(0);
	}
	set_glContext(&the_Context);
}

void camera_vs(float* vs_output, void* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	glm::mat4 model = u->model;
	glm::mat4 view = u->view;
	glm::mat4 projection = u->projection;

	// The 1 in w is there by default according to spec
	glm::vec4 aPos = ((glm::vec4*)vertex_attribs)[0];

	// TexCoord = vec2(aTexCoord.x, aTexCoord.y)
	//((glm::vec2*)vs_output)[0] = ((glm::vec4*)vertex_attribs)[1].xy(); //tex_coords
	vs_output[0] = ((glm::vec4*)vertex_attribs)[1].x;
	vs_output[1] = ((glm::vec4*)vertex_attribs)[1].y;

	*(glm::vec4*)&builtins->gl_Position = projection * view * model * aPos;
}

void camera_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	glm::vec2 tc = ((glm::vec2*)fs_input)[0];

	GLuint tex1 = u->tex1;
	GLuint tex2 = u->tex2;

	// linearly interpolate between both textures (80% container, 20% awesomeface)
	builtins->gl_FragColor = mix_vec4s(texture2D(tex1, tc.x, tc.y), texture2D(tex2, tc.x, tc.y), 0.2);

	// use glm::mix somehow
	//builtins->gl_FragColor = glm::mix(texture2D(tex1, tc.x, tc.y) texture2D(tex2, tc.x, tc.y), 0.2);
}

void cleanup()
{
	free_glContext(&the_Context);

	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(window);

	SDL_Quit();
}

