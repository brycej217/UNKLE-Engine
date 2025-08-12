#define GLM_ENABLE_EXPERIMENTAL
#include "scene.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <tiny_obj_loader.h>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vector>
#include <algorithm>
#include <array>
#include <fstream>
#include <chrono>
#include <unordered_map>

using namespace std;

SceneManager::SceneManager(Renderer* renderer)
{
	this->renderer = renderer;
}

SceneManager::~SceneManager()
{

}

static mat4 convertAIMatrix(const aiMatrix4x4& m)
{
	mat4 mat = mat4(
		m.a1, m.b1, m.c1, m.d1,
		m.a2, m.b2, m.c2, m.d2,
		m.a3, m.b3, m.c3, m.d3,
		m.a4, m.b4, m.c4, m.d4
	);
	return mat;
}

void SceneManager::loadScene(const char* path)
{
	// load scene
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path,
		aiProcess_MakeLeftHanded |
		aiProcess_Triangulate |
		aiProcess_GlobalScale |
		aiProcess_GenSmoothNormals |
		aiProcess_FlipUVs |
		aiProcess_JoinIdenticalVertices);

	if (!scene) throw runtime_error("failed to load scene");

	// setup vertices and indices SSBOs
	vector<Vertex> verticesSSBO;
	vector<uint32_t> indicesSSBO;

	// create empty texture
	const uint32_t pixel = 0xFFFFFFFFu;
	renderer->createTexture((void*)&pixel, 1 * 1 * 4, 1, 1);

	// load mesh vertex and index buffers
	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		const aiMesh* currMesh = scene->mMeshes[i];

		Mesh mesh;

		mesh.vertexOffset = static_cast<uint32_t>(verticesSSBO.size()); // first vertex corrseponding to this mesh

		// find vertices
		for (unsigned int j = 0; j < currMesh->mNumVertices; j++)
		{
			Vertex vertex;
			vertex.position = vec3(currMesh->mVertices[j].x, currMesh->mVertices[j].y, currMesh->mVertices[j].z);
			vertex.normal = currMesh->HasNormals() ? vec3(currMesh->mNormals[j].x, currMesh->mNormals[j].y, currMesh->mNormals[j].z) : vec3(0.0f);
			vertex.texCoord = currMesh->HasTextureCoords(0) ? vec2(currMesh->mTextureCoords[0][j].x, currMesh->mTextureCoords[0][j].y) : vec2(0.0f);

			verticesSSBO.push_back(vertex);
		}

		// find indices
		mesh.firstIndex = static_cast<uint32_t>(indicesSSBO.size()); // set first index of the mesh to the size of the index buffer

		uint32_t indexCount = 0;
		for (unsigned int j = 0; j < currMesh->mNumFaces; j++)
		{
			const aiFace* face = &currMesh->mFaces[j];

			for (unsigned int k = 0; k < face->mNumIndices; k++)
			{
				indicesSSBO.push_back(face->mIndices[k]);
				indexCount++;
			}
		}
		
		mesh.indexCount = indexCount;

		renderer->meshes.push_back(mesh);
		cout << "found " << verticesSSBO.size() << " vertices" << endl;
	}
	renderer->createVertexBuffer(verticesSSBO, indicesSSBO, renderer->pipelineFeed.vertexSSBO, renderer->pipelineFeed.indexSSBO);

	// load instance data
	visit(scene->mRootNode, mat4(1.0f), scene);

	// load light data
	for (unsigned int i = 0; i < scene->mNumLights; i++)
	{
		const aiLight* L = scene->mLights[i];

		mat4 world = mat4(1.0f);
		if (nodeWorldMap.find(L->mName.C_Str()) != nodeWorldMap.end())
		{
			world = nodeWorldMap[L->mName.C_Str()];
		}

		vec3 localPosition = vec3(L->mPosition.x, L->mPosition.y, L->mPosition.z);
		vec3 localDirection = vec3(L->mDirection.x, L->mDirection.y, L->mDirection.z);

		Light light
		{
			.position = vec3(world * vec4(localPosition, 1.0f)),
			.direction = vec3(world * vec4(localDirection, 1.0f)),
			.color = vec3(L->mColorDiffuse.r, L->mColorDiffuse.g, L->mColorDiffuse.b) / 1000.0f,
		};

		cout << to_string(light.color) << endl;

		renderer->pipelineFeed.lights.push_back(light);
	}
}

void SceneManager::visit(const aiNode* node, const mat4& parentTransform, const aiScene* scene)
{
	// determine world matrix based on parent world and local transforms
	mat4 local = convertAIMatrix(node->mTransformation);
	mat4 world = parentTransform * local;
	nodeWorldMap[node->mName.C_Str()] = world;

	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		// determine which of the scene's meshes this mesh refers to
		unsigned meshIndex = node->mMeshes[i];
		const aiMesh* currMesh = scene->mMeshes[meshIndex];
		Mesh* mesh = &renderer->meshes[meshIndex];

		// record current index of transform buffers and create transform buffer
		uint32_t transformIndex = static_cast<uint32_t>(renderer->pipelineFeed.transforms.size());

		VkBuffer transformBuffer;
		VmaAllocationInfo transformInfo;
		VkDeviceSize size = sizeof(mat4);

		MVP mvp;
		mvp.model = world;

		renderer->pipelineFeed.transforms.push_back(mvp);
		
		uint32_t textureIndex = getTextureIndexForMesh(scene, currMesh);

		Instance instance
		{
			.transformIndex = transformIndex,
			.textureIndex = textureIndex
		};

		// create instance entry for mesh
		if (mesh->firstInstance == -1)
		{
			mesh->firstInstance = static_cast<uint32_t>(renderer->pipelineFeed.instances.size());
		}
		renderer->pipelineFeed.instances.insert(renderer->pipelineFeed.instances.begin() + mesh->firstInstance + mesh->instanceCount, instance);

		mesh->instanceCount += 1; // increment mesh's count as we have found the mesh
	}

	for (unsigned i = 0; i < node->mNumChildren; i++)
	{
		visit(node->mChildren[i], world, scene);
	}
}

uint32_t SceneManager::getTextureIndexForMesh(const aiScene* scene, const aiMesh* mesh)
{
	uint32_t index = 0;

	const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

	aiString path;

	if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
	{
		string key(path.C_Str());

		if (textureMap.find(key) != textureMap.end())
		{
			return textureMap[key];
		}
		else
		{
			index = static_cast<uint32_t>(renderer->pipelineFeed.textureImages.size());
			cout << "texture index: " << index << endl;
			readTexture(path.C_Str());
			textureMap[key] = index;
		}
	}

	return index;
}

void SceneManager::readTexture(const char* path)
{
	LOG("Initializing texture");

	// read image data
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) throw runtime_error("failed to load mesh texture");

	renderer->createTexture(pixels, imageSize, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	stbi_image_free(pixels);
}