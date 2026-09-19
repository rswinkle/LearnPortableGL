#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <learnopengl/camera.h>

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

using namespace glm;

struct My_Uniforms
{
	mat4 mvp;
	vec4 color;
	GLuint accumTex;
	GLuint revealTex;
	GLuint opaqueTex;
	int pass; // 0 accum, 1 reveal
};

void setup_context();
void cleanup();
bool handle_events();
mat4 calculate_model_matrix(const vec3& position, const vec3& rotation = vec3(0.0f), const vec3& scale = vec3(1.0f));

void solid_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void solid_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void transparent_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void transparent_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void composite_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void composite_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

vec4 toglm(pgl_vec4 v) { return vec4(v.x, v.y, v.z, v.w); }

unsigned int scr_width = 640;
unsigned int scr_height = 480;
Camera camera(vec3(0.0f, 0.0f, 5.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;
My_Uniforms uniforms;

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);

	GLenum smooth2[] = { PGL_SMOOTH2 };
	GLuint solidShader = pglCreateProgram(solid_vs, solid_fs, 0, NULL, GL_FALSE);
	GLuint transparentShader = pglCreateProgram(transparent_vs, transparent_fs, 0, NULL, GL_FALSE);
	GLuint compositeShader = pglCreateProgram(composite_vs, composite_fs, 2, smooth2, GL_TRUE);
	glUseProgram(solidShader); pglSetUniform(&uniforms);
	glUseProgram(transparentShader); pglSetUniform(&uniforms);
	glUseProgram(compositeShader); pglSetUniform(&uniforms);

	float quadVertices[] = {
		-1.0f, -1.0f, 0.0f,	0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f
	};
	unsigned int quadVAO, quadVBO;
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
	glEnableVertexAttribArray(1);
	pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 3 * sizeof(float));

	unsigned int opaqueFBO, transparentFBO;
	glGenFramebuffers(1, &opaqueFBO);
	glGenFramebuffers(1, &transparentFBO);

	unsigned int opaqueTexture;
	glGenTextures(1, &opaqueTexture);
	glBindTexture(GL_TEXTURE_2D, opaqueTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, scr_width, scr_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	unsigned int depthTexture;
	glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, scr_width, scr_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opaqueTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Opaque framebuffer is not complete!" << std::endl;

	unsigned int accumTexture;
	glGenTextures(1, &accumTexture);
	glBindTexture(GL_TEXTURE_2D, accumTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, scr_width, scr_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	unsigned int revealTexture;
	glGenTextures(1, &revealTexture);
	glBindTexture(GL_TEXTURE_2D, revealTexture);
	// RGBA16F, not GL_R8: PGL has no packed R8, and 1-channel float is enough
	// for the fetch; 4-channel matches accum and texelFetch2D.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, scr_width, scr_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, transparentFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, revealTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Transparent framebuffer is not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	mat4 redModelMat = calculate_model_matrix(vec3(0.0f, 0.0f, 1.0f));
	mat4 greenModelMat = calculate_model_matrix(vec3(0.0f, 0.0f, 0.0f));
	mat4 blueModelMat = calculate_model_matrix(vec3(0.0f, 0.0f, 2.0f));

	while (true)
	{
		fps_log_tick();
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (handle_events())
			break;

		mat4 projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);
		mat4 view = camera.GetViewMatrix();
		mat4 vp = projection * view;

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(solidShader);
		glBindVertexArray(quadVAO);
		uniforms.mvp = vp * redModelMat;
		uniforms.color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		glBindFramebuffer(GL_FRAMEBUFFER, transparentFBO);

		// No glBlendFunci / glClearBufferfv: two passes + draw-buffer clears.
		GLenum accumOnly[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, accumOnly);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		GLenum revealOnly[] = { GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(1, revealOnly);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(transparentShader);
		glBindVertexArray(quadVAO);

		glDrawBuffers(1, accumOnly);
		glBlendFunc(GL_ONE, GL_ONE);
		uniforms.pass = 0;
		uniforms.mvp = vp * greenModelMat;
		uniforms.color = vec4(0.0f, 1.0f, 0.0f, 0.5f);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		uniforms.mvp = vp * blueModelMat;
		uniforms.color = vec4(0.0f, 0.0f, 1.0f, 0.5f);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDrawBuffers(1, revealOnly);
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		uniforms.pass = 1;
		uniforms.mvp = vp * greenModelMat;
		uniforms.color = vec4(0.0f, 1.0f, 0.0f, 0.5f);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		uniforms.mvp = vp * blueModelMat;
		uniforms.color = vec4(0.0f, 0.0f, 1.0f, 0.5f);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// PGL float FBO attachments replace (no blend), so the original
		// SRC_ALPHA composite into opaque RGBA16F would wipe the red quad
		// with the blue average color. Apply the over-operator in the
		// shader and write the default (U8) backbuffer instead.
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(compositeShader);
		uniforms.accumTex = accumTexture;
		uniforms.revealTex = revealTexture;
		uniforms.opaqueTex = opaqueTexture;
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

void solid_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)vs_output;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec4 pos = ((vec4*)vertex_attribs)[0];
	*(vec4*)&builtins->gl_Position = u->mvp * vec4(vec3(pos), 1.0f);
}

void solid_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)fs_input;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	*(vec4*)&builtins->gl_FragColor = vec4(vec3(u->color), 1.0f);
}

