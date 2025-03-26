#pragma once
#include "graphics_plugin.hpp"

class TempGraphicsPlugin : public GraphicsPlugin {
public:
    std::vector<std::string> GetOpenXrInstanceExtensions() const override;
    void InitializeDevice(XrInstance instance, XrSystemId system_id) override;
    const XrBaseInStructure* GetGraphicsBinding() const override;
    int64_t SelectSwapchainFormat(const std::vector<int64_t>& runtime_formats) override;
    XrSwapchainImageBaseHeader* AllocateSwapchainImageStructs(uint32_t capacity,
                                                              const XrSwapchainCreateInfo& swapchain_create_info) override;
    void SwapchainImageStructsReady(XrSwapchainImageBaseHeader* images) override;
    void RenderView(const XrCompositionLayerProjectionView& layer_view,
                    XrSwapchainImageBaseHeader* swapchain_images,
                    const uint32_t image_index,
                    const std::vector<math::Transform>& cube_transforms) override;
    void DeinitDevice() override;
};