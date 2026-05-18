#include "SystemManager.h"

#include <iostream>

#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <filesystem>
#include <fstream>

#include <AndroidCacheHelper.h>

#include "Vertex3D.h"

SystemManager::SystemManager(
#ifdef __ANDROID__
    android_app* androidApp
#endif
    )
{
    createWolfInstance(
#ifdef __ANDROID__
    androidApp
#endif
    );

    m_uniquePass.reset(new UniquePass());
    m_wolfInstance->initializePass(m_uniquePass.createNonOwnerResource<Wolf::CommandRecordBase>());
    m_bufferPoolInterface = m_wolfInstance->getDefaultMeshBufferPool().duplicateAs<Wolf::BufferPoolInterface>();

    {
        std::string inputFilename = "data.bin";
        std::string outputFilename = "data.bin";
#ifdef __ANDROID__
        Wolf::copyCompressedFileToStorage(inputFilename, "bin_cache", outputFilename);
#endif

        std::ifstream input(outputFilename, std::ios::in | std::ios::binary);

        uint32_t materialCount;
        input.read(reinterpret_cast<char*>(&materialCount), sizeof(materialCount));

        std::vector<uint32_t> materials(materialCount);
        for (uint32_t materialIdx = 0; materialIdx < materialCount; ++materialIdx)
        {
            Wolf::Format albedoFormat;
            input.read(reinterpret_cast<char*>(&albedoFormat), sizeof(uint32_t));

            if (static_cast<uint32_t>(albedoFormat) == -1)
            {
                materials[materialIdx] = -1;
                continue;
            }

            Wolf::Extent3D albedoExtent;
            input.read(reinterpret_cast<char*>(&albedoExtent.width), sizeof(uint32_t));
            input.read(reinterpret_cast<char*>(&albedoExtent.height), sizeof(uint32_t));
            input.read(reinterpret_cast<char*>(&albedoExtent.depth), sizeof(uint32_t));

            uint32_t albedoMiplevelCount;
            input.read(reinterpret_cast<char*>(&albedoMiplevelCount), sizeof(albedoMiplevelCount));

            Wolf::CreateImageInfo createImageInfo {};
            createImageInfo.format = albedoFormat;
            createImageInfo.extent = albedoExtent;
            createImageInfo.aspectFlags = Wolf::ImageAspectFlagBits::COLOR;
            createImageInfo.mipLevelCount = albedoMiplevelCount;
            createImageInfo.usage = Wolf::ImageUsageFlagBits::TRANSFER_DST | Wolf::ImageUsageFlagBits::SAMPLED;
            Wolf::ResourceUniqueOwner<Wolf::Image>& image = m_images.emplace_back();
            image.reset(Wolf::Image::createImage(createImageInfo));

            for (uint32_t mipLevel = 0; mipLevel < albedoMiplevelCount; ++mipLevel)
            {
                uint32_t mipPixelCount;
                input.read(reinterpret_cast<char*>(&mipPixelCount), sizeof(mipPixelCount));

                uint32_t copySize = mipPixelCount * Wolf::Image::computeBPPFromFormat(albedoFormat);
                uint8_t* data = new uint8_t[copySize];
                input.read(reinterpret_cast<char*>(data), copySize);

                Wolf::GPUDataTransfersManagerInterface::PushDataToGPUImageInfo pushDataToGpuImageInfo(data, image.createNonOwnerResource(),
                                                                                                      Wolf::Image::SampledInFragmentShader(mipLevel), mipLevel);
                m_wolfInstance->getGPUDataTransfersManager()->pushDataToGPUImage(pushDataToGpuImageInfo);
            }

            materials[materialIdx] = m_images.size() - 1;
        }

        uint32_t meshCount;
        input.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));

        for (uint32_t meshIdx = 0; meshIdx < meshCount; ++meshIdx)
        {
            MeshInfo& mesh = m_meshes.emplace_back();

            Wolf::AABB meshAABB;
            input.read(reinterpret_cast<char*>(&meshAABB), sizeof(meshAABB));

            uint32_t lodCount;
            input.read(reinterpret_cast<char*>(&lodCount), sizeof(lodCount));

            for (uint32_t lod = 0; lod < lodCount; ++lod)
            {
                Wolf::ResourceUniqueOwner<Wolf::Mesh>& lodMesh = mesh.m_lods.emplace_back();

                uint32_t vertexCount;
                input.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));

                std::vector<Vertex3D> vertices(vertexCount);
                for (uint32_t vertexIdx = 0; vertexIdx < vertexCount; ++vertexIdx)
                {
                    Vertex3D& vertex= vertices[vertexIdx];
                    input.read(reinterpret_cast<char*>(&vertex.pos), sizeof(vertex.pos));
                    input.read(reinterpret_cast<char*>(&vertex.texCoord), sizeof(vertex.texCoord));
                }

                uint32_t indexCount;
                input.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));

                std::vector<uint32_t> indices(indexCount);
                for (uint32_t indexIdx = 0; indexIdx < indexCount; ++indexIdx)
                {
                    input.read(reinterpret_cast<char*>(&indices[indexIdx]), sizeof(uint32_t));
                }

                lodMesh.reset(new Wolf::Mesh(vertices, indices, m_bufferPoolInterface, meshAABB, Wolf::BoundingSphere(meshAABB)));
            }

            uint32_t instanceCount;
            input.read(reinterpret_cast<char*>(&instanceCount), sizeof(instanceCount));

            for (uint32_t instanceIdx = 0; instanceIdx < instanceCount; ++instanceIdx)
            {
                MeshInfo::InstanceInfo& instanceInfo = mesh.m_instances.emplace_back();

                input.read(reinterpret_cast<char*>(&instanceInfo.m_transform), sizeof(instanceInfo.m_transform));
                input.read(reinterpret_cast<char*>(&instanceInfo.m_materialIdx), sizeof(instanceInfo.m_materialIdx));
            }
        }

        Wolf::ResourceNonOwner<Wolf::MaterialsGPUManager> materialsGPUManager = m_wolfInstance->getMaterialsManager();
        materialsGPUManager->lockMaterials();
        materialsGPUManager->lockTextureSets();

        uint32_t firstTextureSetIdx = materialsGPUManager->getCurrentTextureSetCount();
        uint32_t firstMaterialIdx = materialsGPUManager->getCurrentMaterialCount();

        for (uint32_t materialIdx = 0; materialIdx < materialCount; ++materialIdx)
        {
            Wolf::MaterialsGPUManager::TextureSetInfo textureSetInfo;

            for (uint32_t imageIdx = 0; imageIdx < Wolf::MaterialsGPUManager::TEXTURE_COUNT_PER_TEXTURE_SET; ++imageIdx)
            {
                if (imageIdx > 0)
                {
                    continue;
                }

                if (materials[materialIdx] != -1)
                {
                    textureSetInfo.images[imageIdx] = m_images[materials[materialIdx]].createNonOwnerResource();
                }
            }

            materialsGPUManager->addNewTextureSet(textureSetInfo);

            Wolf::MaterialsGPUManager::MaterialInfo materialInfo;
            materialInfo.textureSetInfos[0].textureSetIdx = materialsGPUManager->getTextureSetsCacheInfo().back().textureSetIdx;
            materialsGPUManager->addNewMaterial(materialInfo);
        }
        materialsGPUManager->unlockTextureSets();
        materialsGPUManager->unlockMaterials();

        for (uint32_t meshIdx = 0; meshIdx < m_meshes.size(); ++meshIdx)
        {
            const MeshInfo& meshInfo = m_meshes[meshIdx];
            float radius = meshInfo.m_lods[0]->getBoundingSphere().getRadius();
            float quality = 0.5f;

            Wolf::InstanceMeshRenderer::MeshToRender meshToRenderInfo = { m_uniquePass->getPipelineSet() };
            meshToRenderInfo.m_lods.emplace_back(meshInfo.m_lods[0].createNonOwnerResource<Wolf::MeshInterface>(),
                                                 meshInfo.m_lods.empty() ? 10'000.0f : Wolf::InstanceMeshRenderer::computeLODDistance(radius, meshInfo.m_lods[0]->getIndexCount(), quality));

            for (uint32_t lod = 0; lod < meshInfo.m_lods.size(); ++lod)
            {
                float lodDistance = lod == meshInfo.m_lods.size() - 1 ? 10'000.0f : Wolf::InstanceMeshRenderer::computeLODDistance(radius, meshInfo.m_lods[lod]->getIndexCount(), quality);
                meshToRenderInfo.m_lods.emplace_back(meshInfo.m_lods[lod].createNonOwnerResource<Wolf::MeshInterface>(), lodDistance);
            }

            uint32_t registeredMeshIdx = m_wolfInstance->getInstanceMeshRenderer()->registerMesh(meshToRenderInfo);

            for (uint32_t instanceIdx = 0; instanceIdx < meshInfo.m_instances.size(); ++instanceIdx)
            {
                const MeshInfo::InstanceInfo& instanceInfo = meshInfo.m_instances[instanceIdx];

                [[maybe_unused]] uint32_t registeredInstanceIdx = m_wolfInstance->getInstanceMeshRenderer()->addInstance(registeredMeshIdx, instanceInfo.m_transform,
                    instanceInfo.m_materialIdx + firstMaterialIdx, 0, m_uniquePass->getPipelineSet(),
                    meshToRenderInfo.m_perPipelineDescriptorSets);
            }
        }
    }

    m_mainCamera.reset(new Wolf::FirstPersonCamera(glm::vec3(1.4f, 1.2f, 0.3f), glm::vec3(2.0f, 0.9f, -0.3f), glm::vec3(0.0f, 1.0f, 0.0f), 0.025f, 10.0f,16.0f / 9.0f));
    m_mainCamera->setLocked(false);

    Wolf::Extent3D swapChainExtent = m_wolfInstance->getSwapChainExtent();
    float aspect = (float)swapChainExtent.height / (float)swapChainExtent.width;
    m_mainCamera->setAspect(aspect);
}

