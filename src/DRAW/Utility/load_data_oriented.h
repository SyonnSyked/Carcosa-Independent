#ifndef LOAD_DATA_ORIENTED_H
#define	LOAD_DATA_ORIENTED_H







#include "h2bParser.h"



















class Level_Data {


	std::set<std::string> level_strings;
public:
	struct LEVEL_MODEL
	{
		const char* filename;
		unsigned vertexCount, indexCount, materialCount, meshCount;
		unsigned vertexStart, indexStart, materialStart, meshStart, batchStart;
		unsigned colliderIndex;
		bool isCollidable;
		bool isDynamic;

	};
	struct MODEL_INSTANCES
	{
		unsigned modelIndex, transformStart, transformCount, flags;
	};
	struct MATERIAL_TEXTURES
	{
		unsigned int albedoIndex, roughnessIndex, metalIndex, normalIndex;
	};
	struct BLENDER_OBJECT
	{
		const char* blendername;
		unsigned int modelIndex, transformIndex;
	};

	std::vector<H2B::VERTEX> levelVertices;
	std::vector<unsigned> levelIndices;

	std::vector<H2B::MATERIAL> levelMaterials;

	std::vector<MATERIAL_TEXTURES> levelTextures;

	std::vector<GW::MATH::GMATRIXF> levelTransforms;

	std::vector<GW::MATH::GOBBF> levelColliders;

	std::vector<H2B::BATCH> levelBatches;
	std::vector<H2B::MESH> levelMeshes;
	std::vector<LEVEL_MODEL> levelModels;

	std::vector<MODEL_INSTANCES> levelInstances;

	std::vector<BLENDER_OBJECT> blenderObjects;
	

	bool LoadLevel(	const char* gameLevelPath, 
					const char* h2bFolderPath, 
					GW::SYSTEM::GLog log) {







		std::set<MODEL_ENTRY> uniqueModels;
		log.LogCategorized("EVENT", "LOADING GAME LEVEL [DATA ORIENTED]");

		UnloadLevel();
		if (ReadGameLevel(gameLevelPath, uniqueModels, log) == false) {
			log.LogCategorized("ERROR", "Fatal error reading game level, aborting level load.");
			return false;
		}
		if (ReadAndCombineH2Bs(h2bFolderPath, uniqueModels, log) == false) {
			log.LogCategorized("ERROR", "Fatal error combining H2B mesh data, aborting level load.");
			return false;
		}

		log.LogCategorized("EVENT", "GAME LEVEL WAS LOADED TO CPU [DATA ORIENTED]");
		return true;
	}

	void UnloadLevel() {
		level_strings.clear();
		levelVertices.clear();
		levelIndices.clear();
		levelMaterials.clear();
		levelTextures.clear();
		levelBatches.clear();
		levelMeshes.clear();
		levelModels.clear();
		levelTransforms.clear();
		levelInstances.clear();
		blenderObjects.clear();
	}





private:

	struct MODEL_ENTRY
	{
		std::string modelFile;

		GW::MATH2D::GVECTOR3F boundary[8];
		bool isCollidable = false;
		bool isDynamic = false;
		mutable std::vector<std::string> blenderNames;
		mutable std::vector<GW::MATH::GMATRIXF> instances;
		bool operator<(const MODEL_ENTRY& cmp) const {
			return modelFile < cmp.modelFile;
		}

		GW::MATH::GOBBF ComputeOBB() const {
			GW::MATH::GOBBF out = {
				GW::MATH::GIdentityVectorF,
				GW::MATH::GIdentityVectorF,
				GW::MATH::GIdentityQuaternionF
			};
			out.center.x = (boundary[0].x + boundary[6].x) * 0.5f;
			out.center.y = (boundary[0].y + boundary[6].y) * 0.5f;
			out.center.z = (boundary[0].z + boundary[6].z) * 0.5f;
			out.extent.x = std::fabsf(boundary[0].x - boundary[6].x) * 0.5f;
			out.extent.y = std::fabsf(boundary[0].y - boundary[6].y) * 0.5f;
			out.extent.z = std::fabsf(boundary[0].z - boundary[6].z) * 0.5f;
			return out;
		}
	};

