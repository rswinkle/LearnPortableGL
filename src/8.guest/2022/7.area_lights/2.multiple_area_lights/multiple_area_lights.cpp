#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>
#include "../ltc_matrix.hpp"
#include "../colors.hpp"

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

using namespace glm;

struct Light {
	float intensity;
	vec3 color;
	vec3 points[4];
	int twoSided;
};

struct My_Uniforms {
	mat4 model, view, projection;
	mat3 normalMatrix;
	vec3 viewPosition;
	vec3 areaLightTranslate;
	Light areaLight;
	GLuint LTC1, LTC2, diffuse;
	vec4 albedoRoughness;
	vec3 lightColor;
	vec3 lightColor2;
};

void setup_context();
void cleanup();
bool handle_events();
unsigned int loadTexture(const char *path, bool gammaCorrection);
void configureMockupData();
void renderPlane();
void renderAreaLight();
GLuint loadMTexture();
GLuint loadLUTTexture();

void ltc_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void ltc_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void plane_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void plane_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

vec4 toglm(pgl_vec4 v) { return vec4(v.x, v.y, v.z, v.w); }

unsigned int scr_width = 640, scr_height = 480;
const vec3 LIGHT_COLOR = Color::BurlyWood;
const vec3 LIGHT_COLOR2 = Color::SteelBlue;
const vec3 LIGHT2_OFFSET = vec3(16.0f, 0.0f, 0.0f);
// Original multi-light overview: both quads (x=-8 and x=+8) in frame.
Camera camera(vec3(-0.224556f, 10.4038f, -18.9259f), vec3(0.0f, 1.0f, 0.0f), 89.3999f, -34.3001f);
float deltaTime = 0.0f, lastFrame = 0.0f;
vec3 areaLightTranslate(0.0f);
float roughness = 0.5f;
float intensity = 4.0f;
int twoSided = 1;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;
My_Uniforms uniforms;

struct VertexAL { vec3 position, normal; vec2 texcoord; };
const GLfloat psize = 10.0f;
VertexAL planeVertices[6] = {
	{ {-psize, 0.0f, -psize}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
	{ {-psize, 0.0f,  psize}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f} },
	{ { psize, 0.0f,  psize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} },
	{ {-psize, 0.0f, -psize}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} },
	{ { psize, 0.0f,  psize}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f} },
	{ { psize, 0.0f, -psize}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f} }
};
VertexAL areaLightVertices[6] = {
	{ {-8.0f, 2.4f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
	{ {-8.0f, 2.4f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f} },
	{ {-8.0f, 0.4f,  1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} },
	{ {-8.0f, 2.4f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f} },
	{ {-8.0f, 0.4f,  1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f} },
	{ {-8.0f, 0.4f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f} }
};
GLuint planeVBO, planeVAO, areaLightVBO, areaLightVAO;

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);
	glEnable(GL_DEPTH_TEST);

	GLenum smooth8[] = { PGL_SMOOTH2, PGL_SMOOTH3, PGL_SMOOTH3 };
	GLuint shaderLTC = pglCreateProgram(ltc_vs, ltc_fs, 8, smooth8, GL_FALSE);
	GLuint shaderLight = pglCreateProgram(plane_vs, plane_fs, 0, NULL, GL_FALSE);
	glUseProgram(shaderLTC); pglSetUniform(&uniforms);
	glUseProgram(shaderLight); pglSetUniform(&uniforms);

	uniforms.LTC1 = loadMTexture();
	uniforms.LTC2 = loadLUTTexture();
	uniforms.diffuse = loadTexture(FileSystem::getPath("resources/textures/concreteTexture.png").c_str(), true);
	uniforms.areaLight.points[0] = areaLightVertices[0].position;
	uniforms.areaLight.points[1] = areaLightVertices[1].position;
	uniforms.areaLight.points[2] = areaLightVertices[4].position;
	uniforms.areaLight.points[3] = areaLightVertices[5].position;
	uniforms.areaLight.color = LIGHT_COLOR;
	uniforms.areaLight.intensity = intensity;
	uniforms.areaLight.twoSided = twoSided;
	uniforms.albedoRoughness = vec4(Color::SlateGray, roughness);
	uniforms.lightColor = LIGHT_COLOR;
	uniforms.lightColor2 = LIGHT_COLOR2;
	configureMockupData();

	while (true)
	{
		fps_log_tick();
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (handle_events())
			break;

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shaderLTC);
		mat4 model(1.0f);
		uniforms.model = model;
		uniforms.normalMatrix = mat3(model);
		uniforms.view = camera.GetViewMatrix();
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);
		uniforms.viewPosition = camera.Position;
		uniforms.areaLightTranslate = areaLightTranslate;
		uniforms.areaLight.intensity = intensity;
		uniforms.areaLight.twoSided = twoSided;
		uniforms.albedoRoughness.w = roughness;
		renderPlane();

		glUseProgram(shaderLight);
		uniforms.lightColor = LIGHT_COLOR;
		uniforms.model = translate(mat4(1.0f), areaLightTranslate);
		renderAreaLight();
		uniforms.lightColor = LIGHT_COLOR2;
		uniforms.model = translate(mat4(1.0f), areaLightTranslate + LIGHT2_OFFSET);
		renderAreaLight();

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

