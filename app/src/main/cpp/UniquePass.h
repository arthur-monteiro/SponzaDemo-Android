#pragma once

#include <glm/glm.hpp>
#include <vector>

#include <Buffer.h>
#include <CommandRecordBase.h>
#include <DescriptorSetGenerator.h>
#include <FirstPersonCamera.h>
#include <FrameBuffer.h>
#include <Image.h>
#include <Pipeline.h>
#include <RenderPass.h>
#include <ResourceUniqueOwner.h>
#include <ShaderParser.h>
#include <WolfEngine.h>

class UniquePass : public Wolf::CommandRecordBase
{
public:
    void initializeResources(const Wolf::InitializationContext& context) override;
    void resize(const Wolf::InitializationContext& context) override;
    void record(const Wolf::RecordContext& context) override;
    void submit(const Wolf::SubmitContext& context) override;

    void updateGamepad(const Wolf::ResourceNonOwner<Wolf::InputHandler>& inputHandler);

    Wolf::ResourceNonOwner<const Wolf::PipelineSet> getPipelineSet() { return m_pipelineSet.createConstNonOwnerResource(); }

private:
    void createDepthImage(const Wolf::InitializationContext& context);

    std::unique_ptr<Wolf::RenderPass> m_renderPass;
    std::unique_ptr<Wolf::Image> m_depthImage;
    std::vector<std::unique_ptr<Wolf::FrameBuffer>> m_frameBuffers;

    uint32_t m_swapChainWidth;
    uint32_t m_swapChainHeight;

    void createSceneRenderPipeline(uint32_t width, uint32_t height);

    static constexpr uint32_t DESCRIPTOR_SET_SLOT_CAMERA = 0;
    static constexpr uint32_t DESCRIPTOR_SET_SLOT_MATERIAL_MANAGER = 1;
    Wolf::ResourceUniqueOwner<Wolf::PipelineSet> m_pipelineSet;

    // Virtual gamepad rendering
    void createGamepadRenderPipeline(uint32_t width, uint32_t height);

    std::unique_ptr<Wolf::Buffer> m_squareVertexBuffer;
    std::unique_ptr<Wolf::Buffer> m_squareIndexBuffer;

    Wolf::ResourceUniqueOwner<Wolf::Pipeline> m_gamepadRenderPipeline;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_gamepadVertexShaderParser;
    Wolf::ResourceUniqueOwner<Wolf::ShaderParser> m_gamepadFragmentShaderParser;

    glm::vec2 m_joysticksPos[2];
    glm::vec2 m_joysticksCenter[2];
    float m_joysticksOpacity[2];

    struct GamepadRenderUniformData
    {
        glm::vec2 m_offset;
        glm::vec2 m_scale;

        glm::vec4 m_color;
    };
    class GamepadRenderInfo
    {
    public:
        GamepadRenderInfo(const Wolf::DescriptorSetLayoutGenerator& descriptorSetLayoutGenerator, const Wolf::ResourceNonOwner<Wolf::DescriptorSetLayout>& descriptorSetLayout);

        void updateUniformBuffer(const GamepadRenderUniformData& data);
        Wolf::ResourceNonOwner<const Wolf::DescriptorSet> getDescriptorSet() { return m_gamepadDescriptorSet.createConstNonOwnerResource(); }

    private:
        Wolf::ResourceUniqueOwner<Wolf::UniformBuffer> m_gamepadUniformBuffer;
        Wolf::ResourceUniqueOwner<Wolf::DescriptorSet> m_gamepadDescriptorSet;
    };
    std::array<Wolf::ResourceUniqueOwner<GamepadRenderInfo>, 4> m_gamepadRenderInfo;

    Wolf::ResourceUniqueOwner<Wolf::DescriptorSetLayout> m_gamepadDescriptorSetLayout;
};