	bool ReadGameLevel(const char* gameLevelPath, 
						std::set<MODEL_ENTRY> &outModels,
						GW::SYSTEM::GLog log) {
		log.LogCategorized("MESSAGE", "Begin Reading Game Level Text File.");
		GW::SYSTEM::GFile file;
		file.Create();
		if (-file.OpenBinaryRead(gameLevelPath)) {
			log.LogCategorized(
				"ERROR", (std::string("Game level not found: ") + gameLevelPath).c_str());
			return false;
		}

		unsigned int fileSize = 0;
		file.GetFileSize(gameLevelPath, fileSize);
		char* jsonFile = new char[fileSize];
		file.Read(jsonFile, fileSize);

		nlohmann::json GameLevelData = nlohmann::json::parse(jsonFile);

		nlohmann::json EntityData;
		std::string blenderName;

		for (auto it = GameLevelData["entities"].begin(); it != GameLevelData["entities"].end(); it++)
		{
			EntityData = it.value();
			blenderName = EntityData["name"];
			MODEL_ENTRY add = { blenderName };
			add.modelFile = add.modelFile.substr(0, add.modelFile.find_last_of("."));
			add.modelFile += ".h2b";
			const std::string modelMessage = "Name is: " + add.modelFile;
			log.LogCategorized("MESSAGE", modelMessage.c_str());

			GW::MATH::GMATRIXF transform;

			for (unsigned int i = 0; i < 16; i++)
				transform.data[i] = EntityData["matrix"][i];

			if (EntityData["is_mesh"])
			{
				for (unsigned int i = 0; i < 8; i++)
				{
					add.boundary[i].x = EntityData["bounds"][i][0];
					add.boundary[i].y = EntityData["bounds"][i][1];
					add.boundary[i].z = EntityData["bounds"][i][2];
				}
			

				std::string bounds = "Boundary: Left ";
				bounds += std::to_string(add.boundary[0].x) +
					" Right " + std::to_string(add.boundary[6].x) +
					" Bottom " + std::to_string(add.boundary[0].y) +
					" Top " + std::to_string(add.boundary[6].y) +
					" Near " + std::to_string(add.boundary[0].z) +
					" Far " + std::to_string(add.boundary[6].z);
				log.LogCategorized("INFO", bounds.c_str());



				if (EntityData.find("staticCollidable") != EntityData.end())
				{
					add.isCollidable = EntityData["staticCollidable"];
				}
				if (EntityData.find("gameType") != EntityData.end())
				{
					// I will expand per-object gameplay metadata after our game design is established.
					add.isDynamic = EntityData["gameType"] != "none";
				}


				auto found = outModels.find(add);
				if (found == outModels.end())
				{
					add.blenderNames.push_back(blenderName);
					add.instances.push_back(transform);
					outModels.insert(add);
				}
				else
				{
					found->blenderNames.push_back(blenderName);
					found->instances.push_back(transform);
				}
			}
		}
		log.LogCategorized("MESSAGE", "Game Level File Reading Complete.");
		return true;
	}

	bool ReadAndCombineH2Bs(const char* h2bFolderPath, 
							const std::set<MODEL_ENTRY>& modelSet,
							GW::SYSTEM::GLog log) {
		log.LogCategorized("MESSAGE", "Begin Importing .H2B File Data.");

		H2B::Parser p;
		const std::string modelPath = h2bFolderPath;
		for (auto i = modelSet.begin(); i != modelSet.end(); ++i)
		{
			if (p.Parse((modelPath + "/" + i->modelFile).c_str()))
			{
				log.LogCategorized("INFO", (std::string("H2B Imported: ") + i->modelFile).c_str());

				for (int j = 0; j < p.materialCount; ++j) {
					for (int k = 0; k < 10; ++k) {
						if (*((&p.materials[j].name) + k) != nullptr)
							*((&p.materials[j].name) + k) =
							level_strings.insert(*((&p.materials[j].name) + k)).first->c_str();
					}
				}
				for (int j = 0; j < p.meshCount; ++j) {
					if (p.meshes[j].name != nullptr)
						p.meshes[j].name =
						level_strings.insert(p.meshes[j].name).first->c_str();
				}

				LEVEL_MODEL model;
				model.filename = level_strings.insert(i->modelFile).first->c_str();
				model.vertexCount = p.vertexCount;
				model.indexCount = p.indexCount;
				model.materialCount = p.materialCount;
				model.meshCount = p.meshCount;

				model.vertexStart = levelVertices.size();
				model.indexStart = levelIndices.size();
				model.materialStart = levelMaterials.size();
				model.batchStart = levelBatches.size();
				model.meshStart = levelMeshes.size();
				model.isCollidable = i->isCollidable;
				model.isDynamic = i->isDynamic;

				levelVertices.insert(levelVertices.end(), p.vertices.begin(), p.vertices.end());
				levelIndices.insert(levelIndices.end(), p.indices.begin(), p.indices.end());
				levelMaterials.insert(levelMaterials.end(), p.materials.begin(), p.materials.end());
				levelBatches.insert(levelBatches.end(), p.batches.begin(), p.batches.end());
				levelMeshes.insert(levelMeshes.end(), p.meshes.begin(), p.meshes.end());

				model.colliderIndex = levelColliders.size();
				levelColliders.push_back(i->ComputeOBB());

				levelModels.push_back(model);

				MODEL_INSTANCES instances;
				instances.flags = 0;
				instances.modelIndex = levelModels.size() - 1;
				instances.transformStart = levelTransforms.size();
				instances.transformCount = i->instances.size();
				levelTransforms.insert(levelTransforms.end(), i->instances.begin(), i->instances.end());

				levelInstances.push_back(instances);

				int offset = 0;
				for (auto &n : i->blenderNames) {
					BLENDER_OBJECT obj {
						level_strings.insert(n).first->c_str(),
						instances.modelIndex, instances.transformStart + offset++
					};
					blenderObjects.push_back(obj);
				}
			}
			else {

				log.LogCategorized("ERROR",
					(std::string("H2B Not Found: ") + modelPath + "/" + i->modelFile).c_str());
				log.LogCategorized("WARNING", "Loading will continue but model(s) are missing.");
			}
		}
		log.LogCategorized("MESSAGE", "Importing of .H2B File Data Complete.");
		return true;
	}
};
#endif
