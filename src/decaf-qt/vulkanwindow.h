#pragma once
#include <vulkan/vulkan.hpp>

#include <QWindow>
#include <QVulkanInstance>
#include <thread>

#include "decafinterface.h"

class DecafInterface;

namespace gpu
{
class VulkanDriver;
}
namespace debugui
{
class VulkanRenderer;
}

class QVulkanFunctions;
class QVulkanInstance;

struct Viewport
{
   float x;
   float y;
   float width;
   float height;
};

#include "qvulkanwindow2.h"

class VulkanRenderer : public QObject, public QVulkanWindowRenderer
{
   Q_OBJECT
public:
   VulkanRenderer(QVulkanWindow2 *w, DecafInterface *decafInterface);

   void initResources() override;
   void initSwapChainResources() override;
   void releaseSwapChainResources() override;
   void releaseResources() override;

   void startNextFrame() override;

   void keyPressEvent(QKeyEvent *);
   void keyReleaseEvent(QKeyEvent *);
   void mousePressEvent(QMouseEvent *);
   void mouseReleaseEvent(QMouseEvent *);
   void mouseMoveEvent(QMouseEvent *);
   void wheelEvent(QWheelEvent *);
   void touchEvent(QTouchEvent *);

public slots:
   void titleLoaded(quint64 id, const QString &name);

private:
   bool createRenderPipeline();
   void calculateScreenViewports(Viewport &tv, Viewport &drc);
   void renderFrame(Viewport &tv, Viewport &drc);
   void acquireScanBuffer(vk::CommandBuffer cmdBuffer, vk::DescriptorSet descriptorSet, vk::Image image, vk::ImageView imageView);
   void renderScanBuffer(vk::Viewport viewport, vk::CommandBuffer cmdBuffer, vk::DescriptorSet descriptorSet, vk::Image image, vk::ImageView imageView);
   void releaseScanBuffer(vk::CommandBuffer cmdBuffer, vk::DescriptorSet descriptorSet, vk::Image image, vk::ImageView imageView);

private:
   DecafInterface *mDecafInterface;
   QVulkanWindow2 *m_window;
   QVulkanDeviceFunctions *m_devFuncs;

   vk::Device mDevice;

   // Resources which exist between initResources and releaseResources
   vk::Buffer mVertexBuffer;
   vk::Sampler mTrivialSampler;
   vk::DescriptorSetLayout mDescriptorSetLayout;
   vk::PipelineLayout mPipelineLayout;
   vk::DescriptorPool mDescriptorPool;

   // Resources which exist between initSwapChainResources and releaseSwapChainResources
   vk::Pipeline mGraphicsPipeline;

   std::thread mGraphicsThread;

   struct FrameResources
   {
      vk::DescriptorSet tvDesc;
      vk::DescriptorSet drcDesc;
   };
   int mFrameIndex = 0;
   std::array<FrameResources, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT> mFrameResources;

   bool mGameLoaded = false;
   debugui::VulkanRenderer *mDebugUi;
   gpu::VulkanDriver* mDecafDriver;
};

class VulkanWindow : public QVulkanWindow2
{
public:
   VulkanWindow(QVulkanInstance *instance, DecafInterface *decafInterface);
   QVulkanWindowRenderer *createRenderer() override;

   void keyPressEvent(QKeyEvent *) override;
   void keyReleaseEvent(QKeyEvent *) override;
   void mousePressEvent(QMouseEvent *) override;
   void mouseReleaseEvent(QMouseEvent *) override;
   void mouseMoveEvent(QMouseEvent *) override;
   void wheelEvent(QWheelEvent *) override;
   void touchEvent(QTouchEvent *) override;

private:
   DecafInterface *mDecafInterface;
   VulkanRenderer *mRenderer;
};