static vec3 IntegrateEdgeVec(vec3 v1, vec3 v2)
{
	float x = dot(v1, v2);
	float y = fabs(x);
	float a = 0.8543985f + (0.4965155f + 0.0145206f * y) * y;
	float b = 3.4175940f + (4.1616724f + y) * y;
	float v = a / b;
	float theta_sintheta = (x > 0.0f) ? v : 0.5f / sqrt(max(1.0f - x * x, 1e-7f)) - v;
	return cross(v1, v2) * theta_sintheta;
}

static vec3 LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, const vec3 points[4], bool twoSided, GLuint ltc2)
{
	vec3 T1 = normalize(V - N * dot(V, N));
	vec3 T2 = cross(N, T1);
	Minv = Minv * transpose(mat3(T1, T2, N));
	vec3 L[4];
	L[0] = Minv * (points[0] - P);
	L[1] = Minv * (points[1] - P);
	L[2] = Minv * (points[2] - P);
	L[3] = Minv * (points[3] - P);
	vec3 dir = points[0] - P;
	vec3 lightNormal = cross(points[1] - points[0], points[3] - points[0]);
	bool behind = (dot(dir, lightNormal) < 0.0f);
	L[0] = normalize(L[0]); L[1] = normalize(L[1]); L[2] = normalize(L[2]); L[3] = normalize(L[3]);
	vec3 vsum = IntegrateEdgeVec(L[0], L[1]) + IntegrateEdgeVec(L[1], L[2]) + IntegrateEdgeVec(L[2], L[3]) + IntegrateEdgeVec(L[3], L[0]);
	float len = length(vsum);
	float z = vsum.z / len;
	if (behind) z = -z;
	vec2 uv = vec2(z * 0.5f + 0.5f, len);
	const float LUT_SIZE = 64.0f;
	uv = uv * ((LUT_SIZE - 1.0f) / LUT_SIZE) + (0.5f / LUT_SIZE);
	float scale = toglm(texture2D(ltc2, uv.x, uv.y)).w;
	float sum = len * scale;
	if (!behind && !twoSided) sum = 0.0f;
	return vec3(sum);
}

void ltc_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 aPos = vec3(((vec4*)vertex_attribs)[0]);
	vec3 aN = vec3(((vec4*)vertex_attribs)[1]);
	vec2 aUv = vec2(((vec4*)vertex_attribs)[2]);
	vec3 worldpos = vec3(u->model * vec4(aPos, 1.0f));
	// Texcoords first: PGL auto-LOD uses vs_output[0..1] as UV.
	*(vec2*)&vs_output[0] = aUv;
	*(vec3*)&vs_output[2] = worldpos;
	*(vec3*)&vs_output[5] = u->normalMatrix * aN;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * vec4(worldpos, 1.0f);
}

void ltc_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec2 texcoord = *(vec2*)&fs_input[0];
	vec3 worldPosition = *(vec3*)&fs_input[2];
	vec3 worldNormal = *(vec3*)&fs_input[5];
	vec3 mDiffuse = vec3(toglm(texture2D(u->diffuse, texcoord.x, texcoord.y)));
	vec3 mSpecular = pow(vec3(0.23f), vec3(2.2f));
	vec3 N = normalize(worldNormal);
	vec3 V = normalize(u->viewPosition - worldPosition);
	float dotNV = clamp(dot(N, V), 0.0f, 1.0f);
	const float LUT_SIZE = 64.0f;
	vec2 uv = vec2(u->albedoRoughness.w, sqrt(max(1.0f - dotNV, 0.0f)));
	uv = uv * ((LUT_SIZE - 1.0f) / LUT_SIZE) + (0.5f / LUT_SIZE);
	vec4 t1 = toglm(texture2D(u->LTC1, uv.x, uv.y));
	vec4 t2 = toglm(texture2D(u->LTC2, uv.x, uv.y));
	mat3 Minv = mat3(vec3(t1.x, 0, t1.y), vec3(0, 1, 0), vec3(t1.z, 0, t1.w));
	vec3 fresnel = mSpecular * t2.x + (1.0f - mSpecular) * t2.y;
	vec3 result(0.0f);
	vec3 pts[4];
	for (int i = 0; i < 4; i++)
		pts[i] = u->areaLight.points[i] + u->areaLightTranslate;
	vec3 diffuse = LTC_Evaluate(N, V, worldPosition, mat3(1.0f), pts, u->areaLight.twoSided != 0, u->LTC2);
	vec3 specular = LTC_Evaluate(N, V, worldPosition, Minv, pts, u->areaLight.twoSided != 0, u->LTC2);
	specular *= fresnel;
	result += u->areaLight.color * u->areaLight.intensity * (specular + mDiffuse * diffuse);
	vec3 pts2[4];
	for (int i = 0; i < 4; i++)
		pts2[i] = u->areaLight.points[i] + u->areaLightTranslate + LIGHT2_OFFSET;
	diffuse = LTC_Evaluate(N, V, worldPosition, mat3(1.0f), pts2, u->areaLight.twoSided != 0, u->LTC2);
	specular = LTC_Evaluate(N, V, worldPosition, Minv, pts2, u->areaLight.twoSided != 0, u->LTC2);
	specular *= fresnel;
	result += u->lightColor2 * u->areaLight.intensity * (specular + mDiffuse * diffuse);
	result = pow(result, vec3(1.0f / 2.2f));
	*(vec4*)&builtins->gl_FragColor = vec4(result, 1.0f);
}

