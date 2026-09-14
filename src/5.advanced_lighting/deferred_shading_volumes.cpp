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
#include <learnopengl/model.h>
#include <uniforms.h>

#include <iostream>
#include <cstdlib>
#include <cmath>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

using namespace glm;

// Original uses 32 lights; 16 keeps the software lighting pass tractable.
// Lighting loop skips fragments outside each light's radius (original volume cull).
const int NR_LIGHTS = 16;
const unsigned int fbo_width = 640;
const unsigned int fbo_height = 480;

struct Lighting_Uniforms
{
	GLuint gPosition;
	GLuint gNormal;
	GLuint gAlbedoSpec;
	vec3 lightPositions[NR_LIGHTS];
	vec3 lightColors[NR_LIGHTS];
	float lightLinear[NR_LIGHTS];
	float lightQuadratic[NR_LIGHTS];
	float lightRadius[NR_LIGHTS];
	vec3 viewPos;
	mat4 view;
	mat4 projection;
};

struct LightBox_Uniforms
{
	mat4 model;
	mat4 view;
	mat4 projection;
	vec3 lightColor;
};

void setup_context();
void cleanup();
bool handle_events();
void renderQuad();
void renderCube();

void gbuffer_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void gbuffer_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void lighting_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void lighting_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void lightbox_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void lightbox_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
vec4 toglm(pgl_vec4 v);

