#define PGL_PREFIX_TYPES
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
#include <cmath>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

using namespace glm;

// No geometry shader / layered glFramebufferTexture: one lookAt pass per
// cubemap face, attached with glFramebufferTexture2D.

struct My_Uniforms
{
	mat4 model;
	mat4 view;
	mat4 projection;
	mat4 shadowMatrix;

	GLuint tex;
	GLuint depthCubemap;

	vec3 lightPos;
	vec3 viewPos;
	float far_plane;
	int shadows;
	int reverse_normals;
};

void setup_context();
void cleanup();
bool handle_events();
unsigned int loadTexture(const char *path);
void renderScene();
void renderCube();

void lighting_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void lighting_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void depth_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void depth_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

vec4 toglm(pgl_vec4 v);

const unsigned int fbo_width = 640;
const unsigned int fbo_height = 480;
unsigned int scr_width = fbo_width;
unsigned int scr_height = fbo_height;
#ifndef NDEBUG
const unsigned int SHADOW_WIDTH = 256, SHADOW_HEIGHT = 256;
#else
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
#endif
bool shadows = true;

Camera camera(vec3(0.0f, 0.0f, 3.0f));

float deltaTime = 0.0f;
float lastFrame = 0.0f;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;

pix_t* bbufpix;
glContext the_Context;

My_Uniforms uniforms;

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	GLenum smooth[] = { PGL_SMOOTH3, PGL_SMOOTH3, PGL_SMOOTH2 };
	GLuint shader = pglCreateProgram(lighting_vs, lighting_fs, 8, smooth, GL_FALSE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	GLenum depthSmooth[] = { PGL_SMOOTH3 };
	GLuint depthShader = pglCreateProgram(depth_vs, depth_fs, 3, depthSmooth, GL_TRUE);
	glUseProgram(depthShader);
	pglSetUniform(&uniforms);

	unsigned int woodTexture = loadTexture(FileSystem::getPath("resources/textures/wood.png").c_str());

	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	unsigned int depthCubemap;
	glGenTextures(1, &depthCubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
	for (unsigned int i = 0; i < 6; ++i)
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, depthCubemap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Depth framebuffer is not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	uniforms.tex = woodTexture;
	uniforms.depthCubemap = depthCubemap;
	uniforms.shadows = shadows;
	vec3 lightPos(0.0f, 0.0f, 0.0f);

	while (true)
	{
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;

		if (handle_events())
			break;

		lightPos.z = sin(currentFrame / 1000.0f * 0.5f) * 3.0f;
		uniforms.lightPos = lightPos;

		float near_plane = 1.0f;
		float far_plane = 25.0f;
		uniforms.far_plane = far_plane;
		mat4 shadowProj = perspective(radians(90.0f), (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT, near_plane, far_plane);
		mat4 shadowTransforms[6];
		shadowTransforms[0] = shadowProj * lookAt(lightPos, lightPos + vec3( 1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f));
		shadowTransforms[1] = shadowProj * lookAt(lightPos, lightPos + vec3(-1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f));
		shadowTransforms[2] = shadowProj * lookAt(lightPos, lightPos + vec3( 0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f));
		shadowTransforms[3] = shadowProj * lookAt(lightPos, lightPos + vec3( 0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f));
		shadowTransforms[4] = shadowProj * lookAt(lightPos, lightPos + vec3( 0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f));
		shadowTransforms[5] = shadowProj * lookAt(lightPos, lightPos + vec3( 0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f));

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glUseProgram(depthShader);
		for (int face = 0; face < 6; ++face) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, depthCubemap, 0);
			glClear(GL_DEPTH_BUFFER_BIT);
			uniforms.shadowMatrix = shadowTransforms[face];
			renderScene();
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, scr_width, scr_height);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader);
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);
		uniforms.view = camera.GetViewMatrix();
		uniforms.viewPos = camera.Position;
		uniforms.shadows = shadows;
		uniforms.tex = woodTexture;
		renderScene();

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	glDeleteVertexArrays(1, &cubeVAO);
	glDeleteBuffers(1, &cubeVBO);
	cleanup();
	return 0;
}

