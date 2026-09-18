/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#include "sound.h"

#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <map>
#include <string>

static Mix_Music *music = nullptr;
static std::string musicPath;
static std::map<std::string, Mix_Chunk*> chunks;

void Sound_Play(const char *path, bool loop)
{
	if (!path || Mix_QuerySpec(nullptr, nullptr, nullptr) == 0)
		return;

	if (loop)
	{
		if (!music || musicPath != path)
		{
			if (music)
			{
				Mix_HaltMusic();
				Mix_FreeMusic(music);
				music = nullptr;
			}
			music = Mix_LoadMUS(path);
			musicPath = path;
			if (!music)
			{
				std::cout << "ERROR::SOUND: Mix_LoadMUS " << path << ": " << Mix_GetError() << std::endl;
				return;
			}
		}
		if (!Mix_PlayingMusic())
			Mix_PlayMusic(music, -1);
		return;
	}

	Mix_Chunk *&chunk = chunks[path];
	if (!chunk)
	{
		chunk = Mix_LoadWAV(path);
		if (!chunk)
		{
			std::cout << "ERROR::SOUND: Mix_LoadWAV " << path << ": " << Mix_GetError() << std::endl;
			return;
		}
	}
	Mix_PlayChannel(-1, chunk, 0);
}

void Sound_Shutdown()
{
	Mix_HaltMusic();
	if (music)
	{
		Mix_FreeMusic(music);
		music = nullptr;
	}
	musicPath.clear();
	for (auto &it : chunks)
	{
		if (it.second)
			Mix_FreeChunk(it.second);
	}
	chunks.clear();
}