void transparent_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)vs_output;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec4 pos = ((vec4*)vertex_attribs)[0];
	*(vec4*)&builtins->gl_Position = u->mvp * vec4(vec3(pos), 1.0f);
}

void transparent_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)fs_input;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec4 color = u->color;
	float z = builtins->gl_FragCoord.z;
	float weight = clamp(pow(min(1.0f, color.a * 10.0f) + 0.01f, 3.0f) * 1e8f * pow(1.0f - z * 0.9f, 3.0f), 1e-2f, 3e3f);
	if (u->pass == 0)
		*(vec4*)&builtins->gl_FragColor = vec4(vec3(color) * color.a, color.a) * weight;
	else
		*(vec4*)&builtins->gl_FragColor = vec4(color.a, color.a, color.a, color.a);
}

void composite_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	vec4 pos = ((vec4*)vertex_attribs)[0];
	vec2 uv = vec2(((vec4*)vertex_attribs)[1]);
	*(vec2*)&vs_output[0] = uv;
	*(vec4*)&builtins->gl_Position = vec4(vec3(pos), 1.0f);
}

void composite_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)fs_input;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	int x = (int)builtins->gl_FragCoord.x;
	int y = (int)builtins->gl_FragCoord.y;
	vec4 opaque = toglm(texelFetch2D(u->opaqueTex, x, y, 0));
	float revealage = toglm(texelFetch2D(u->revealTex, x, y, 0)).r;
	if (fabs(revealage - 1.0f) <= 0.00001f) {
		*(vec4*)&builtins->gl_FragColor = vec4(vec3(opaque), 1.0f);
		return;
	}
	vec4 accumulation = toglm(texelFetch2D(u->accumTex, x, y, 0));
	vec3 average_color = vec3(accumulation) / max(accumulation.a, 0.00001f);
	float a = 1.0f - revealage;
	*(vec4*)&builtins->gl_FragColor = vec4(average_color * a + vec3(opaque) * (1.0f - a), 1.0f);
}

mat4 calculate_model_matrix(const vec3& position, const vec3& rotation, const vec3& scale)
{
	mat4 trans = mat4(1.0f);
	trans = translate(trans, position);
	trans = rotate(trans, radians(rotation.x), vec3(1.0, 0.0, 0.0));
	trans = rotate(trans, radians(rotation.y), vec3(0.0, 1.0, 0.0));
	trans = rotate(trans, radians(rotation.z), vec3(0.0, 0.0, 1.0));
	trans = glm::scale(trans, scale);
	return trans;
}

bool handle_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT: return true;
		case SDL_KEYDOWN:
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) return true;
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
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
			camera.ProcessMouseMovement((float)event.motion.xrel, -(float)event.motion.yrel);
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
	if (!window) { std::cerr << "Failed to create window\n"; SDL_Quit(); exit(0); }
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
