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

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <fps_log.h>

using namespace glm;

struct My_Uniforms
{
	mat4 model;
	mat4 view;
	mat4 projection;

	vec3 lightPos;
	vec3 viewPos;

	GLuint diffuseMap;
	GLuint normalMap;
	GLuint depthMap;
	float heightScale;
};

void setup_context();
void cleanup();
bool handle_events();
unsigned int loadTexture(const char *path);
void renderQuad();

void shader_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms);
void shader_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms);
vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir, My_Uniforms* u);

vec4 toglm(pgl_vec4 v);

unsigned int scr_width = 640;
unsigned int scr_height = 480;
float heightScale = 0.1f;

Camera camera(vec3(0.0f, 0.0f, 3.0f));

float deltaTime = 0.0f;
float lastFrame = 0.0f;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;

pix_t* bbufpix;

glContext the_Context;

My_Uniforms uniforms;

unsigned int quadVAO = 0;
unsigned int quadVBO;

int main()
{
	setup_context();

	SDL_SetRelativeMouseMode(SDL_TRUE);

	glEnable(GL_DEPTH_TEST);

	// PGL mip LOD uses the first even-aligned non-FLAT pair as UVs; original GLSL had FragPos first.
	GLenum smooth[] = { PGL_SMOOTH2, PGL_SMOOTH3, PGL_SMOOTH3, PGL_SMOOTH3, PGL_SMOOTH3 };
	GLuint shader = pglCreateProgram(shader_vs, shader_fs, 14, smooth, GL_TRUE);
	glUseProgram(shader);
	pglSetUniform(&uniforms);

	unsigned int diffuseMap = loadTexture(FileSystem::getPath("resources/textures/bricks2.jpg").c_str());
	unsigned int normalMap = loadTexture(FileSystem::getPath("resources/textures/bricks2_normal.jpg").c_str());
	unsigned int heightMap = loadTexture(FileSystem::getPath("resources/textures/bricks2_disp.jpg").c_str());
	/* unsigned int diffuseMap = loadTexture(FileSystem::getPath("resources/textures/toy_box_diffuse.png").c_str());
	unsigned int normalMap = loadTexture(FileSystem::getPath("resources/textures/toy_box_normal.png").c_str());
	unsigned int heightMap = loadTexture(FileSystem::getPath("resources/textures/toy_box_disp.png").c_str());*/

	uniforms.diffuseMap = diffuseMap;
	uniforms.normalMap = normalMap;
	uniforms.depthMap = heightMap;

	vec3 lightPos(0.5f, 1.0f, 0.3f);
	uniforms.lightPos = lightPos;

	while (true)
	{
		fps_log_tick();

		int currentFrame = SDL_GetTicks();
		deltaTime = (currentFrame - lastFrame)/1000.0f;
		lastFrame = currentFrame;

		if (handle_events())
			break;

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shader);
		uniforms.projection = perspective(radians(camera.Zoom), (float)scr_width / (float)scr_height, 0.1f, 100.0f);
		uniforms.view = camera.GetViewMatrix();
		mat4 model = mat4(1.0f);
		model = rotate(model, radians((SDL_GetTicks() / 1000.0f) * -10.0f), normalize(vec3(1.0, 0.0, 1.0)));
		uniforms.model = model;
		uniforms.viewPos = camera.Position;
		uniforms.lightPos = lightPos;
		uniforms.heightScale = heightScale;
		renderQuad();

		model = mat4(1.0f);
		model = translate(model, lightPos);
		model = scale(model, vec3(0.1f));
		uniforms.model = model;
		renderQuad();

		SDL_UpdateTexture(tex, NULL, bbufpix, scr_width * sizeof(pix_t));
		SDL_RenderCopy(ren, tex, NULL, NULL);
		SDL_RenderPresent(ren);
	}

	glDeleteVertexArrays(1, &quadVAO);
	glDeleteBuffers(1, &quadVBO);

	cleanup();
	return 0;
}

void renderQuad()
{
	if (quadVAO == 0)
	{
		vec3 pos1(-1.0f,  1.0f, 0.0f);
		vec3 pos2(-1.0f, -1.0f, 0.0f);
		vec3 pos3( 1.0f, -1.0f, 0.0f);
		vec3 pos4( 1.0f,  1.0f, 0.0f);
		vec2 uv1(0.0f, 1.0f);
		vec2 uv2(0.0f, 0.0f);
		vec2 uv3(1.0f, 0.0f);
		vec2 uv4(1.0f, 1.0f);
		vec3 nm(0.0f, 0.0f, 1.0f);

		vec3 tangent1, bitangent1;
		vec3 tangent2, bitangent2;
		vec3 edge1 = pos2 - pos1;
		vec3 edge2 = pos3 - pos1;
		vec2 deltaUV1 = uv2 - uv1;
		vec2 deltaUV2 = uv3 - uv1;

		float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

		tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
		tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
		tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
		tangent1 = normalize(tangent1);

		bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
		bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
		bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
		bitangent1 = normalize(bitangent1);

		edge1 = pos3 - pos1;
		edge2 = pos4 - pos1;
		deltaUV1 = uv3 - uv1;
		deltaUV2 = uv4 - uv1;

		f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

		tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
		tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
		tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
		tangent2 = normalize(tangent2);

		bitangent2.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
		bitangent2.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
		bitangent2.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
		bitangent2 = normalize(bitangent2);

		float quadVertices[] = {
			pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
			pos2.x, pos2.y, pos2.z, nm.x, nm.y, nm.z, uv2.x, uv2.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
			pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,

			pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
			pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
			pos4.x, pos4.y, pos4.z, nm.x, nm.y, nm.z, uv4.x, uv4.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z
		};
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), 0);
		glEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), 3 * sizeof(float));
		glEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), 6 * sizeof(float));
		glEnableVertexAttribArray(3);
		pglVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), 8 * sizeof(float));
		glEnableVertexAttribArray(4);
		pglVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), 11 * sizeof(float));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
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

	float dt = deltaTime;
	// original 0.0005/frame at ~60 Hz
	if (state[SDL_SCANCODE_Q]) {
		if (heightScale > 0.0f) {
			heightScale -= 0.03f * dt;
			if (heightScale < 0.0f)
				heightScale = 0.0f;
			std::cout << "heightScale: " << heightScale << std::endl;
		}
	} else if (state[SDL_SCANCODE_E]) {
		if (heightScale < 1.0f) {
			heightScale += 0.03f * dt;
			if (heightScale > 1.0f)
				heightScale = 1.0f;
			std::cout << "heightScale: " << heightScale << std::endl;
		}
	}

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

