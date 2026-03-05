#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include <glm/glm.hpp>

#include <JSONReader.h>
#include <MaterialsGPUManager.h>

void debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message)
{
	switch (severity)
	{
		case Wolf::Debug::Severity::ERROR:
			std::cout << "Error : ";
			break;
		case Wolf::Debug::Severity::WARNING:
			std::cout << "Warning : ";
			break;
		case Wolf::Debug::Severity::INFO:
			std::cout << "Info : ";
			break;
		case Wolf::Debug::Severity::VERBOSE:
			return;
		default: ;
	}

	std::cout << message << '\n';
}


std::string g_exportFolder;
std::map<std::string, uint32_t> g_assets;

std::unique_ptr<std::ofstream> g_outFileStream;

void addAsset(const std::string& loadingPath, uint32_t assetId)
{
	std::cout << "Adding asset " + loadingPath + "\n";
	g_assets[loadingPath] = assetId;
}

void loadAssets(const std::string& filePath)
{
	Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { filePath});

	uint32_t meshCount = jsonReader.getRoot()->getArraySize("meshes");
	for (uint32_t i = 0; i < meshCount; ++i)
	{
		Wolf::JSONReader::JSONObjectInterface* meshObject = jsonReader.getRoot()->getArrayObjectItem("meshes", i);
		addAsset(meshObject->getPropertyString("loadingPath"), static_cast<uint32_t>(meshObject->getPropertyFloat("assetId")));
	}

	uint32_t imageCount = jsonReader.getRoot()->getArraySize("images");
	for (uint32_t i = 0; i < imageCount; ++i)
	{
		Wolf::JSONReader::JSONObjectInterface* imageObject = jsonReader.getRoot()->getArrayObjectItem("images", i);
		addAsset(imageObject->getPropertyString("loadingPath"), static_cast<uint32_t>(imageObject->getPropertyFloat("assetId")));
	}
}

void loadImage(uint32_t textureSetIdx, uint32_t assetId)
{
	std::cout << "Loading image (assetId " << assetId << ")\n";

	std::string assetPath = g_exportFolder + std::to_string(assetId) + ".bin";
	if (!std::filesystem::exists(assetPath) || assetId == 0)
	{
		std::cout << "File does not exist\n";

		Wolf::Format undefinedFormat = Wolf::Format::UNDEFINED;
		g_outFileStream->write(reinterpret_cast<const char*>(&undefinedFormat), sizeof(undefinedFormat));

		return;
	}

	std::ifstream input(assetPath, std::ios::in | std::ios::binary);

	Wolf::Format format;
	input.read(reinterpret_cast<char*>(&format), sizeof(format));

	if (format == Wolf::Format::UNDEFINED)
	{
		throw std::runtime_error("Undefined format");
	}

	Wolf::Extent3D extent3D;
	input.read(reinterpret_cast<char*>(&extent3D), sizeof(extent3D));

	uint32_t pixelCount = extent3D.width * extent3D.height * extent3D.depth;
	float bpp = Wolf::Image::computeBPPFromFormat(format);
	std::vector<uint8_t> pixels(pixelCount * bpp);
	input.read(reinterpret_cast<char*>(pixels.data()), pixels.size() * sizeof(pixels[0]));

	g_outFileStream->write(reinterpret_cast<const char*>(&format), sizeof(format));
	g_outFileStream->write(reinterpret_cast<const char*>(&extent3D), sizeof(extent3D));
	g_outFileStream->write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(pixels[0]));

	uint32_t mipCount;
	input.read(reinterpret_cast<char*>(&mipCount), sizeof(mipCount));
	g_outFileStream->write(reinterpret_cast<const char*>(&mipCount), sizeof(mipCount));

	std::cout << mipCount << " mips found\n";

	for (uint32_t mipLevel = 1; mipLevel < mipCount + 1; ++mipLevel)
	{
		const Wolf::Extent3D mipExtent = { extent3D.width >> mipLevel, extent3D.height >> mipLevel, extent3D.depth };
		uint32_t copySize = mipExtent.width * mipExtent.height * mipExtent.depth * Wolf::Image::computeBPPFromFormat(format);

		std::vector<uint8_t> mipPixels(copySize);
		input.read(reinterpret_cast<char*>(mipPixels.data()), mipPixels.size() * sizeof(mipPixels[0]));
		g_outFileStream->write(reinterpret_cast<const char*>(mipPixels.data()), mipPixels.size() * sizeof(mipPixels[0]));
	}

	std::cout << "Loaded " << pixelCount << " pixels" << std::endl;
}

struct InVertex3D
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 texCoord;
	glm::uint subMeshIdx;
};

using OutVertex3D = InVertex3D;

