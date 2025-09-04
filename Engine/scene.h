#pragma once

#include "structs.h"
#include "renderer.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include <unordered_map>
#include <iostream>

using namespace std;

class SceneManager
{
public:
	Renderer* renderer;
	unordered_map<string, uint32_t> textureMap;
	unordered_map<string, mat4> nodeWorldMap;

	SceneManager(Renderer* renderer);

	~SceneManager();

	void loadScene(const char* path);

	void visit(const aiNode* node, const mat4& parentTransform, const aiScene* scene);

	uint32_t getTextureIndexForMesh(const aiScene* scene, const aiMesh* mesh);

	void readTexture(const char* path);
};