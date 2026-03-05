#include "SystemManager.h"

#include <android/log.h>
#include <filesystem>
#include <fstream>

#include <AndroidCacheHelper.h>

#include "Vertex3D.h"

SystemManager::SystemManager(android_app* androidApp)
{
    createWolfInstance(androidApp);

    m_uniquePass.reset(new UniquePass());
    m_wolfInstance->initializePass(m_uniquePass.createNonOwnerResource<Wolf::CommandRecordBase>());

    {
        std::string inputFilename = "data.bin";
        std::string outputFilename = "data.bin";
        Wolf::copyCompressedFileToStorage(inputFilename, "bin_cache", outputFilename);

        std::ifstream input(outputFilename, std::ios::in | std::ios::binary);

        uint32_t vertexCount;
        input.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));

        std::vector<Vertex3D> vertices(vertexCount);
        input.read(reinterpret_cast<char*>(vertices.data()), vertexCount * sizeof(vertices[0]));

        uint32_t indexCount;
        input.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        std::vector<uint32_t> indices(indexCount);
        input.read(reinterpret_cast<char*>(indices.data()), indexCount * sizeof(indices[0]));

        m_mesh.reset(new Wolf::Mesh(vertices, indices));

        Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> materialsGPUManager = m_wolfInstance->getMaterialsManager();
        materialsGPUManager->lockMaterials();
        materialsGPUManager->lockTextureSets();

        uint32_t textureSetCount;
        input.read(reinterpret_cast<char*>(&textureSetCount), sizeof(textureSetCount));

        if (textureSetCount == 0)
        {
            Wolf::Debug::sendCriticalError("Not texture set found");
        }

        uint32_t firstTextureSetIdx = materialsGPUManager->getCurrentTextureSetCount();
        uint32_t firstMaterialIdx = materialsGPUManager->getCurrentMaterialCount();

        for (uint32_t textureSetIdx = 0; textureSetIdx < textureSetCount; ++textureSetIdx)
        {
            Wolf::MaterialsGPUManager::TextureSetInfo textureSetInfo;

            for (uint32_t imageIdx = 0; imageIdx < Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_MATERIAL; ++imageIdx)
            {
                Wolf::Format format;
                input.read(reinterpret_cast<char*>(&format), sizeof(format));

                if (format == Wolf::Format::UNDEFINED)
                {
                    Wolf::Debug::sendWarning("Format is undefined");
                    continue;
                }

                Wolf::Extent3D extent3D;
                input.read(reinterpret_cast<char*>(&extent3D), sizeof(extent3D));

                uint32_t pixelCount = extent3D.width * extent3D.height * extent3D.depth;
                float bpp = Wolf::Image::computeBPPFromFormat(format);
                std::vector<uint8_t> pixels(pixelCount * bpp);
                input.read(reinterpret_cast<char*>(pixels.data()), pixels.size() * sizeof(pixels[0]));

                Wolf::ResourceUniqueOwner<Wolf::Image>& image = m_images.emplace_back();

                Wolf::CreateImageInfo createImageInfo {};
                createImageInfo.format = format;
                createImageInfo.extent = extent3D;
                createImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::COLOR;
                createImageInfo.mipLevelCount = 1;
                createImageInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
                image.reset(Wolf::Image::createImage(createImageInfo));

                {
                    Wolf::GPUDataTransfersManagerInterface::PushDataToGPUImageInfo pushDataToGpuImageInfo(pixels.data(), image.createNonOwnerResource(), Wolf::Image::SampledInFragmentShader());
                    m_wolfInstance->getGPUDataTransfersManager()->pushDataToGPUImage(pushDataToGpuImageInfo);
                }

                textureSetInfo.images[imageIdx] = image.createNonOwnerResource();
            }

            materialsGPUManager->addNewTextureSet(textureSetInfo);

            Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
            materialInfo.textureSetInfos[0].textureSetIdx = materialsGPUManager->getTextureSetsCacheInfo().back().textureSetIdx;
            materialsGPUManager->addNewMaterial(materialInfo);
        }
        materialsGPUManager->unlockTextureSets();
        materialsGPUManager->unlockMaterials();
    }

    Wolf::RenderMeshList::MeshToRender meshToRender(m_mesh.createNonOwnerResource<Wolf::MeshInterface>(), m_uniquePass->getPipelineSet());
    m_wolfInstance->getRenderMeshList()->registerMesh(meshToRender);

    m_mainCamera.reset(new Wolf::FirstPersonCamera(glm::vec3(1.4f, 1.2f, 0.3f), glm::vec3(2.0f, 0.9f, -0.3f), glm::vec3(0.0f, 1.0f, 0.0f), 0.025f, 10.0f,16.0f / 9.0f));
    m_mainCamera->setLocked(false);

    Wolf::Extent3D swapChainExtent = m_wolfInstance->getSwapChainExtent();
    float aspect = (float)swapChainExtent.height / (float)swapChainExtent.width;
    m_mainCamera->setAspect(aspect);
}

void SystemManager::frame()
{
    m_wolfInstance->getCameraList().addCameraForThisFrame(m_mainCamera.get(), 0);
    m_wolfInstance->updateBeforeFrame();

    Wolf::ResourceNonOwner<Wolf::InputHandler> inputHandler = m_wolfInstance->getInputHandler();
    m_uniquePass->updateGamepad(inputHandler);

    std::vector<Wolf::ResourceNonOwner<Wolf::CommandRecordBase>> passes;
    passes.push_back(m_uniquePass.createNonOwnerResource<Wolf::CommandRecordBase>());
    uint32_t swapChainImageIdx = m_wolfInstance->acquireNextSwapChainImage();
    m_wolfInstance->frame(passes, m_uniquePass->getSemaphore(swapChainImageIdx), swapChainImageIdx);
}

void SystemManager::createWolfInstance(android_app* androidApp)
{
    Wolf::WolfInstanceCreateInfo wolfInstanceCreateInfo;
    wolfInstanceCreateInfo.m_configFilename = "";
    wolfInstanceCreateInfo.m_debugCallback = [this](Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message) { debugCallback(severity, type, message); };
    wolfInstanceCreateInfo.m_androidApp = androidApp;
    wolfInstanceCreateInfo.m_androidWindow = androidApp->window;
    wolfInstanceCreateInfo.m_assetManager = androidApp->activity->assetManager;
    wolfInstanceCreateInfo.m_useMaterialGPUManager = true;

    m_wolfInstance.reset(new Wolf::WolfEngine(wolfInstanceCreateInfo));
}

void SystemManager::debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string &message)
{
    if (severity == Wolf::Debug::Severity::VERBOSE)
        return;

    std::string logMessage;

    switch (severity)
    {
        case Wolf::Debug::Severity::ERROR:
            logMessage += "Error : ";
            break;
        case Wolf::Debug::Severity::WARNING:
            logMessage += "Warning : ";
            break;
        case Wolf::Debug::Severity::INFO:
            logMessage += "Info : ";
            break;
    }

    logMessage += message;
    __android_log_print(ANDROID_LOG_INFO, "WolfEngine", "%s", logMessage.c_str());
}
