/*******************************************************************
** This code is part of Breakout.
******************************************************************/
#include "post_processor.h"
#include "game_uniforms.h"
#include "pgl_gl.h"
#include <iostream>

PostProcessor::PostProcessor(Shader shader, unsigned int width, unsigned int height)
	: PostProcessingShader(shader), Texture(), Width(width), Height(height), Confuse(false), Chaos(false), Shake(false)
{
	// PGL's glRenderbufferStorageMultisample is a no-op; render straight to a
	// color texture FBO instead of MSAA + blit.
	glGenFramebuffers(1, &this->FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, this->FBO);
	this->Texture.Internal_Format = GL_RGBA;
	this->Texture.Image_Format = GL_RGBA;
	this->Texture.Generate(width, height, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->Texture.ID, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::POSTPROCESSOR: Failed to initialize FBO" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	this->initRenderData();
	this->PostProcessingShader.Use();
	float offset = 1.0f / 300.0f;
	glm::vec2 offs[9] = {
		{ -offset,  offset  },
		{  0.0f,    offset  },
		{  offset,  offset  },
		{ -offset,  0.0f    },
		{  0.0f,    0.0f    },
		{  offset,  0.0f    },
		{ -offset, -offset  },
		{  0.0f,   -offset  },
		{  offset, -offset  }
	};
	int edge_kernel[9] = { -1, -1, -1, -1, 8, -1, -1, -1, -1 };
	float blur_kernel[9] = {
		1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f,
		2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f,
		1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f
	};
	for (int i = 0; i < 9; i++) {
		uniforms.offsets[i] = offs[i];
		uniforms.edge_kernel[i] = edge_kernel[i];
		uniforms.blur_kernel[i] = blur_kernel[i];
	}
}

void PostProcessor::BeginRender()
{
	glBindFramebuffer(GL_FRAMEBUFFER, this->FBO);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void PostProcessor::EndRender()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::Render(float time)
{
	this->PostProcessingShader.Use();
	uniforms.time = time;
	uniforms.confuse = this->Confuse;
	uniforms.chaos = this->Chaos;
	uniforms.shake = this->Shake;
	uniforms.tex = this->Texture.ID;
	glActiveTexture(GL_TEXTURE0);
	this->Texture.Bind();
	glBindVertexArray(this->VAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void PostProcessor::initRenderData()
{
	unsigned int VBO;
	float vertices[] = {
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f
	};
	glGenVertexArrays(1, &this->VAO);
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindVertexArray(this->VAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}
