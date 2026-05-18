#pragma once

#include <glm/glm.hpp>
#include <vector>

#include <VertexInputs.h>

struct Vertex3D
{
    glm::vec3 pos;
    glm::vec2 texCoord;

    static void getBindingDescription(Wolf::VertexInputBindingDescription& bindingDescription, uint32_t binding)
    {
        bindingDescription.binding = binding;
        bindingDescription.stride = sizeof(Vertex3D);
        bindingDescription.inputRate = Wolf::VertexInputRate::VERTEX;
    }

    static void getAttributeDescriptions(std::vector<Wolf::VertexInputAttributeDescription>& attributeDescriptions, uint32_t binding)
    {
        const uint32_t attributeDescriptionCountBefore = static_cast<uint32_t>(attributeDescriptions.size());
        attributeDescriptions.resize(attributeDescriptionCountBefore + 2);

        attributeDescriptions[attributeDescriptionCountBefore + 0].binding = binding;
        attributeDescriptions[attributeDescriptionCountBefore + 0].location = 0;
        attributeDescriptions[attributeDescriptionCountBefore + 0].format = Wolf::Format::R32G32B32_SFLOAT;
        attributeDescriptions[attributeDescriptionCountBefore + 0].offset = offsetof(Vertex3D, pos);

        attributeDescriptions[attributeDescriptionCountBefore + 1].binding = binding;
        attributeDescriptions[attributeDescriptionCountBefore + 1].location = 1;
        attributeDescriptions[attributeDescriptionCountBefore + 1].format = Wolf::Format::R32G32_SFLOAT;
        attributeDescriptions[attributeDescriptionCountBefore + 1].offset = offsetof(Vertex3D, texCoord);
    }

    bool operator==(const Vertex3D& other) const
    {
        return pos == other.pos && texCoord == other.texCoord;
    }
};