#include "debugui.h"
#include "debugui_renderer_opengl.h"
#include "debugui_renderer_vulkan.h"

#include <common/decaf_assert.h>

namespace debugui
{

OpenGLRenderer *
createOpenGLRenderer(const std::string &configPath)
{
#ifdef DECAF_GL
   return new RendererOpenGL { configPath };
#else
   decaf_abort("libdecaf was built with OpenGL support disabled");
#endif // ifdef DECAF_GL
}

VulkanRenderer *
createVulkanRenderer(const std::string &configPath, VulkanRendererInfo &info)
{
#ifdef DECAF_VULKAN
   return new RendererVulkan { configPath, info };
#else
   decaf_abort("libdecaf was built with Vulkan support disabled");
#endif // ifdef DECAF_VULKAN
}

} // namespace debugui
