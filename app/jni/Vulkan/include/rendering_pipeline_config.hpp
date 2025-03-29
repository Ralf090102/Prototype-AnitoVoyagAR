#pragma once

namespace vulkan {
    enum class CompareOp {
        NEVER,
        LESS,
        EQUAL,
        LESS_OR_EQUAL,
        GREATER,
        NOT_EQUAL,
        GREATER_OR_EQUAL,
        ALWAYS,
        COUNT,
    };
    enum class CullMode {
        NONE,
        FRONT,
        BACK,
        FRONT_AND_BACK,
        COUNT,
    };
    enum class FrontFace {
        CW,
        CCW,
        COUNT,
    };
    enum class DrawMode {
        POINT_LIST,
        LINE_LIST,
        LINE_STRIP,
        TRIANGLE_LIST,
        TRIANGLE_STRIP,
        TRIANGLE_FAN,
        COUNT,
    };
    enum class BlendMode {
        NONE,
        ALPHA,
        ADDITIVE,
        PREMULTIPLIED_ALPHA
    };

    struct RenderingPipelineConfig {
        DrawMode draw_mode = DrawMode::TRIANGLE_STRIP;
        CullMode cull_mode = CullMode::BACK;
        FrontFace front_face = FrontFace::CW;
        bool enable_depth_test = true;
        CompareOp depth_function = CompareOp::LESS;

        BlendMode blend_mode = BlendMode::NONE;
        bool camera_background = false;
    };

}
