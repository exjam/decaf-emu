#pragma once
#include <libgpu/gpu7_tiling.h>

namespace gpu7::tiling::vulkan
{

void
untileImage(const SurfaceInfo &surface,
            void *src,
            void *dst);

} // namespace gpu7::tiling::vulkan