unsigned int scr_width = fbo_width;
unsigned int scr_height = fbo_height;
Camera camera(vec3(0.0f, 0.0f, 5.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;

Model_Uniforms gbufUniforms;
Lighting_Uniforms lightingUniforms;
LightBox_Uniforms lightBoxUniforms;

unsigned int cubeVAO = 0, cubeVBO = 0;
unsigned int quadVAO = 0, quadVBO = 0;

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);
	stbi_set_flip_vertically_on_load(true);
	glEnable(GL_DEPTH_TEST);

	GLenum gSmooth[] = { PGL_SMOOTH3, PGL_SMOOTH3, PGL_SMOOTH2 };
	GLuint gbufShader = pglCreateProgram(gbuffer_vs, gbuffer_fs, 8, gSmooth, GL_FALSE);
	glUseProgram(gbufShader);
	pglSetUniform(&gbufUniforms);

	GLenum qSmooth[] = { PGL_SMOOTH2 };
	GLuint lightingShader = pglCreateProgram(lighting_vs, lighting_fs, 2, qSmooth, GL_TRUE);
	glUseProgram(lightingShader);
	pglSetUniform(&lightingUniforms);

	GLuint lightBoxShader = pglCreateProgram(lightbox_vs, lightbox_fs, 0, NULL, GL_FALSE);
	glUseProgram(lightBoxShader);
	pglSetUniform(&lightBoxUniforms);

	Model backpack(FileSystem::getPath("resources/objects/backpack/backpack.obj"));
	vec3 objectPositions[] = {
		vec3(-3.0f, -0.5f, -3.0f), vec3( 0.0f, -0.5f, -3.0f), vec3( 3.0f, -0.5f, -3.0f),
		vec3(-3.0f, -0.5f,  0.0f), vec3( 0.0f, -0.5f,  0.0f), vec3( 3.0f, -0.5f,  0.0f),
		vec3(-3.0f, -0.5f,  3.0f), vec3( 0.0f, -0.5f,  3.0f), vec3( 3.0f, -0.5f,  3.0f)
	};

	unsigned int gBuffer;
	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	unsigned int gPosition, gNormal, gAlbedoSpec;
	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_width, fbo_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_width, fbo_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

	glGenTextures(1, &gAlbedoSpec);
	glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_width, fbo_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);

	GLenum attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);

	unsigned int rboDepth;
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fbo_width, fbo_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	srand(13);
	const float linear = 0.7f;
	const float quadratic = 1.8f;
	for (int i = 0; i < NR_LIGHTS; i++) {
		lightingUniforms.lightPositions[i] = vec3(
			((rand() % 100) / 100.0f) * 6.0f - 3.0f,
			((rand() % 100) / 100.0f) * 6.0f - 4.0f,
			((rand() % 100) / 100.0f) * 6.0f - 3.0f);
		lightingUniforms.lightColors[i] = vec3(
			(rand() % 100) / 200.0f + 0.5f,
			(rand() % 100) / 200.0f + 0.5f,
			(rand() % 100) / 200.0f + 0.5f);
		lightingUniforms.lightLinear[i] = linear;
		lightingUniforms.lightQuadratic[i] = quadratic;
		const float constant = 1.0f;
		float maxBrightness = glm::max(glm::max(lightingUniforms.lightColors[i].r, lightingUniforms.lightColors[i].g), lightingUniforms.lightColors[i].b);
		lightingUniforms.lightRadius[i] = (-linear + sqrt(linear * linear - 4.0f * quadratic * (constant - (256.0f / 5.0f) * maxBrightness))) / (2.0f * quadratic);
	}
	lightingUniforms.gPosition = gPosition;
	lightingUniforms.gNormal = gNormal;
	lightingUniforms.gAlbedoSpec = gAlbedoSpec;

	while (true)
	{
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (handle_events())
			break;

		mat4 projection = perspective(radians(camera.Zoom), (float)fbo_width / (float)fbo_height, 0.1f, 100.0f);
		mat4 view = camera.GetViewMatrix();

		glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
		glViewport(0, 0, fbo_width, fbo_height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(gbufShader);
		gbufUniforms.projection = projection;
		gbufUniforms.view = view;
		for (int i = 0; i < 9; i++) {
			mat4 model = translate(mat4(1.0f), objectPositions[i]);
			gbufUniforms.model = scale(model, vec3(0.25f));
			backpack.Draw(gbufShader, &gbufUniforms);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, scr_width, scr_height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(lightingShader);
		lightingUniforms.viewPos = camera.Position;
		lightingUniforms.view = view;
		lightingUniforms.projection = projection;
		renderQuad();

		glUseProgram(lightBoxShader);
		lightBoxUniforms.projection = projection;
		lightBoxUniforms.view = view;
		for (int i = 0; i < NR_LIGHTS; i++) {
			mat4 model = translate(mat4(1.0f), lightingUniforms.lightPositions[i]);
			lightBoxUniforms.model = scale(model, vec3(0.125f));
			lightBoxUniforms.lightColor = lightingUniforms.lightColors[i];
			renderCube();
		}

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

void renderCube()
{
	if (cubeVAO == 0) {
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
		glBindVertexArray(0);
	}
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void renderQuad()
{
	if (quadVAO == 0) {
		float quadVertices[] = {
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
		glEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 3 * sizeof(float));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

vec4 toglm(pgl_vec4 v)
{
	return vec4(v.x, v.y, v.z, v.w);
}

void gbuffer_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	Model_Uniforms* u = (Model_Uniforms*)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec3 aNormal = vec3(((vec4*)vertex_attribs)[1]);
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[2]);
	vec4 worldPos = u->model * aPos;
	*(vec3*)&vs_output[0] = vec3(worldPos);
	*(vec3*)&vs_output[3] = transpose(inverse(mat3(u->model))) * aNormal;
	*(vec2*)&vs_output[6] = aTexCoords;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * worldPos;
}

void gbuffer_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	Model_Uniforms* u = (Model_Uniforms*)uniforms;
	vec3 FragPos = *(vec3*)&fs_input[0];
	vec3 Normal = *(vec3*)&fs_input[3];
	vec2 TexCoords = *(vec2*)&fs_input[6];
	*(vec4*)&builtins->gl_FragData[0] = vec4(FragPos, 1.0f);
	*(vec4*)&builtins->gl_FragData[1] = vec4(normalize(Normal), 1.0f);
	vec3 diff = vec3(toglm(texture2D(u->texture_diffuse[0], TexCoords.x, TexCoords.y)));
	float spec = toglm(texture2D(u->texture_specular[0], TexCoords.x, TexCoords.y)).r;
	*(vec4*)&builtins->gl_FragData[2] = vec4(diff, spec);
}

void lighting_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	*(vec2*)&vs_output[0] = vec2(((vec4*)vertex_attribs)[1]);
	*(vec4*)&builtins->gl_Position = vec4(vec3(aPos), 1.0f);
}

void lighting_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	Lighting_Uniforms* u = (Lighting_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];
	vec3 FragPos = vec3(toglm(texture2D(u->gPosition, TexCoords.x, TexCoords.y)));
	vec3 Normal = vec3(toglm(texture2D(u->gNormal, TexCoords.x, TexCoords.y)));
	vec4 albedoSpec = toglm(texture2D(u->gAlbedoSpec, TexCoords.x, TexCoords.y));
	vec3 Diffuse = vec3(albedoSpec);
	float Specular = albedoSpec.a;

	vec3 lighting = Diffuse * 0.1f;
	vec3 viewDir = normalize(u->viewPos - FragPos);
	for (int i = 0; i < NR_LIGHTS; ++i) {
		float distance = length(u->lightPositions[i] - FragPos);
		if (distance >= u->lightRadius[i])
			continue;
		vec3 lightDir = normalize(u->lightPositions[i] - FragPos);
		vec3 diffuse = glm::max(dot(Normal, lightDir), 0.0f) * Diffuse * u->lightColors[i];
		vec3 halfwayDir = normalize(lightDir + viewDir);
		float spec = pow(glm::max(dot(Normal, halfwayDir), 0.0f), 16.0f);
		vec3 specular = u->lightColors[i] * spec * Specular;
		float attenuation = 1.0f / (1.0f + u->lightLinear[i] * distance + u->lightQuadratic[i] * distance * distance);
		lighting += (diffuse + specular) * attenuation;
	}
	*(vec4*)&builtins->gl_FragColor = vec4(lighting, 1.0f);

	// glBlitFramebuffer is a stub; reconstruct scene depth so light boxes occlude.
	if (length(Normal) < 0.01f)
		builtins->gl_FragDepth = 1.0f;
	else {
		vec4 clip = u->projection * u->view * vec4(FragPos, 1.0f);
		builtins->gl_FragDepth = clip.z / clip.w * 0.5f + 0.5f;
	}
}

void lightbox_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)vs_output;
	LightBox_Uniforms* u = (LightBox_Uniforms*)uniforms;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * ((vec4*)vertex_attribs)[0];
}

void lightbox_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)fs_input;
	LightBox_Uniforms* u = (LightBox_Uniforms*)uniforms;
	*(vec4*)&builtins->gl_FragColor = vec4(u->lightColor, 1.0f);
}

bool handle_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			return true;
		case SDL_KEYDOWN:
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
				return true;
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				SDL_SetRelativeMouseMode(SDL_TRUE);
			else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				SDL_SetRelativeMouseMode(SDL_FALSE);
			else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
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
			camera.ProcessMouseMovement(event.motion.xrel, -event.motion.yrel);
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
