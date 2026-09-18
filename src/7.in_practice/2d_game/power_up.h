/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#ifndef POWER_UP_H
#define POWER_UP_H
#include <string>
#include <glm/glm.hpp>
#include "game_object.h"

const glm::vec2 POWERUP_SIZE(60.0f, 20.0f);
const glm::vec2 VELOCITY(0.0f, 150.0f);

class PowerUp : public GameObject
{
public:
	std::string Type;
	float       Duration;
	bool        Activated;
	PowerUp(std::string type, glm::vec3 color, float duration, glm::vec2 position, Texture2D texture)
		: GameObject(position, POWERUP_SIZE, texture, color, VELOCITY), Type(type), Duration(duration), Activated() { }
};

#endif
