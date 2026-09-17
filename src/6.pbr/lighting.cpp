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
#include <vector>
#include <cmath>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

using namespace glm;

struct My_Uniforms
{
	mat4 model;
	mat4 view;
	mat4 projection;
	mat3 normalMatrix;

	vec3 albedo;
	float metallic;
	float roughness;
	float ao;

	vec3 lightPositions[4];
	vec3 lightColors[4];
	vec3 camPos;
};

void setup_context();
void cleanup();
bool handle_events();
void renderSphere();

void pbr_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void pbr_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);

vec4 toglm(pgl_vec4 v);

const float PI = 3.14159265359f;

unsigned int scr_width = 640;
unsigned int scr_height = 480;

Camera camera(vec3(0.0f, 0.0f, 3.0f));

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

	glEnable(GL_DEPTH_TEST);

	// PGL mip LOD uses the first even-aligned non-FLAT pair as UVs.
	GLenum smooth[] = { PGL_SMOOTH2, PGL_SMOOTH3, PGL_SMOOTH3 };
	GLuint shader = pglCreateProgram(pbr_vs, pbr_fs, 8, smooth, GL_FALSE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	uniforms.albedo = vec3(0.5f, 0.0f, 0.0f);
	uniforms.ao = 1.0f;

	vec3 lightPositions[] = {
		vec3(-10.0f,  10.0f, 10.0f),
		vec3( 10.0f,  10.0f, 10.0f),
		vec3(-10.0f, -10.0f, 10.0f),
		vec3( 10.0f, -10.0f, 10.0f),
	};
	vec3 lightColors[] = {
		vec3(300.0f, 300.0f, 300.0f),
		vec3(300.0f, 300.0f, 300.0f),
		vec3(300.0f, 300.0f, 300.0f),
		vec3(300.0f, 300.0f, 300.0f)
	};
	for (int i = 0; i < 4; i++) {
		uniforms.lightPositions[i] = lightPositions[i];
		uniforms.lightColors[i] = lightColors[i];
	}

	int nrRows = 7;
	int nrColumns = 7;
	float spacing = 2.5f;

	uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);

	while (true)
	{
		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame) / 1000.0f;
		lastFrame = currentFrame;

		if (handle_events())
			break;

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shader);
		uniforms.view = camera.GetViewMatrix();
		uniforms.camPos = camera.Position;
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);

		for (int row = 0; row < nrRows; ++row)
		{
			uniforms.metallic = (float)row / (float)nrRows;
			for (int col = 0; col < nrColumns; ++col)
			{
				uniforms.roughness = clamp((float)col / (float)nrColumns, 0.05f, 1.0f);

				mat4 model = mat4(1.0f);
				model = translate(model, vec3(
					(col - (nrColumns / 2)) * spacing,
					(row - (nrRows / 2)) * spacing,
					0.0f
				));
				uniforms.model = model;
				uniforms.normalMatrix = transpose(inverse(mat3(model)));
				renderSphere();
			}
		}

		for (int i = 0; i < 4; ++i)
		{
			mat4 model = mat4(1.0f);
			model = translate(model, lightPositions[i]);
			model = scale(model, vec3(0.5f));
			uniforms.model = model;
			uniforms.normalMatrix = transpose(inverse(mat3(model)));
			renderSphere();
		}

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	cleanup();
	return 0;
}