void shader_vs(float* vs_output, pgl_vec4* vertex_attribs, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	vec4 aPos = ((vec4*)vertex_attribs)[0];
	vec3 aNormal = vec3(((vec4*)vertex_attribs)[1]);
	vec2 aTexCoords = vec2(((vec4*)vertex_attribs)[2]);
	vec3 aTangent = vec3(((vec4*)vertex_attribs)[3]);

	vec3 FragPos = vec3(u->model * aPos);
	// PGL mip LOD uses the first even-aligned non-FLAT pair as UVs; original GLSL had FragPos first.
	*(vec2*)&vs_output[0] = aTexCoords;
	*(vec3*)&vs_output[2] = FragPos;

	mat3 normalMatrix = transpose(inverse(mat3(u->model)));
	vec3 T = normalize(normalMatrix * aTangent);
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T);

	mat3 TBN = transpose(mat3(T, B, N));
	*(vec3*)&vs_output[5] = TBN * u->lightPos;
	*(vec3*)&vs_output[8] = TBN * u->viewPos;
	*(vec3*)&vs_output[11] = TBN * FragPos;

	*(vec4*)&builtins->gl_Position = u->projection * u->view * u->model * aPos;
}

vec4 toglm(pgl_vec4 v)
{
	return vec4(v.x, v.y, v.z, v.w);
}

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir, My_Uniforms* u)
{
	const float minLayers = 8;
	const float maxLayers = 32;
	float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0f, 0.0f, 1.0f), viewDir)));
	float layerDepth = 1.0f / numLayers;
	float currentLayerDepth = 0.0f;
	vec2 P = vec2(viewDir) / viewDir.z * u->heightScale;
	vec2 deltaTexCoords = P / numLayers;

	vec2 currentTexCoords = texCoords;
	float currentDepthMapValue = toglm(texture2D(u->depthMap, currentTexCoords.x, currentTexCoords.y)).r;

	while (currentLayerDepth < currentDepthMapValue)
	{
		currentTexCoords -= deltaTexCoords;
		currentDepthMapValue = toglm(texture2D(u->depthMap, currentTexCoords.x, currentTexCoords.y)).r;
		currentLayerDepth += layerDepth;
	}

	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = toglm(texture2D(u->depthMap, prevTexCoords.x, prevTexCoords.y)).r - currentLayerDepth + layerDepth;

	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0f - weight);

	return finalTexCoords;
}

void shader_fs(float* fs_input, Shader_Builtins* builtins, void* uniforms)
{
	My_Uniforms* u = (My_Uniforms*)uniforms;

	vec2 TexCoords = *(vec2*)&fs_input[0];
	vec3 TangentLightPos = *(vec3*)&fs_input[5];
	vec3 TangentViewPos = *(vec3*)&fs_input[8];
	vec3 TangentFragPos = *(vec3*)&fs_input[11];

	vec3 viewDir = normalize(TangentViewPos - TangentFragPos);
	vec2 texCoords = ParallaxMapping(TexCoords, viewDir, u);
	if (texCoords.x > 1.0f || texCoords.y > 1.0f || texCoords.x < 0.0f || texCoords.y < 0.0f) {
		builtins->discard = GL_TRUE;
		return;
	}

	vec3 normal = vec3(toglm(texture2D(u->normalMap, texCoords.x, texCoords.y)));
	normal = normalize(normal * 2.0f - 1.0f);

	vec3 color = vec3(toglm(texture2D(u->diffuseMap, texCoords.x, texCoords.y)));
	vec3 ambient = 0.1f * color;
	vec3 lightDir = normalize(TangentLightPos - TangentFragPos);
	float diff = max(dot(lightDir, normal), 0.0f);
	vec3 diffuse = diff * color;
	vec3 reflectDir = reflect(-lightDir, normal);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);

	vec3 specular = vec3(0.2f) * spec;
	*(vec4*)&builtins->gl_FragColor = vec4(ambient + diffuse + specular, 1.0f);
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
		GLenum format = GL_RGBA;
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
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
