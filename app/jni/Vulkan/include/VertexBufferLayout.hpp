#pragma once

#include <stdexcept>
#include <vector>

#include "lib/DataType.hpp"

namespace vulkan {
    struct VertexAttribute {
        unsigned int binding_index;
        DataType type;
        size_t count;
    };

    class VertexBufferLayout {
    private:
        std::vector<VertexAttribute> elements_{};
    public:
        VertexBufferLayout() = default;

        void Push(VertexAttribute attribute);

        [[nodiscard]] size_t GetElementSize() const;

        [[nodiscard]] const std::vector<VertexAttribute> &GetElements() const;
    };
}
