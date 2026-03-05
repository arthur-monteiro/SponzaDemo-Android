#include "UniquePass.h"

#include <filesystem>
#include <fstream>

#include <glm/gtc/matrix_transform.hpp>

#include <Attachment.h>
#include <DescriptorSetLayout.h>
#include <DescriptorSetLayoutGenerator.h>
#include <Image.h>
#include <FrameBuffer.h>
#include <GPUSemaphore.h>
#include <UniformBuffer.h>

#include "Vertex2D.h"
#include "Vertex3D.h"

using namespace Wolf;

void UniquePass::initializeResources(const InitializationContext& context)
{
    const Wolf::Format outputFormat = context.swapChainFormat;

    createDepthImage(context);

    Attachment depth({ context.swapChainWidth, context.swapChainHeight }, context.depthFormat, SAMPLE_COUNT_1, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, AttachmentStoreOp::DONT_CARE, Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
                     m_depthImage->getDefaultImageView());
    Attachment color({ context.swapChainWidth, context.swapChainHeight }, outputFormat, SAMPLE_COUNT_1, ImageLayout::PRESENT_SRC_KHR, AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT,
                     IMAGE_VIEW_NULL);

    m_renderPass.reset(RenderPass::createRenderPass({ depth, color }));

    m_commandBuffer.reset(CommandBuffer::createCommandBuffer(QueueType::GRAPHIC, false /* isTransient */));

    m_frameBuffers.resize(context.swapChainImageCount);
    for (uint32_t i = 0; i < context.swapChainImageCount; ++i)
    {
        color.imageView = context.swapChainImages[i]->getDefaultImageView();
        m_frameBuffers[i].reset(FrameBuffer::createFrameBuffer(*m_renderPass, { depth, color }));
    }

    createSemaphores(context, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, true);

    createSceneRenderPipeline(context.swapChainWidth, context.swapChainHeight);

    m_gamepadVertexShaderParser.reset(new ShaderParser("shaders/gamepad/shader.vert"));
    m_gamepadFragmentShaderParser.reset(new ShaderParser("shaders/gamepad/shader.frag"));

    // Gamepad render
    {
        Wolf::DescriptorSetLayoutGenerator descriptorSetLayoutGenerator;
        descriptorSetLayoutGenerator.addUniformBuffer(ShaderStageFlagBits::VERTEX | ShaderStageFlagBits::FRAGMENT, 0);
        m_gamepadDescriptorSetLayout.reset(Wolf::DescriptorSetLayout::createDescriptorSetLayout(descriptorSetLayoutGenerator.getDescriptorLayouts()));

        for (uint32_t i = 0; i < m_gamepadRenderInfo.size(); ++i)
        {
            m_gamepadRenderInfo[i].reset(new GamepadRenderInfo(descriptorSetLayoutGenerator, m_gamepadDescriptorSetLayout.createNonOwnerResource()));
        }

        std::vector<Vertex2D> vertices =
        {
            { glm::vec2(-1.0f, -1.0f) }, // top left
            { glm::vec2(1.0f, -1.0f) }, // top right
            { glm::vec2(-1.0f, 1.0f) }, // bot left
            { glm::vec2(1.0f, 1.0f) } // bot right
        };

        std::vector<uint32_t> indices =
        {
            0, 2, 3,
            1, 0, 3
        };

        m_squareVertexBuffer.reset(Buffer::createBuffer(sizeof(Vertex2D) * vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        m_squareVertexBuffer->transferCPUMemoryWithStagingBuffer(vertices.data(), sizeof(Vertex2D) * vertices.size());

        m_squareIndexBuffer.reset(Buffer::createBuffer(sizeof(uint32_t) * indices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        m_squareIndexBuffer->transferCPUMemoryWithStagingBuffer(indices.data(), sizeof(uint32_t) * indices.size());
    }

    createGamepadRenderPipeline(context.swapChainWidth, context.swapChainHeight);
}

void UniquePass::resize(const InitializationContext& context)
{
    createDepthImage(context);
    m_renderPass->setExtent({ context.swapChainWidth, context.swapChainHeight });

    m_frameBuffers.clear();
    m_frameBuffers.resize(context.swapChainImageCount);

    Attachment depth({ context.swapChainWidth, context.swapChainHeight }, context.depthFormat, SAMPLE_COUNT_1, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, AttachmentStoreOp::DONT_CARE, Wolf::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
                     m_depthImage->getDefaultImageView());
    Attachment color({ context.swapChainWidth, context.swapChainHeight }, context.swapChainFormat, SAMPLE_COUNT_1, ImageLayout::PRESENT_SRC_KHR, AttachmentStoreOp::STORE, Wolf::ImageUsageFlagBits::COLOR_ATTACHMENT,
                     IMAGE_VIEW_NULL);

    for (uint32_t i = 0; i < context.swapChainImageCount; ++i)
    {
        color.imageView = context.swapChainImages[i]->getDefaultImageView();
        m_frameBuffers[i].reset(FrameBuffer::createFrameBuffer(*m_renderPass, { depth, color }));
    }

    createSceneRenderPipeline(context.swapChainWidth, context.swapChainHeight);
    createGamepadRenderPipeline(context.swapChainWidth, context.swapChainHeight);
}

void UniquePass::record(const RecordContext& context)
{
    float aspect = (float)context.m_swapchainImage->getExtent().height / (float)context.m_swapchainImage->getExtent().width;

    const uint32_t frameBufferIdx = context.m_swapChainImageIdx;

    for (uint32_t i = 0; i < m_gamepadRenderInfo.size(); ++i)
    {
        GamepadRenderUniformData gamepadRenderUniformData{};
        gamepadRenderUniformData.m_scale = glm::vec2(1.0f, 1.0 / aspect);

        if (i % 2 == 1)
        {
            gamepadRenderUniformData.m_scale *= 0.15f;

            glm::vec2 joystickPos = m_joysticksPos[i / 2];
            if (context.m_swapchainRotation == 90.0f)
            {
                joystickPos.y = 1.0f - joystickPos.y;
            }
            else
            {
                joystickPos.x = 1.0f - joystickPos.x;
            }
            gamepadRenderUniformData.m_offset = glm::vec2(joystickPos.y, joystickPos.x) * 2.0f - glm::vec2(1.0f);

            gamepadRenderUniformData.m_color = glm::vec4(1.0f, 1.0f, 1.0f, m_joysticksOpacity[i / 2]);
        }
        else
        {
            gamepadRenderUniformData.m_scale *= 0.25f;

            glm::vec2 joystickCenter = m_joysticksCenter[i / 2];
            if (context.m_swapchainRotation == 90.0f)
            {
                joystickCenter.y = 1.0f - joystickCenter.y;
            }
            else
            {
                joystickCenter.x = 1.0f - joystickCenter.x;
            }
            gamepadRenderUniformData.m_offset = glm::vec2(joystickCenter.y, joystickCenter.x) * 2.0f - glm::vec2(1.0f);

            gamepadRenderUniformData.m_color = glm::vec4(0.5f, 0.5f, 0.5f, m_joysticksOpacity[i / 2]);
        }

        m_gamepadRenderInfo[i]->updateUniformBuffer(gamepadRenderUniformData);
    }

    m_commandBuffer->beginCommandBuffer();

    std::vector<Wolf::ClearValue> clearValues(2);
    clearValues[0] = {{{1.0f}}};
    clearValues[1] = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
    m_commandBuffer->beginRenderPass(*m_renderPass, *m_frameBuffers[frameBufferIdx], clearValues);

    // Scene
    context.m_renderMeshList->draw(context, *m_commandBuffer, m_renderPass.get(), 0, 0, {}, {});

    // Gamepad
    m_commandBuffer->bindPipeline(m_gamepadRenderPipeline.createConstNonOwnerResource());
    m_commandBuffer->bindVertexBuffer(*m_squareVertexBuffer);
    m_commandBuffer->bindIndexBuffer(*m_squareIndexBuffer, IndexType::U32);

    for (uint32_t i = 0; i < m_gamepadRenderInfo.size(); ++i)
    {
        if (m_joysticksOpacity[i / 2] > 0.0f)
        {
            m_commandBuffer->bindDescriptorSet(m_gamepadRenderInfo[i]->getDescriptorSet(), 0, *m_gamepadRenderPipeline);
            m_commandBuffer->drawIndexed(6, 1, 0, 0, 0);
        }
    }

    m_commandBuffer->endRenderPass();

    m_commandBuffer->endCommandBuffer();
}

void UniquePass::submit(const SubmitContext& context)
{
    const std::vector<const Semaphore*> waitSemaphores{ context.swapChainImageAvailableSemaphore };
    const std::vector<const Semaphore*> signalSemaphores{ getSemaphore(context.swapChainImageIndex) };
    m_commandBuffer->submit(waitSemaphores, signalSemaphores, context.frameFence);
}

void UniquePass::createDepthImage(const InitializationContext& context)
{
    CreateImageInfo depthImageCreateInfo;
    depthImageCreateInfo.format = context.depthFormat;
    depthImageCreateInfo.extent.width = context.swapChainWidth;
    depthImageCreateInfo.extent.height = context.swapChainHeight;
    depthImageCreateInfo.extent.depth = 1;
    depthImageCreateInfo.mipLevelCount = 1;
    depthImageCreateInfo.aspectFlags = ImageAspectFlagBits::DEPTH;
    depthImageCreateInfo.usage = DEPTH_STENCIL_ATTACHMENT;
    m_depthImage.reset(Image::createImage(depthImageCreateInfo));
}

void UniquePass::createSceneRenderPipeline(uint32_t width, uint32_t height)
{
    m_pipelineSet.reset(new Wolf::PipelineSet);

    Wolf::PipelineSet::PipelineInfo pipelineInfo;

    // Shaders
    pipelineInfo.shaderInfos.resize(2);
    pipelineInfo.shaderInfos[0].shaderFilename = "shaders/sceneRender/shader.vert";
    pipelineInfo.shaderInfos[0].stage = Wolf::ShaderStageFlagBits::VERTEX;
    pipelineInfo.shaderInfos[1].shaderFilename = "shaders/sceneRender/shader.frag";
    pipelineInfo.shaderInfos[1].stage = Wolf::ShaderStageFlagBits::FRAGMENT;

    // IA
    Vertex3D::getAttributeDescriptions(pipelineInfo.vertexInputAttributeDescriptions, 0);

    pipelineInfo.vertexInputBindingDescriptions.resize(1);
    Vertex3D::getBindingDescription(pipelineInfo.vertexInputBindingDescriptions[0], 0);

    // Color Blend
    pipelineInfo.blendModes = { Wolf::RenderingPipelineCreateInfo::BLEND_MODE::OPAQUE };

    // Resources
    pipelineInfo.cameraDescriptorSlot = DESCRIPTOR_SET_SLOT_CAMERA;
    pipelineInfo.materialsDescriptorSlot = DESCRIPTOR_SET_SLOT_MATERIAL_MANAGER;
//    pipelineInfo.lightDescriptorSlot = DescriptorSetSlots::DESCRIPTOR_SET_SLOT_LIGHT_INFO;
//    pipelineInfo.customMask = AdditionalDescriptorSetsMaskBits::SHADOW_MASK_INFO | AdditionalDescriptorSetsMaskBits::GLOBAL_IRRADIANCE_SHADOW_MASK_INFO;

    m_pipelineSet->addPipeline(pipelineInfo);

    m_swapChainWidth = width;
    m_swapChainHeight = height;
}

void UniquePass::createGamepadRenderPipeline(uint32_t width, uint32_t height)
{
    RenderingPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.renderPass = m_renderPass.get();

    // Programming stages
    pipelineCreateInfo.shaderCreateInfos.resize(2);
    m_gamepadVertexShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[0].shaderCode);
    pipelineCreateInfo.shaderCreateInfos[0].stage = VERTEX;
    m_gamepadFragmentShaderParser->readCompiledShader(pipelineCreateInfo.shaderCreateInfos[1].shaderCode);
    pipelineCreateInfo.shaderCreateInfos[1].stage = FRAGMENT;

    // IA
    std::vector<Wolf::VertexInputAttributeDescription> attributeDescriptions;
    Vertex2D::getAttributeDescriptions(attributeDescriptions, 0);
    pipelineCreateInfo.vertexInputAttributeDescriptions = attributeDescriptions;

    std::vector<Wolf::VertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0] = {};
    Vertex2D::getBindingDescription(bindingDescriptions[0], 0);
    pipelineCreateInfo.vertexInputBindingDescriptions = bindingDescriptions;

    // Resources
    pipelineCreateInfo.descriptorSetLayouts = { m_gamepadDescriptorSetLayout.createConstNonOwnerResource() };

    // Viewport
    pipelineCreateInfo.extent = { width, height };

    // Depth testing
    pipelineCreateInfo.enableDepthTesting = false;

    // Color Blend
    std::vector blendModes = { RenderingPipelineCreateInfo::BLEND_MODE::TRANS_ALPHA };
    pipelineCreateInfo.blendModes = blendModes;

    m_gamepadRenderPipeline.reset(Pipeline::createRenderingPipeline(pipelineCreateInfo));
}

void UniquePass::updateGamepad(const ResourceNonOwner<Wolf::InputHandler>& inputHandler)
{
    uint32_t frameIdx = inputHandler->getFrameIdx();
    for (uint32_t joystickIdx = 0; joystickIdx < Wolf::InputHandler::GAMEPAD_JOYSTICK_COUNT; ++joystickIdx)
    {
        m_joysticksPos[joystickIdx] = inputHandler->getJoystickPosForVirtualGamepad(joystickIdx);
        m_joysticksCenter[joystickIdx] = inputHandler->getJoystickCenterForVirtualGamepad(joystickIdx);

        uint32_t lastActiveFrameIdx = inputHandler->getLastActiveFrameIdxForVirtualGamepad(joystickIdx);
        if (lastActiveFrameIdx == -1)
        {
            m_joysticksOpacity[joystickIdx] = 0.0f;
        }
        else
        {
            float FADE_FRAME_COUNT = 10;
            float opacity = 1.0f - (glm::clamp(static_cast<float>(frameIdx - lastActiveFrameIdx), 0.0f, FADE_FRAME_COUNT) / FADE_FRAME_COUNT);

            m_joysticksOpacity[joystickIdx] = opacity;
        }
    }
}

UniquePass::GamepadRenderInfo::GamepadRenderInfo(const Wolf::DescriptorSetLayoutGenerator& descriptorSetLayoutGenerator, const Wolf::ResourceNonOwner<Wolf::DescriptorSetLayout>& descriptorSetLayout)
{
    m_gamepadUniformBuffer.reset(new Wolf::UniformBuffer(sizeof(GamepadRenderUniformData)));

    DescriptorSetGenerator descriptorSetGenerator(descriptorSetLayoutGenerator.getDescriptorLayouts());
    descriptorSetGenerator.setUniformBuffer(0, *m_gamepadUniformBuffer);

    m_gamepadDescriptorSet.reset(DescriptorSet::createDescriptorSet(*descriptorSetLayout));
    m_gamepadDescriptorSet->update(descriptorSetGenerator.getDescriptorSetCreateInfo());
}

void UniquePass::GamepadRenderInfo::updateUniformBuffer(const UniquePass::GamepadRenderUniformData& data)
{
    m_gamepadUniformBuffer->transferCPUMemory(&data, sizeof(data));
}
