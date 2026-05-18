#pragma once

#include <ResourceUniqueOwner.h>
#include <WolfEngine.h>

#include "UniquePass.h"

class SystemManager
{
public:
    SystemManager(
#ifdef __ANDROID__
        android_app* androidApp
#endif
    );

    void frame();

private:
    void createWolfInstance(
#ifdef __ANDROID__
        android_app* androidApp
#endif
    );
    void debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message);

    Wolf::ResourceUniqueOwner<Wolf::WolfEngine> m_wolfInstance;
    std::unique_ptr<Wolf::FirstPersonCamera> m_mainCamera;

    Wolf::ResourceUniqueOwner<UniquePass> m_uniquePass;

    Wolf::NullableResourceNonOwner<Wolf::BufferPoolInterface> m_bufferPoolInterface;

    struct MeshInfo
    {
        std::vector<Wolf::ResourceUniqueOwner<Wolf::Mesh>> m_lods;

        struct InstanceInfo
        {
            uint32_t m_materialIdx;
            glm::mat4 m_transform;
        };
        std::vector<InstanceInfo> m_instances;
    };
    std::vector<MeshInfo> m_meshes;
    std::vector<Wolf::ResourceUniqueOwner<Wolf::Image>> m_images;
};