void renderScene()
{
	mat4 model = mat4(1.0f);
	uniforms.model = scale(model, vec3(5.0f));
	glDisable(GL_CULL_FACE);
	uniforms.reverse_normals = 1;
	renderCube();
	uniforms.reverse_normals = 0;
	glEnable(GL_CULL_FACE);

	model = mat4(1.0f);
	model = translate(model, vec3(4.0f, -3.5f, 0.0f));
	uniforms.model = scale(model, vec3(0.5f));
	renderCube();
	model = mat4(1.0f);
	model = translate(model, vec3(2.0f, 3.0f, 1.0f));
	uniforms.model = scale(model, vec3(0.75f));
	renderCube();
	model = mat4(1.0f);
	model = translate(model, vec3(-3.0f, -1.0f, 0.0f));
	uniforms.model = scale(model, vec3(0.5f));
	renderCube();
	model = mat4(1.0f);
	model = translate(model, vec3(-1.5f, 1.0f, 1.5f));
	uniforms.model = scale(model, vec3(0.5f));
	renderCube();
	model = mat4(1.0f);
	model = translate(model, vec3(-1.5f, 2.0f, -3.0f));
	model = rotate(model, radians(60.0f), normalize(vec3(1.0f, 0.0f, 1.0f)));
	uniforms.model = scale(model, vec3(0.75f));
	renderCube();
}

void renderCube()
{
	if (cubeVAO == 0)
	{
		float vertices[] = {
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,

			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,

			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,

			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
			 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
			 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,

			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
			 1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
			 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
			-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,

			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
			 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
			 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
			-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
		glEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 3 * sizeof(float));
		glEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 6 * sizeof(float));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

vec4 toglm(pgl_vec4 v)
{
	return vec4(v.x, v.y, v.z, v.w);
}

void depth_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec4 worldPos = u->model * aPos;
	*(vec3*)&vs_output[0] = vec3(worldPos);
	*(vec4*)&builtins->gl_Position = u->shadowMatrix * worldPos;
}

void depth_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 FragPos = *(vec3*)&fs_input[0];
	builtins->gl_FragDepth = length(FragPos - u->lightPos) / u->far_plane;
}

void lighting_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec3 aNormal = vec3(((vec4*)vertex_attribs)[1]);
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[2]);

	vec4 worldPos = u->model * aPos;
	*(vec3*)&vs_output[0] = vec3(worldPos);
	mat3 normalMatrix = transpose(inverse(mat3(u->model)));
	*(vec3*)&vs_output[3] = u->reverse_normals ? normalMatrix * (-aNormal) : normalMatrix * aNormal;
	*(vec2*)&vs_output[6] = aTexCoords;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * worldPos;
}

void lighting_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 FragPos = *(vec3*)&fs_input[0];
	vec3 Normal = *(vec3*)&fs_input[3];
	vec2 TexCoords = *(vec2*)&fs_input[6];

	vec3 color = vec3(toglm(texture2D(u->tex, TexCoords.x, TexCoords.y)));
	vec3 normal = normalize(Normal);
	vec3 lightColor = vec3(0.3f);
	vec3 ambient = 0.3f * lightColor;
	vec3 lightDir = normalize(u->lightPos - FragPos);
	float diff = max(dot(lightDir, normal), 0.0f);
	vec3 diffuse = diff * lightColor;
	vec3 viewDir = normalize(u->viewPos - FragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(normal, halfwayDir), 0.0f), 64.0f);
	vec3 specular = spec * lightColor;

	float shadow = 0.0f;
	if (u->shadows) {
		vec3 fragToLight = FragPos - u->lightPos;
		float closestDepth = toglm(texture_cubemap(u->depthCubemap, fragToLight.x, fragToLight.y, fragToLight.z)).r;
		closestDepth *= u->far_plane;
		float currentDepth = length(fragToLight);
		float bias = 0.05f;
		shadow = currentDepth - bias > closestDepth ? 1.0f : 0.0f;
	}

	vec3 lighting = (ambient + (1.0f - shadow) * (diffuse + specular)) * color;
	*(vec4*)&builtins->gl_FragColor = vec4(lighting, 1.0f);
}

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
			case SDL_SCANCODE_SPACE:
				if (!event.key.repeat) {
					shadows = !shadows;
					std::cout << (shadows ? "Shadows on" : "Shadows off") << std::endl;
				}
				break;
			default:
				;
			}
			break;

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				SDL_SetRelativeMouseMode(SDL_TRUE);
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				SDL_SetRelativeMouseMode(SDL_FALSE);
				break;
			case SDL_WINDOWEVENT_RESIZED:
				scr_width = event.window.data1;
				scr_height = event.window.data2;
				pglResizeFramebuffer(scr_width, scr_height);
				bbufpix = (pix_t*)pglGetBackBuffer();
				glViewport(0, 0, scr_width, scr_height);
				SDL_DestroyTexture(tex);
				tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
				break;
			}
			break;

		case SDL_MOUSEMOTION:
		{
			float dx = event.motion.xrel;
			float dy = -event.motion.yrel;
			camera.ProcessMouseMovement(dx, dy);
		} break;

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

unsigned int loadTexture(char const * path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char *data = stbi_load(path, &width, &height, &nrComponents, STBI_rgb_alpha);
	if (data)
	{
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
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