void loadMesh(const std::string& filePath)
{
	std::cout << "Loading mesh " << filePath << "\n";

	uint32_t assetId = g_assets.find(filePath)->second;
	std::string assetPath = g_exportFolder + std::to_string(assetId) + ".bin";

	std::cout << "Reading " << assetPath << "\n";
	if (!std::filesystem::exists(assetPath))
	{
		std::cerr << "File does not exist\n";
		return;
	}

	std::ifstream input(assetPath, std::ios::in | std::ios::binary);

	uint8_t isAnimated;
	input.read(reinterpret_cast<char*>(&isAnimated), sizeof(isAnimated));

	uint32_t vertexCount;
	input.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));

	if (isAnimated)
	{
		std::cerr << "Animated mesh not currently supported\n";
		return;
	}

	std::vector<InVertex3D> vertices(vertexCount);
	input.read(reinterpret_cast<char*>(vertices.data()), vertexCount * sizeof(vertices[0]));

	std::vector<OutVertex3D> outVertices(vertexCount);
	for (uint32_t vertexIdx = 0; vertexIdx < vertexCount; ++vertexIdx)
	{
		outVertices[vertexIdx] = vertices[vertexIdx];
	}
	g_outFileStream->write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
	g_outFileStream->write(reinterpret_cast<const char*>(outVertices.data()), sizeof(outVertices[0]) * vertexCount);

	std::cout << "Loaded " << vertexCount << " vertices" << std::endl;

	uint32_t indexCount;
	input.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

	std::vector<uint32_t> indices(indexCount);
	input.read(reinterpret_cast<char*>(indices.data()), indexCount * sizeof(indices[0]));

	g_outFileStream->write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
	g_outFileStream->write(reinterpret_cast<const char*>(indices.data()), sizeof(indices[0]) * indexCount);

	std::cout << "Loaded " << indexCount << " indices" << std::endl;

	uint32_t textureSetCount;
	input.read(reinterpret_cast<char*>(&textureSetCount), sizeof(textureSetCount));
	std::vector<uint32_t> textureSetAssetIds(textureSetCount * Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL);
	input.read(reinterpret_cast<char*>(textureSetAssetIds.data()), textureSetAssetIds.size() * sizeof(textureSetAssetIds[0]));
	std::cout << "Found " << textureSetCount << " texture sets" << std::endl;

	g_outFileStream->write(reinterpret_cast<const char*>(&textureSetCount), sizeof(textureSetCount));

	for (uint32_t textureSetIdx = 0; textureSetIdx < textureSetCount; ++textureSetIdx)
	{
		std::cout << "Loading texture set " << textureSetIdx << std::endl;

		for (uint32_t textureIdx = 0; textureIdx < Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL; ++textureIdx)
		{
			std::cout << "Loading texture " << textureIdx << std::endl;

			uint32_t idx = textureSetIdx * Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL + textureIdx;
			uint32_t assetId = textureSetAssetIds[idx];

			loadImage(textureSetIdx, assetId);
		}
	}
}

void loadEntity(const std::string& filePath)
{
	std::cout << "Loading entity " + filePath + "\n";

	const std::ifstream inFile(filePath);
	if (!filePath.empty() && inFile.good())
	{
		Wolf::JSONReader jsonReader(Wolf::JSONReader::FileReadInfo { filePath});

		const uint32_t componentCount = jsonReader.getRoot()->getPropertyCount();
		for (uint32_t i = 0; i < componentCount; ++i)
		{
			std::string componentId = jsonReader.getRoot()->getPropertyString(i);
			if (componentId == "entity")
				continue;

			std::cout << "Component " + componentId + " found\n";

			if (componentId == "staticModel")
			{
				std::cout << "Loading component\n";

				Wolf::JSONReader::JSONObjectInterface* staticModelParamsObject = jsonReader.getRoot()->getPropertyObject(componentId);
				uint32_t paramCount = staticModelParamsObject->getArraySize("params");
				for (uint32_t j = 0; j < paramCount; ++j)
				{
					Wolf::JSONReader::JSONObjectInterface* param = staticModelParamsObject->getArrayObjectItem("params", j);
					std::string paramName = param->getPropertyString("name");

					if (paramName == "Mesh")
					{
						std::string meshPath = param->getPropertyString("value");
						loadMesh(meshPath);
					}
				}
			}
			else
			{
				std::cout << "Component ignored\n";
			}
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "Not enough arguments\n";
		return EXIT_FAILURE;
	}

	Wolf::Debug::setCallback(debugCallback);

	g_outFileStream.reset(new std::ofstream("data.bin", std::ios::out | std::ios::binary));

	g_exportFolder = argv[1];

	if (g_exportFolder.back() != '/' && g_exportFolder.back() != '\\')
	{
		g_exportFolder += "/";
	}

	std::string sceneFullPath = g_exportFolder + "scene.json";
	std::string assetsFullPath = g_exportFolder + "assets.json";

	loadAssets(assetsFullPath);

	Wolf::JSONReader exportSONReader(Wolf::JSONReader::FileReadInfo { sceneFullPath });

	std::string sceneJSON = exportSONReader.getRoot()->getPropertyString("sceneJSON");
	std::string dataFolder = exportSONReader.getRoot()->getPropertyString("dataFolder");
	std::cout << "Generating data for " + sceneJSON + "\n";

	Wolf::JSONReader sceneJSONReader(Wolf::JSONReader::FileReadInfo { dataFolder + "/" + sceneJSON });

	const std::string& sceneName = sceneJSONReader.getRoot()->getPropertyString("sceneName");
	std::cout << "Scene name is " + sceneName + "\n";

	const uint32_t entityCount = static_cast<uint32_t>(sceneJSONReader.getRoot()->getPropertyFloat("entityCount"));
	for(uint32_t entityIdx = 0; entityIdx < entityCount; entityIdx++)
	{
		Wolf::JSONReader::JSONObjectInterface* entityObject = sceneJSONReader.getRoot()->getArrayObjectItem("entities", entityIdx);

		const std::string& entityLoadingPath = entityObject->getPropertyString("loadingPath");
		std::string deduplicatedLoadingPath;
		bool ignoreNextCharacter = false;
		for (const char character : entityLoadingPath)
		{
			if (ignoreNextCharacter)
			{
				ignoreNextCharacter = false;
				continue;
			}

			if (character == '\\')
			{
				deduplicatedLoadingPath += '/';
				ignoreNextCharacter = true;
			}
			else
			{
				deduplicatedLoadingPath += character;
			}
		}

		loadEntity(dataFolder + "/" + deduplicatedLoadingPath);
	}

	g_outFileStream.reset(nullptr);
}
