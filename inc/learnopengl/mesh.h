#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <uniforms.h>

#include <string>
#include <vector>
using namespace std;

#define MAX_BONE_INFLUENCE 4

struct Vertex {
	// position
	glm::vec3 Position;
	// normal
	glm::vec3 Normal;
	// texCoords
	glm::vec2 TexCoords;
	// tangent
	glm::vec3 Tangent;
	// bitangent
	glm::vec3 Bitangent;
	//bone indexes which will influence this vertex
	int m_BoneIDs[MAX_BONE_INFLUENCE];
	//weights from each bone
	float m_Weights[MAX_BONE_INFLUENCE];
};

struct Texture {
	unsigned int id;
	string type;
	string path;
};

class Mesh {
public:
	// mesh Data
	vector<Vertex>       vertices;
	vector<unsigned int> indices;
	vector<Texture>      textures;
	unsigned int VAO;

	// constructor
	Mesh(vector<Vertex> vertices, vector<unsigned int> indices, vector<Texture> textures)
	{
		this->vertices = vertices;
		this->indices = indices;
		this->textures = textures;

		// now that we have all the required data, set the vertex buffers and its attribute pointers.
		setupMesh();
	}

	// render the mesh
	void Draw(GLuint shader, Model_Uniforms* uniforms) 
	{
		// bind appropriate textures
		unsigned int diffuseNr  = 0;
		unsigned int specularNr = 0;
		unsigned int normalNr   = 0;
		unsigned int heightNr   = 0;
		for(unsigned int i = 0; i < textures.size(); i++)
		{
			//glActiveTexture(GL_TEXTURE0 + i); // active proper texture unit before binding

			// retrieve texture number (the N in diffuse_textureN)
			string number;
			string name = textures[i].type;
			if(name == "texture_diffuse")
				uniforms->texture_diffuse[diffuseNr++] = textures[i].id;
			else if(name == "texture_specular")
				uniforms->texture_specular[specularNr++] = textures[i].id;
			else if(name == "texture_normal")
				uniforms->texture_normal[normalNr++] = textures[i].id;
			 else if(name == "texture_height")
				uniforms->texture_height[heightNr++] = textures[i].id;

			// TODO(rswinkle) I'm not sure this is actually necessary...
			// since I think it's only here to tie it ACTIVE TEXTURE unit
			// which I commented above because PGL doesn't have the
			// concept of Texture Units
			
			// and finally bind the texture
			//glBindTexture(GL_TEXTURE_2D, textures[i].id);
		}
		
		// draw mesh
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		// always good practice to set everything back to defaults once configured.
		//glActiveTexture(GL_TEXTURE0);
	}

private:
	// render data 
	unsigned int VBO, EBO;

	// initializes all the buffer objects/arrays
	void setupMesh()
	{
		// create buffers/arrays
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		// A great thing about structs is that their memory layout is sequential for all its items.
		// The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
		// again translates to 3/2 floats which translates to a byte array.
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);  

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex Positions
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
		// vertex normals
		glEnableVertexAttribArray(1);
		pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, Normal));
		// vertex texture coords
		glEnableVertexAttribArray(2);
		pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, TexCoords));

		/*
		// vertex tangent
		glEnableVertexAttribArray(3);
		pglVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, Tangent));
		// vertex bitangent
		glEnableVertexAttribArray(4);
		pglVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, Bitangent));

		// ids
		//glEnableVertexAttribArray(5);
		// TODO is there any reason to use IPointer or LPointer?  Well PGL doesn't support anything put float
		// attributes anyway right now so...
		//pglVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), offsetof(Vertex, m_BoneIDs));

		// weights
		glEnableVertexAttribArray(6);
		pglVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, m_Weights));
		glBindVertexArray(0);
		*/
	}
};
#endif
