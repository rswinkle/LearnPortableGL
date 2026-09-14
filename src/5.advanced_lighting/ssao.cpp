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
#include <random>
#include <cmath>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

using namespace glm;

const unsigned int fbo_width = 640;
const unsigned int fbo_height = 480;
const int KERNEL_SIZE = 16; // original uses 64 samples per pixel

struct GBuffer_Uniforms : Model_Uniforms
{
	int invertedNormals;
};

struct SSAO_Uniforms
{
	GLuint gPosition;
	GLuint gNormal;
	GLuint texNoise;
	vec3 samples[KERNEL_SIZE];
	mat4 projection;
	vec2 noiseScale;
	int kernelSize;
	float radius;
	float bias;
};

struct Blur_Uniforms
{
	GLuint ssaoInput;
	vec2 texelSize;
};

struct Lighting_Uniforms
{
	GLuint gPosition;
	GLuint gNormal;
	GLuint gAlbedo;
	GLuint ssao;
	vec3 lightPos;
	vec3 lightColor;
	float lightLinear;
	float lightQuadratic;
};

void setup_context();
void cleanup();
bool handle_events();
void renderQuad();
void renderCube();
void gbuffer_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void gbuffer_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void ssao_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void ssao_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void blur_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void lighting_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
vec4 toglm(pgl_vec4 v);
float ourLerp(float a, float b, float f) { return a + f * (b - a); }

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

GBuffer_Uniforms gbufUniforms;
SSAO_Uniforms ssaoUniforms;
Blur_Uniforms blurUniforms;
Lighting_Uniforms lightingUniforms;