void SystemManager::frame()
{
    m_wolfInstance->getInstanceMeshRenderer()->activateCameraForThisFrame(0, 0);

    m_wolfInstance->getCameraList().addCameraForThisFrame(m_mainCamera.get(), 0);
    m_wolfInstance->updateBeforeFrame();

    Wolf::ResourceNonOwner<Wolf::InputHandler> inputHandler = m_wolfInstance->getInputHandler();
    m_uniquePass->updateGamepad(inputHandler);

    std::vector<Wolf::ResourceNonOwner<Wolf::CommandRecordBase>> passes;
    passes.push_back(m_uniquePass.createNonOwnerResource<Wolf::CommandRecordBase>());
    uint32_t swapChainImageIdx = m_wolfInstance->acquireNextSwapChainImage();
    m_wolfInstance->frame(passes, m_uniquePass->getSemaphore(swapChainImageIdx), swapChainImageIdx);
}

void SystemManager::createWolfInstance(
#ifdef __ANDROID__
    android_app* androidApp
#endif
)
{
    Wolf::WolfInstanceCreateInfo wolfInstanceCreateInfo;
    wolfInstanceCreateInfo.m_configFilename = "";
    wolfInstanceCreateInfo.m_debugCallback = [this](Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message) { debugCallback(severity, type, message); };
#ifdef __ANDROID__
    wolfInstanceCreateInfo.m_androidApp = androidApp;
    wolfInstanceCreateInfo.m_androidWindow = androidApp->window;
    wolfInstanceCreateInfo.m_assetManager = androidApp->activity->assetManager;
#endif
    wolfInstanceCreateInfo.m_useMaterialGPUManager = true;

    wolfInstanceCreateInfo.m_meshBufferPoolSizes.resize(2);

    wolfInstanceCreateInfo.m_meshBufferPoolSizes[0].m_itemSize = sizeof(uint32_t); // mesh indices
    wolfInstanceCreateInfo.m_meshBufferPoolSizes[0].m_minimumPoolSize = 268'435'456;
    wolfInstanceCreateInfo.m_meshBufferPoolSizes[0].m_bufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    wolfInstanceCreateInfo.m_meshBufferPoolSizes[1].m_itemSize = sizeof(Vertex3D); // mesh vertices
    wolfInstanceCreateInfo.m_meshBufferPoolSizes[1].m_minimumPoolSize = 536'870'912;
    wolfInstanceCreateInfo.m_meshBufferPoolSizes[1].m_bufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

#ifndef __ANDROID__
    wolfInstanceCreateInfo.m_configFilename = "config.ini";
#endif

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

#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "WolfEngine", "%s", logMessage.c_str());
#else
    std::cout << logMessage << std::endl;
#endif
}
