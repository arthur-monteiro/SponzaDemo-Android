#pragma once

#include <ResourceUniqueOwner.h>
#include <WolfEngine.h>

#include "UniquePass.h"

class SystemManager
{
public:
    SystemManager(android_app* androidApp);

    void frame();

private:
    void createWolfInstance(android_app* androidApp);
    void debugCallback(Wolf::Debug::Severity severity, Wolf::Debug::Type type, const std::string& message);

    Wolf::ResourceUniqueOwner<Wolf::WolfEngine> m_wolfInstance;
    std::unique_ptr<Wolf::FirstPersonCamera> m_mainCamera;

    Wolf::ResourceUniqueOwner<UniquePass> m_uniquePass;

    Wolf::ResourceUniqueOwner<Wolf::Mesh> m_mesh;
    std::vector<Wolf::ResourceUniqueOwner<Wolf::Image>> m_images;
};