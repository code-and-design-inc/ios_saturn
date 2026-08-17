

#define VK_USE_PLATFORM_WIN32_KHR 1
#include <windows.h>
#include "..\vulkan\Window.h"
#include "QYabVulkanWidget.h"
#include "vulkan/VIDVulkan.h"
#include "vulkan/VIDVulkanCInterface.h"
#include "VolatileSettings.h"
#include "QtYabause.h"
#include <YabauseThread.h>
#include <QResizeEvent>

extern Renderer* _vulkanRenderer;
QYabVulkanWidget* QYabVulkanWidget::_instance = nullptr;

QYabVulkanWidget::QYabVulkanWidget(QWidget *parent) : QWidget(parent) {
  _instance = this;
  pYabauseThread = nullptr;
  _vulkanRenderer = nullptr;

  // The Vulkan renderer presents straight into this widget's own window, so Qt
  // must not paint it at all. Without WA_PaintOnScreen (and a null paint
  // engine) Qt keeps a backingstore for the widget and flushes it on every
  // repaint; paintEvent() draws nothing and immediately schedules another
  // update, so those empty black flushes alternate with the Vulkan present and
  // the game flickers.
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setAttribute(Qt::WA_NoSystemBackground);

}

// Required by WA_PaintOnScreen: tells Qt this widget has no paint device of
// its own, so it never composes or clears the area behind the renderer.
QPaintEngine* QYabVulkanWidget::paintEngine() const {
  return nullptr;
}

QYabVulkanWidget::~QYabVulkanWidget() {
}

void QYabVulkanWidget::ready() {
  // Size the Vulkan surface to this widget, not to Video/WinWidth and
  // Video/WinHeight. Nothing ever writes those keys, so they always fell back
  // to 800x600: on a window larger than that the renderer presented into only
  // part of the widget and the rest kept showing whatever the previous page
  // (the game browser) had painted there.
  const int width = qMax(1, this->width());
  const int height = qMax(1, this->height());
  auto w = _vulkanRenderer->OpenWindow(width, height, "", nullptr);
  VIDVulkan::getInstance()->setRenderer(_vulkanRenderer);
}

void QYabVulkanWidget::paintEvent(QPaintEvent* event) {
  if (pYabauseThread) pYabauseThread->execEmulation();
  update();
}

void QYabVulkanWidget::resizeEvent(QResizeEvent* event) {

  if (pYabauseThread) pYabauseThread->resize(event->size().width(), event->size().height());
  QWidget::resizeEvent(event);
}

void QYabVulkanWidget::updateView(const QSize& s) {

    const QSize size = s.isValid() ? s : this->size();
    int viewport_width_ = size.width();
    int viewport_height_ = size.height();
    if (pYabauseThread) pYabauseThread->resize(viewport_width_, viewport_height_);
}


void InitPlatform()
{

}

void DeInitPlatform()
{

}

void AddRequiredPlatformInstanceExtensions(std::vector<const char*>* instance_extensions) {
  instance_extensions->push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
  //instance_extensions->push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  //instance_extensions->push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

  //instance_extensions->push_back(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
}

void AddRequiredPlatformDeviceExtensions(std::vector<const char*>* device_extensions) {
  //device_extensions->push_back(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME);
}

void Window::_InitOSWindow() {

}

void Window::_DeInitOSWindow() {

}

void Window::_UpdateOSWindow() {

}

void Window::_InitOSSurface() {

  HWND hwnd = reinterpret_cast<HWND>(QYabVulkanWidget::getInstance()->winId());
  VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
  surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
  surfaceCreateInfo.hwnd = hwnd;

  VkSurfaceKHR surface;
  VkResult result = vkCreateWin32SurfaceKHR(_vulkanRenderer->GetVulkanInstance(), &surfaceCreateInfo, nullptr, &surface);
  if (result != VK_SUCCESS) {
    qFatal("Failed to create Vulkan surface: %d", result);
  }

  this->_surface = surface;
  if (_surface == VK_NULL_HANDLE) {
    qFatal("Failed to retrieve Vulkan surface");
  }
}