void plane_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	(void)vs_output;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 aPos = vec3(((vec4*)vertex_attribs)[0]);
	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * vec4(aPos, 1.0f);
}

void plane_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)fs_input;
	My_Uniforms* u = (My_Uniforms*)uniforms;
	*(vec4*)&builtins->gl_FragColor = vec4(u->lightColor, 1.0f);
}

void configureMockupData()
{
	glGenVertexArrays(1, &planeVAO);
	glGenBuffers(1, &planeVBO);
	glBindVertexArray(planeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
	glEnableVertexAttribArray(1);
	pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 3 * sizeof(float));
	glEnableVertexAttribArray(2);
	pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 6 * sizeof(float));

	glGenVertexArrays(1, &areaLightVAO);
	glGenBuffers(1, &areaLightVBO);
	glBindVertexArray(areaLightVAO);
	glBindBuffer(GL_ARRAY_BUFFER, areaLightVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(areaLightVertices), areaLightVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
	glEnableVertexAttribArray(1);
	pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 3 * sizeof(float));
	glEnableVertexAttribArray(2);
	pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 6 * sizeof(float));
}

void renderPlane() { glBindVertexArray(planeVAO); glDrawArrays(GL_TRIANGLES, 0, 6); }
void renderAreaLight() { glBindVertexArray(areaLightVAO); glDrawArrays(GL_TRIANGLES, 0, 6); }

GLuint loadMTexture()
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_FLOAT, LTC1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return texture;
}

GLuint loadLUTTexture()
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_FLOAT, LTC2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return texture;
}

unsigned int loadTexture(const char *path, bool gammaCorrection)
{
	(void)gammaCorrection;
	unsigned int textureID;
	glGenTextures(1, &textureID);
	int width, height, nrComponents;
	unsigned char *data = stbi_load(path, &width, &height, &nrComponents, STBI_rgb_alpha);
	if (data) {
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		stbi_image_free(data);
	} else {
		std::cout << "Texture failed to load at path: " << path << std::endl;
	}
	return textureID;
}

bool handle_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT: return true;
		case SDL_KEYDOWN:
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) return true;
			if (event.key.keysym.scancode == SDL_SCANCODE_B) twoSided = !twoSided;
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				scr_width = event.window.data1; scr_height = event.window.data2;
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
	float dt = deltaTime;
	// original 0.01 / 0.025 / 0.01 per frame at ~60 Hz
	if (state[SDL_SCANCODE_R]) roughness = clamp(roughness + (state[SDL_SCANCODE_LSHIFT] ? 0.6f : -0.6f) * dt, 0.0f, 1.0f);
	if (state[SDL_SCANCODE_I]) intensity = clamp(intensity + (state[SDL_SCANCODE_LSHIFT] ? 1.5f : -1.5f) * dt, 0.0f, 10.0f);
	if (state[SDL_SCANCODE_LEFT]) areaLightTranslate.z += 0.6f * dt;
	if (state[SDL_SCANCODE_RIGHT]) areaLightTranslate.z -= 0.6f * dt;
	if (state[SDL_SCANCODE_UP]) areaLightTranslate.y += 0.6f * dt;
	if (state[SDL_SCANCODE_DOWN]) areaLightTranslate.y -= 0.6f * dt;
	return false;
}

void setup_context()
{
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO)) { std::cout << "SDL_Init error: " << SDL_GetError() << "\n"; exit(0); }
	window = SDL_CreateWindow("LearnPortablGL", 100, 100, scr_width, scr_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if (!window) { std::cerr << "Failed to create window\n"; SDL_Quit(); exit(0); }
	ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, scr_width, scr_height);
	if (!init_glContext(&the_Context, &bbufpix, scr_width, scr_height)) { puts("Failed to initialize glContext"); exit(0); }
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