unsigned int cubeVAO = 0, cubeVBO = 0;
unsigned int quadVAO = 0, quadVBO = 0;

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);
	glEnable(GL_DEPTH_TEST);

	GLenum gSmooth[] = { PGL_SMOOTH3, PGL_SMOOTH3, PGL_SMOOTH2 };
	GLuint gbufShader = pglCreateProgram(gbuffer_vs, gbuffer_fs, 8, gSmooth, GL_FALSE);
	glUseProgram(gbufShader);
	pglSetUniform(&gbufUniforms);

	GLenum qSmooth[] = { PGL_SMOOTH2 };
	GLuint ssaoShader = pglCreateProgram(ssao_vs, ssao_fs, 2, qSmooth, GL_FALSE);
	glUseProgram(ssaoShader);
	pglSetUniform(&ssaoUniforms);

	GLuint blurShader = pglCreateProgram(ssao_vs, blur_fs, 2, qSmooth, GL_FALSE);
	glUseProgram(blurShader);
	pglSetUniform(&blurUniforms);

	GLuint lightingShader = pglCreateProgram(ssao_vs, lighting_fs, 2, qSmooth, GL_FALSE);
	glUseProgram(lightingShader);
	pglSetUniform(&lightingUniforms);

	Model backpack(FileSystem::getPath("resources/objects/backpack/backpack.obj"));

	unsigned int gBuffer;
	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	unsigned int gPosition, gNormal, gAlbedo;
	glGenTextures(1, &gPosition);
	glBindTexture(GL_TEXTURE_2D, gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_width, fbo_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_width, fbo_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

	glGenTextures(1, &gAlbedo);
	glBindTexture(GL_TEXTURE_2D, gAlbedo);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fbo_width, fbo_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedo, 0);

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

	unsigned int ssaoFBO, ssaoBlurFBO;
	glGenFramebuffers(1, &ssaoFBO);
	glGenFramebuffers(1, &ssaoBlurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
	unsigned int ssaoColorBuffer, ssaoColorBufferBlur;
	glGenTextures(1, &ssaoColorBuffer);
	glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, fbo_width, fbo_height, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "SSAO Framebuffer not complete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
	glGenTextures(1, &ssaoColorBufferBlur);
	glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, fbo_width, fbo_height, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
	std::default_random_engine generator;
	for (int i = 0; i < KERNEL_SIZE; ++i) {
		vec3 sample(randomFloats(generator) * 2.0f - 1.0f, randomFloats(generator) * 2.0f - 1.0f, randomFloats(generator));
		sample = normalize(sample);
		sample *= randomFloats(generator);
		float scale = ourLerp(0.1f, 1.0f, (float(i) / KERNEL_SIZE) * (float(i) / KERNEL_SIZE));
		ssaoUniforms.samples[i] = sample * scale;
	}

	vec4 ssaoNoise[16];
	for (int i = 0; i < 16; i++)
		ssaoNoise[i] = vec4(randomFloats(generator) * 2.0f - 1.0f, randomFloats(generator) * 2.0f - 1.0f, 0.0f, 1.0f);
	unsigned int noiseTexture;
	glGenTextures(1, &noiseTexture);
	glBindTexture(GL_TEXTURE_2D, noiseTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_FLOAT, ssaoNoise);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	ssaoUniforms.gPosition = gPosition;
	ssaoUniforms.gNormal = gNormal;
	ssaoUniforms.texNoise = noiseTexture;
	ssaoUniforms.noiseScale = vec2(fbo_width / 4.0f, fbo_height / 4.0f);
	ssaoUniforms.kernelSize = KERNEL_SIZE;
	ssaoUniforms.radius = 0.5f;
	ssaoUniforms.bias = 0.025f;

	blurUniforms.ssaoInput = ssaoColorBuffer;
	blurUniforms.texelSize = vec2(1.0f / fbo_width, 1.0f / fbo_height);

	lightingUniforms.gPosition = gPosition;
	lightingUniforms.gNormal = gNormal;
	lightingUniforms.gAlbedo = gAlbedo;
	lightingUniforms.ssao = ssaoColorBufferBlur;
	lightingUniforms.lightPos = vec3(2.0f, 4.0f, -2.0f);
	lightingUniforms.lightColor = vec3(0.2f, 0.2f, 0.7f);
	lightingUniforms.lightLinear = 0.09f;
	lightingUniforms.lightQuadratic = 0.032f;

	while (true)
	{
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (handle_events())
			break;

		mat4 projection = perspective(radians(camera.Zoom), (float)fbo_width / (float)fbo_height, 0.1f, 50.0f);
		mat4 view = camera.GetViewMatrix();

		glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
		glViewport(0, 0, fbo_width, fbo_height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(gbufShader);
		gbufUniforms.projection = projection;
		gbufUniforms.view = view;
		mat4 model = translate(mat4(1.0f), vec3(0.0f, 7.0f, 0.0f));
		gbufUniforms.model = scale(model, vec3(7.5f));
		gbufUniforms.invertedNormals = 1;
		renderCube();
		gbufUniforms.invertedNormals = 0;
		model = translate(mat4(1.0f), vec3(0.0f, 0.5f, 0.0f));
		model = rotate(model, radians(-90.0f), vec3(1.0f, 0.0f, 0.0f));
		gbufUniforms.model = scale(model, vec3(1.0f));
		backpack.Draw(gbufShader, &gbufUniforms);

		glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(ssaoShader);
		ssaoUniforms.projection = projection;
		renderQuad();

		glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(blurShader);
		renderQuad();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, scr_width, scr_height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(lightingShader);
		vec3 lightPosView = vec3(view * vec4(vec3(2.0f, 4.0f, -2.0f), 1.0f));
		lightingUniforms.lightPos = lightPosView;
		renderQuad();

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
			-1, -1, -1,  0,  0, -1, 0, 0,   1,  1, -1,  0,  0, -1, 1, 1,   1, -1, -1,  0,  0, -1, 1, 0,
			 1,  1, -1,  0,  0, -1, 1, 1,  -1, -1, -1,  0,  0, -1, 0, 0,  -1,  1, -1,  0,  0, -1, 0, 1,
			-1, -1,  1,  0,  0,  1, 0, 0,   1, -1,  1,  0,  0,  1, 1, 0,   1,  1,  1,  0,  0,  1, 1, 1,
			 1,  1,  1,  0,  0,  1, 1, 1,  -1,  1,  1,  0,  0,  1, 0, 1,  -1, -1,  1,  0,  0,  1, 0, 0,
			-1,  1,  1, -1,  0,  0, 1, 0,  -1,  1, -1, -1,  0,  0, 1, 1,  -1, -1, -1, -1,  0,  0, 0, 1,
			-1, -1, -1, -1,  0,  0, 0, 1,  -1, -1,  1, -1,  0,  0, 0, 0,  -1,  1,  1, -1,  0,  0, 1, 0,
			 1,  1,  1,  1,  0,  0, 1, 0,   1, -1, -1,  1,  0,  0, 0, 1,   1,  1, -1,  1,  0,  0, 1, 1,
			 1, -1, -1,  1,  0,  0, 0, 1,   1,  1,  1,  1,  0,  0, 1, 0,   1, -1,  1,  1,  0,  0, 0, 0,
			-1, -1, -1,  0, -1,  0, 0, 1,   1, -1, -1,  0, -1,  0, 1, 1,   1, -1,  1,  0, -1,  0, 1, 0,
			 1, -1,  1,  0, -1,  0, 1, 0,  -1, -1,  1,  0, -1,  0, 0, 0,  -1, -1, -1,  0, -1,  0, 0, 1,
			-1,  1, -1,  0,  1,  0, 0, 1,   1,  1,  1,  0,  1,  0, 1, 0,   1,  1, -1,  0,  1,  0, 1, 1,
			 1,  1,  1,  0,  1,  0, 1, 0,  -1,  1, -1,  0,  1,  0, 0, 1,  -1,  1,  1,  0,  1,  0, 0, 0
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

vec4 toglm(pgl_vec4 v) { return vec4(v.x, v.y, v.z, v.w); }

void gbuffer_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	GBuffer_Uniforms* u = (GBuffer_Uniforms*)uniforms;
	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec3 aNormal = vec3(((vec4*)vertex_attribs)[1]);
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[2]);
	vec4 viewPos = u->view * u->model * aPos;
	*(vec3*)&vs_output[0] = vec3(viewPos);
	*(vec2*)&vs_output[3] = aTexCoords;
	mat3 normalMatrix = transpose(inverse(mat3(u->view * u->model)));
	*(vec3*)&vs_output[5] = normalMatrix * (u->invertedNormals ? -aNormal : aNormal);
	*(vec4*)&builtins->gl_Position = u->projection * viewPos;
}

void gbuffer_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	vec3 FragPos = *(vec3*)&fs_input[0];
	vec3 Normal = *(vec3*)&fs_input[5];
	*(vec4*)&builtins->gl_FragData[0] = vec4(FragPos, 1.0f);
	*(vec4*)&builtins->gl_FragData[1] = vec4(normalize(Normal), 1.0f);
	*(vec4*)&builtins->gl_FragData[2] = vec4(0.95f, 0.95f, 0.95f, 1.0f);
}

