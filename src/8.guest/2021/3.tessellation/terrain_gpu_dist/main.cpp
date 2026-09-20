
// This program draws a very large mesh in a single call and the default
// PGL_MAX_VERTICES is not large enough.
#define GL_MAX_VERTEX_ATTRIBS 4
#define PGL_MAX_VERTICES 5000000

#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/camera.h>

#include <iostream>
#include <vector>
#include <cmath>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

using namespace glm;

struct My_Uniforms { mat4 model, view, projection; };

void setup_context();
void cleanup();
bool handle_events();
void height_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void height_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
void rebuild_patches();

unsigned int scr_width = 640, scr_height = 480;
Camera camera(vec3(67.0f, 627.5f, 169.9f), vec3(0.0f, 1.0f, 0.0f), -128.1f, -42.4f);
float deltaTime = 0.0f, lastFrame = 0.0f;
int useWireframe = 0;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;
My_Uniforms uniforms;

unsigned char* hm_data = NULL;
int hm_w, hm_h, hm_n;
unsigned int terrainVAO, terrainVBO, terrainIBO;
std::vector<float> mesh_verts;
std::vector<unsigned int> mesh_idx;

const int PATCHES = 20;
const int MIN_TESS_LEVEL = 4;
const int MAX_TESS_LEVEL = 64;
const float MIN_DISTANCE = 20.0f;
const float MAX_DISTANCE = 800.0f;

static float sample_height(float u, float v)
{
	if (!hm_data || hm_w < 1 || hm_h < 1)
		return 0.0f;
	float fx = clamp(u, 0.0f, 1.0f) * (float)(hm_w - 1);
	float fy = clamp(v, 0.0f, 1.0f) * (float)(hm_h - 1);
	int x0 = (int)floorf(fx);
	int y0 = (int)floorf(fy);
	int x1 = x0 + 1 < hm_w ? x0 + 1 : x0;
	int y1 = y0 + 1 < hm_h ? y0 + 1 : y0;
	float tx = fx - (float)x0;
	float ty = fy - (float)y0;
	int ch = hm_n >= 2 ? 1 : 0; // TES uses texture().y
	auto pix = [&](int x, int y) -> float {
		return (float)hm_data[(y * hm_w + x) * hm_n + ch] / 255.0f;
	};
	float h00 = pix(x0, y0), h10 = pix(x1, y0), h01 = pix(x0, y1), h11 = pix(x1, y1);
	float h0 = h00 + (h10 - h00) * tx;
	float h1 = h01 + (h11 - h01) * tx;
	return (h0 + (h1 - h0) * ty) * 64.0f - 16.0f;
}

static vec3 patch_pos(float u, float v)
{
	return vec3(-hm_w / 2.0f + hm_w * u, sample_height(u, v), -hm_h / 2.0f + hm_h * v);
}