unsigned int sphereVAO = 0;
unsigned int indexCount;
void renderSphere()
{
	if (sphereVAO == 0)
	{
		glGenVertexArrays(1, &sphereVAO);

		unsigned int vbo, ebo;
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);

		std::vector<vec3> positions;
		std::vector<vec2> uv;
		std::vector<vec3> normals;
		std::vector<unsigned int> indices;

#ifndef NDEBUG
		const unsigned int X_SEGMENTS = 32;
		const unsigned int Y_SEGMENTS = 32;
#else
		const unsigned int X_SEGMENTS = 64;
		const unsigned int Y_SEGMENTS = 64;
#endif
		for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
		{
			for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
			{
				float xSegment = (float)x / (float)X_SEGMENTS;
				float ySegment = (float)y / (float)Y_SEGMENTS;
				float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
				float yPos = std::cos(ySegment * PI);
				float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

				positions.push_back(vec3(xPos, yPos, zPos));
				uv.push_back(vec2(xSegment, ySegment));
				normals.push_back(vec3(xPos, yPos, zPos));
			}
		}

		// Original uses GL_TRIANGLE_STRIP; this repo draws triangles.
		for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
		{
			for (unsigned int x = 0; x < X_SEGMENTS; ++x)
			{
				unsigned int i0 = y * (X_SEGMENTS + 1) + x;
				unsigned int i1 = (y + 1) * (X_SEGMENTS + 1) + x;
				unsigned int i2 = y * (X_SEGMENTS + 1) + (x + 1);
				unsigned int i3 = (y + 1) * (X_SEGMENTS + 1) + (x + 1);
				indices.push_back(i0);
				indices.push_back(i1);
				indices.push_back(i2);
				indices.push_back(i2);
				indices.push_back(i1);
				indices.push_back(i3);
			}
		}
		indexCount = static_cast<unsigned int>(indices.size());

		std::vector<float> data;
		for (unsigned int i = 0; i < positions.size(); ++i)
		{
			data.push_back(positions[i].x);
			data.push_back(positions[i].y);
			data.push_back(positions[i].z);
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
			data.push_back(uv[i].x);
			data.push_back(uv[i].y);
		}
		glBindVertexArray(sphereVAO);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
		unsigned int stride = (3 + 3 + 2) * sizeof(float);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, 0);
		glEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, 3 * sizeof(float));
		glEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, 6 * sizeof(float));
	}

	glBindVertexArray(sphereVAO);
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
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

void pbr_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec3 aNormal = vec3(((vec4*)vertex_attribs)[1]);
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[2]);

	vec3 WorldPos = vec3(u->model * aPos);

	*(vec2*)&vs_output[0] = aTexCoords;
	*(vec3*)&vs_output[2] = WorldPos;
	*(vec3*)&vs_output[5] = u->normalMatrix * aNormal;

	*(vec4*)&builtins->gl_Position = u->projection * u->view * vec4(WorldPos, 1.0f);
}

vec4 toglm(pgl_vec4 v)
{
	return vec4(v.x, v.y, v.z, v.w);
}

static float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0f);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
	denom = PI * denom * denom;

	return nom / denom;
}

static float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r * r) / 8.0f;

	float nom = NdotV;
	float denom = NdotV * (1.0f - k) + k;

	return nom / denom;
}

static float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

static vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

void pbr_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	vec3 WorldPos = *(vec3*)&fs_input[2];
	vec3 Normal = *(vec3*)&fs_input[5];

	vec3 N = normalize(Normal);
	vec3 V = normalize(u->camPos - WorldPos);

	vec3 F0 = vec3(0.04f);
	F0 = mix(F0, u->albedo, u->metallic);

	vec3 Lo = vec3(0.0f);
	for (int i = 0; i < 4; ++i)
	{
		vec3 L = normalize(u->lightPositions[i] - WorldPos);
		vec3 H = normalize(V + L);
		float distance = length(u->lightPositions[i] - WorldPos);
		float attenuation = 1.0f / (distance * distance);
		vec3 radiance = u->lightColors[i] * attenuation;

		float NDF = DistributionGGX(N, H, u->roughness);
		float G = GeometrySmith(N, V, L, u->roughness);
		vec3 F = fresnelSchlick(clamp(dot(H, V), 0.0f, 1.0f), F0);

		vec3 numerator = NDF * G * F;
		float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
		vec3 specular = numerator / denominator;

		vec3 kS = F;
		vec3 kD = vec3(1.0f) - kS;
		kD *= 1.0f - u->metallic;

		float NdotL = max(dot(N, L), 0.0f);

		Lo += (kD * u->albedo / PI + specular) * radiance * NdotL;
	}

	vec3 ambient = vec3(0.03f) * u->albedo * u->ao;
	vec3 color = ambient + Lo;

	color = color / (color + vec3(1.0f));
	color = pow(color, vec3(1.0f / 2.2f));

	*(vec4*)&builtins->gl_FragColor = vec4(color, 1.0f);
}

void cleanup()
{
	free_glContext(&the_Context);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