void ssao_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	*(vec2*)&vs_output[0] = vec2(((vec4*)vertex_attribs)[1]);
	*(vec4*)&builtins->gl_Position = vec4(vec3(((vec4*)vertex_attribs)[0]), 1.0f);
}

void ssao_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	SSAO_Uniforms* u = (SSAO_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];
	vec3 fragPos = vec3(toglm(texture2D(u->gPosition, TexCoords.x, TexCoords.y)));
	vec3 normal = normalize(vec3(toglm(texture2D(u->gNormal, TexCoords.x, TexCoords.y))));
	vec3 randomVec = normalize(vec3(toglm(texture2D(u->texNoise, TexCoords.x * u->noiseScale.x, TexCoords.y * u->noiseScale.y))));
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN = mat3(tangent, bitangent, normal);
	float occlusion = 0.0f;
	for (int i = 0; i < u->kernelSize; ++i) {
		vec3 samplePos = TBN * u->samples[i];
		samplePos = fragPos + samplePos * u->radius;
		vec4 offset = u->projection * vec4(samplePos, 1.0f);
		vec3 ndc = vec3(offset) / offset.w;
		ndc = ndc * 0.5f + 0.5f;
		float sampleDepth = texture2D(u->gPosition, ndc.x, ndc.y).z;
		float rangeCheck = smoothstep(0.0f, 1.0f, u->radius / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + u->bias ? 1.0f : 0.0f) * rangeCheck;
	}
	occlusion = 1.0f - (occlusion / (float)u->kernelSize);
	*(vec4*)&builtins->gl_FragColor = vec4(occlusion, occlusion, occlusion, 1.0f);
}

void blur_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	Blur_Uniforms* u = (Blur_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];
	float result = 0.0f;
	for (int x = -2; x < 2; ++x)
		for (int y = -2; y < 2; ++y)
			result += texture2D(u->ssaoInput, TexCoords.x + x * u->texelSize.x, TexCoords.y + y * u->texelSize.y).x;
	result /= 16.0f;
	*(vec4*)&builtins->gl_FragColor = vec4(result, result, result, 1.0f);
}

void lighting_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	Lighting_Uniforms* u = (Lighting_Uniforms*)uniforms;
	vec2 TexCoords = *(vec2*)&fs_input[0];
	vec3 FragPos = vec3(toglm(texture2D(u->gPosition, TexCoords.x, TexCoords.y)));
	vec3 Normal = vec3(toglm(texture2D(u->gNormal, TexCoords.x, TexCoords.y)));
	vec3 Diffuse = vec3(toglm(texture2D(u->gAlbedo, TexCoords.x, TexCoords.y)));
	float AmbientOcclusion = texture2D(u->ssao, TexCoords.x, TexCoords.y).x;
	vec3 lighting = vec3(0.3f * Diffuse * AmbientOcclusion);
	vec3 viewDir = normalize(-FragPos);
	vec3 lightDir = normalize(u->lightPos - FragPos);
	vec3 diffuse = glm::max(dot(Normal, lightDir), 0.0f) * Diffuse * u->lightColor;
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(glm::max(dot(Normal, halfwayDir), 0.0f), 8.0f);
	vec3 specular = u->lightColor * spec;
	float distance = length(u->lightPos - FragPos);
	float attenuation = 1.0f / (1.0f + u->lightLinear * distance + u->lightQuadratic * distance * distance);
	lighting += (diffuse + specular) * attenuation;
	*(vec4*)&builtins->gl_FragColor = vec4(lighting, 1.0f);
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