static int tess_level(const vec4& e0, const vec4& e1)
{
	float d0 = clamp((fabs(e0.z) - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0f, 1.0f);
	float d1 = clamp((fabs(e1.z) - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE), 0.0f, 1.0f);
	int n = (int)(mix((float)MAX_TESS_LEVEL, (float)MIN_TESS_LEVEL, min(d0, d1)) + 0.5f);
	if (n < MIN_TESS_LEVEL) n = MIN_TESS_LEVEL;
	if (n > MAX_TESS_LEVEL) n = MAX_TESS_LEVEL;
	return n;
}

static unsigned add_uv(float u, float v)
{
	vec3 p = patch_pos(u, v);
	mesh_verts.push_back(p.x);
	mesh_verts.push_back(p.y);
	mesh_verts.push_back(p.z);
	return (unsigned)(mesh_verts.size() / 3 - 1);
}

static void tri(unsigned a, unsigned b, unsigned c)
{
	mesh_idx.push_back(a);
	mesh_idx.push_back(b);
	mesh_idx.push_back(c);
}

static void quad(unsigned a, unsigned b, unsigned c, unsigned d)
{
	tri(a, b, c);
	tri(a, c, d);
}

struct ChainPt { unsigned i; float t; };

// inner_plus_v: inner chain has larger patch-v than the outer (bottom of the quad).
static void stitch(const std::vector<ChainPt>& inner, const std::vector<ChainPt>& outer, bool inner_plus_v)
{
	size_t i = 0, j = 0;
	while (i + 1 < inner.size() || j + 1 < outer.size()) {
		bool adv_i = (j + 1 >= outer.size()) ||
			(i + 1 < inner.size() && inner[i + 1].t <= outer[j + 1].t);
		if (adv_i) {
			if (inner_plus_v)
				tri(inner[i + 1].i, inner[i].i, outer[j].i);
			else
				tri(inner[i].i, inner[i + 1].i, outer[j].i);
			i++;
		} else {
			if (inner_plus_v)
				tri(outer[j].i, outer[j + 1].i, inner[i].i);
			else
				tri(outer[j + 1].i, outer[j].i, inner[i].i);
			j++;
		}
	}
}

static void tessellate_quad(float u0, float v0, float u1, float v1, int T0, int T1, int T2, int T3)
{
	int iu = T1 > T3 ? T1 : T3;
	int iv = T0 > T2 ? T0 : T2;
	if (iu < 2) iu = 2;
	if (iv < 2) iv = 2;

	auto U = [&](float s) { return u0 + (u1 - u0) * s; };
	auto V = [&](float s) { return v0 + (v1 - v0) * s; };

	// Uniform grid when all four outers match the inners (common; looks like GL).
	if (T0 == iv && T2 == iv && T1 == iu && T3 == iu) {
		std::vector<unsigned> grid((size_t)(iv + 1) * (size_t)(iu + 1));
		for (int j = 0; j <= iv; j++)
			for (int i = 0; i <= iu; i++)
				grid[(size_t)j * (iu + 1) + i] = add_uv(U((float)i / (float)iu), V((float)j / (float)iv));
		for (int j = 0; j < iv; j++)
			for (int i = 0; i < iu; i++) {
				unsigned a = grid[(size_t)j * (iu + 1) + i];
				unsigned b = grid[(size_t)j * (iu + 1) + i + 1];
				unsigned c = grid[(size_t)(j + 1) * (iu + 1) + i + 1];
				unsigned d = grid[(size_t)(j + 1) * (iu + 1) + i];
				quad(a, b, c, d);
			}
		return;
	}

	unsigned c00 = add_uv(U(0.0f), V(0.0f));
	unsigned c10 = add_uv(U(1.0f), V(0.0f));
	unsigned c11 = add_uv(U(1.0f), V(1.0f));
	unsigned c01 = add_uv(U(0.0f), V(1.0f));

	std::vector<unsigned> eBot((size_t)T1 + 1), eRgt((size_t)T2 + 1), eTop((size_t)T3 + 1), eLft((size_t)T0 + 1);
	eBot[0] = c00;
	eBot[(size_t)T1] = c10;
	for (int k = 1; k < T1; k++)
		eBot[(size_t)k] = add_uv(U((float)k / (float)T1), V(0.0f));
	eRgt[0] = c10;
	eRgt[(size_t)T2] = c11;
	for (int k = 1; k < T2; k++)
		eRgt[(size_t)k] = add_uv(U(1.0f), V((float)k / (float)T2));
	eTop[0] = c01;
	eTop[(size_t)T3] = c11;
	for (int k = 1; k < T3; k++)
		eTop[(size_t)k] = add_uv(U((float)k / (float)T3), V(1.0f));
	eLft[0] = c00;
	eLft[(size_t)T0] = c01;
	for (int k = 1; k < T0; k++)
		eLft[(size_t)k] = add_uv(U(0.0f), V((float)k / (float)T0));

	std::vector<unsigned> inn((size_t)(iv + 1) * (size_t)(iu + 1), ~0u);
	auto at = [&](int i, int j) -> unsigned& { return inn[(size_t)j * (iu + 1) + i]; };
	for (int j = 1; j < iv; j++)
		for (int i = 1; i < iu; i++)
			at(i, j) = add_uv(U((float)i / (float)iu), V((float)j / (float)iv));
	for (int j = 1; j < iv - 1; j++)
		for (int i = 1; i < iu - 1; i++)
			quad(at(i, j), at(i + 1, j), at(i + 1, j + 1), at(i, j + 1));

	std::vector<ChainPt> inner, outer;

	inner.clear();
	outer.clear();
	for (int i = 1; i < iu; i++)
		inner.push_back({ at(i, 1), (float)i / (float)iu });
	for (int k = 0; k <= T1; k++)
		outer.push_back({ eBot[(size_t)k], (float)k / (float)T1 });
	stitch(inner, outer, true);

	inner.clear();
	outer.clear();
	for (int i = 1; i < iu; i++)
		inner.push_back({ at(i, iv - 1), (float)i / (float)iu });
	for (int k = 0; k <= T3; k++)
		outer.push_back({ eTop[(size_t)k], (float)k / (float)T3 });
	stitch(inner, outer, false);

	inner.clear();
	outer.clear();
	for (int j = 1; j < iv; j++)
		inner.push_back({ at(1, j), (float)j / (float)iv });
	for (int k = 1; k < T0; k++)
		outer.push_back({ eLft[(size_t)k], (float)k / (float)T0 });
	stitch(inner, outer, false);

	inner.clear();
	outer.clear();
	for (int j = 1; j < iv; j++)
		inner.push_back({ at(iu - 1, j), (float)j / (float)iv });
	for (int k = 1; k < T2; k++)
		outer.push_back({ eRgt[(size_t)k], (float)k / (float)T2 });
	stitch(inner, outer, true);

	// Side stitches skip the four corners; close them explicitly.
	tri(c00, at(1, 1), eLft[1]);
	tri(c10, eRgt[1], at(iu - 1, 1));
	tri(c11, at(iu - 1, iv - 1), eRgt[(size_t)T2 - 1]);
	tri(c01, eLft[(size_t)T0 - 1], at(1, iv - 1));
}

void rebuild_patches()
{
	mesh_verts.clear();
	mesh_idx.clear();
	mat4 view = camera.GetViewMatrix();
	float du = 1.0f / (float)PATCHES;
	float dv = 1.0f / (float)PATCHES;

	for (int i = 0; i < PATCHES; i++) {
		for (int j = 0; j < PATCHES; j++) {
			float u0 = i * du, u1 = (i + 1) * du;
			float v0 = j * dv, v1 = (j + 1) * dv;
			// Control points at y=0, same as the original VS (distance before displace).
			vec4 p00 = view * vec4(-hm_w / 2.0f + hm_w * u0, 0.0f, -hm_h / 2.0f + hm_h * v0, 1.0f);
			vec4 p01 = view * vec4(-hm_w / 2.0f + hm_w * u1, 0.0f, -hm_h / 2.0f + hm_h * v0, 1.0f);
			vec4 p10 = view * vec4(-hm_w / 2.0f + hm_w * u0, 0.0f, -hm_h / 2.0f + hm_h * v1, 1.0f);
			vec4 p11 = view * vec4(-hm_w / 2.0f + hm_w * u1, 0.0f, -hm_h / 2.0f + hm_h * v1, 1.0f);
			int T0 = tess_level(p10, p00);
			int T1 = tess_level(p00, p01);
			int T2 = tess_level(p01, p11);
			int T3 = tess_level(p11, p10);
			tessellate_quad(u0, v0, u1, v1, T0, T1, T2, T3);
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
	glBufferData(GL_ARRAY_BUFFER, mesh_verts.size() * sizeof(float),
		mesh_verts.empty() ? NULL : mesh_verts.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh_idx.size() * sizeof(unsigned int),
		mesh_idx.empty() ? NULL : mesh_idx.data(), GL_DYNAMIC_DRAW);
}

int main()
{
	setup_context();
	SDL_SetRelativeMouseMode(SDL_TRUE);
	glEnable(GL_DEPTH_TEST);
	camera.MovementSpeed = 50.0f;

	GLenum smooth1[] = { PGL_SMOOTH };
	GLuint shader = pglCreateProgram(height_vs, height_fs, 1, smooth1, GL_FALSE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	hm_data = stbi_load(FileSystem::getPath("resources/textures/heightmaps/iceland_heightmap.png").c_str(), &hm_w, &hm_h, &hm_n, 0);
	if (!hm_data) {
		std::cout << "Failed to load texture" << std::endl;
		return 1;
	}
	std::cout << "Loaded heightmap of size " << hm_h << " x " << hm_w << std::endl;
	std::cout << "No tessellation shaders; " << PATCHES << "x" << PATCHES
		<< " patches, integer quad tess " << MIN_TESS_LEVEL << "-" << MAX_TESS_LEVEL
		<< " (TCS outer/inner levels, regular grid + edge stitch)." << std::endl;

	glGenVertexArrays(1, &terrainVAO);
	glBindVertexArray(terrainVAO);
	glGenBuffers(1, &terrainVBO);
	glBindBuffer(GL_ARRAY_BUFFER, terrainVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
	glGenBuffers(1, &terrainIBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terrainIBO);

	while (true)
	{
		fps_log_tick();
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;
		if (handle_events())
			break;

		rebuild_patches();

		glPolygonMode(GL_FRONT_AND_BACK, useWireframe ? GL_LINE : GL_FILL);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(shader);
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100000.0f);
		uniforms.view = camera.GetViewMatrix();
		uniforms.model = mat4(1.0f);
		glBindVertexArray(terrainVAO);
		glDrawElements(GL_TRIANGLES, (GLsizei)mesh_idx.size(), GL_UNSIGNED_INT, 0);

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	stbi_image_free(hm_data);
	cleanup();
	return 0;
}

void height_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;
	vec3 aPos = vec3(((vec4*)vertex_attribs)[0]);
	vs_output[0] = aPos.y;
	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * vec4(aPos, 1.0f);
}

void height_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	(void)uniforms;
	float h = (fs_input[0] + 16.0f) / 64.0f;
	*(vec4*)&builtins->gl_FragColor = vec4(h, h, h, 1.0f);
}

bool handle_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT: return true;
		case SDL_KEYDOWN:
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) return true;
			if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) useWireframe = !useWireframe;
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
	float moveDt = deltaTime * (state[SDL_SCANCODE_LSHIFT] ? 3.0f : 1.0f);
	if (state[SDL_SCANCODE_W]) camera.ProcessKeyboard(FORWARD, moveDt);
	if (state[SDL_SCANCODE_S]) camera.ProcessKeyboard(BACKWARD, moveDt);
	if (state[SDL_SCANCODE_A]) camera.ProcessKeyboard(LEFT, moveDt);
	if (state[SDL_SCANCODE_D]) camera.ProcessKeyboard(RIGHT, moveDt);
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
