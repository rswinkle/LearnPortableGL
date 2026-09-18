/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#define PGL_PREFIX_TYPES
#define PORTABLEGL_IMPLEMENTATION
#include <portablegl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "game.h"
#include "resource_manager.h"
#include "game_uniforms.h"

#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

const unsigned int SCREEN_WIDTH = 800;
const unsigned int SCREEN_HEIGHT = 600;

GameUniforms uniforms;

SDL_Window* window;
SDL_Renderer* ren;
SDL_Texture* tex;
pix_t* bbufpix;
glContext the_Context;

static void setup_context();
static void cleanup();
static bool handle_events(Game &game);

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	setup_context();

	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	{
		Game Breakout(SCREEN_WIDTH, SCREEN_HEIGHT);
		Breakout.Init();

		float deltaTime = 0.0f;
		float lastFrame = 0.0f;

		while (true)
		{
			float currentFrame = SDL_GetTicks() / 1000.0f;
			deltaTime = currentFrame - lastFrame;
			lastFrame = currentFrame;

			if (handle_events(Breakout))
				break;

			Breakout.ProcessInput(deltaTime);
			Breakout.Update(deltaTime);

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			Breakout.Render(currentFrame);

			SDL_UpdateTexture(tex, NULL, bbufpix, SCREEN_WIDTH * sizeof(pix_t));
			SDL_RenderCopy(ren, tex, NULL, NULL);
			SDL_RenderPresent(ren);
		}
	}

	ResourceManager::Clear();
	cleanup();
	return 0;
}

static bool handle_events(Game &game)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			return true;
		case SDL_KEYDOWN:
			if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
				return true;
			if (event.key.keysym.scancode < 1024)
			{
				game.Keys[event.key.keysym.scancode] = true;
			}
			break;
		case SDL_KEYUP:
			if (event.key.keysym.scancode < 1024)
			{
				game.Keys[event.key.keysym.scancode] = false;
				game.KeysProcessed[event.key.keysym.scancode] = false;
			}
			break;
		}
	}
	return false;
}

static void setup_context()
{
	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO)) {
		std::cout << "SDL_Init error: " << SDL_GetError() << "\n";
		exit(0);
	}
	window = SDL_CreateWindow("Breakout", 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (!window) {
		std::cerr << "Failed to create window\n";
		SDL_Quit();
		exit(0);
	}
	ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
	if (!init_glContext(&the_Context, &bbufpix, SCREEN_WIDTH, SCREEN_HEIGHT)) {
		puts("Failed to initialize glContext");
		exit(0);
	}
	set_glContext(&the_Context);
}

static void cleanup()
{
	free_glContext(&the_Context);
	SDL_DestroyTexture(tex);
	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
