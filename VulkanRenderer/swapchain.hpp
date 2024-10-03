#pragma once
#ifndef SWAPCHAIN_HH
#define SWAPCHAIN_HH

#define VK_USE_PLATFORM_WIN32_KHR
#include "device.hpp"
#include <vulkan/vulkan.hpp>

#include "windef.h" // For HWND

struct ViewportInfo {
  // int width;
  // int height;
  bool isResized = false;
  HWND hwnd;
  HINSTANCE hinstance;
  vk::SurfaceKHR surface;
  vk::SurfaceFormatKHR surfaceFormat;
  vk::Extent2D surfaceExtent;
  // Swapchain swapchain;

  std::vector<const char *> extensions;
};

struct SurfaceSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
};

class Swapchain {
public:
  operator vk::SwapchainKHR &() { return swapchain; }

  vk::SwapchainKHR *operator->() { return &swapchain; }

  Swapchain(Device *device_, ViewportInfo *info) : device(device_), viewportInfo(info)
  {
    createSwapchain();
    createImageViews();
  }

  Swapchain() {}
  ~Swapchain()
  {
    std::cout << "Destroying swapchain." << std::endl;
    cleanupSwapchain();
  }

  const ViewportInfo &getViewportInfo() { return *viewportInfo; }
  const vk::Extent2D &getExtent() { return viewportInfo->surfaceExtent; }

  void createFramebuffers(vk::RenderPass &renderPass)
  {
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
      vk::ImageView attachments[] = { swapchainImageViews[i] };

      vk::FramebufferCreateInfo framebufferInfo = {};
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = attachments;
      framebufferInfo.width = viewportInfo->surfaceExtent.width;
      framebufferInfo.height = viewportInfo->surfaceExtent.height;
      framebufferInfo.layers = 1;

      try {
        swapchainFramebuffers[i] = (*device)->createFramebuffer(framebufferInfo);
      }
      catch (vk::SystemError) {
        throw std::runtime_error("failed to create framebuffer!");
      }
    }
  }


  Device *device;
  ViewportInfo *viewportInfo;

  vk::SwapchainKHR swapchain;
  std::vector<vk::Image> swapchainImages;
  std::vector<vk::ImageView> swapchainImageViews;
  std::vector<vk::Framebuffer> swapchainFramebuffers;

  // vk::Extent2D swapchainExtent;
  // vk::Format swapchainImageFormat;

private:
  void createSwapchain()
  {
    const vk::SurfaceKHR &surface = viewportInfo->surface;

    SurfaceSupportDetails swapchainSupport = querySurfaceSupport(surface);

    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eMailbox;
    viewportInfo->surfaceFormat = { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
    viewportInfo->surfaceExtent = swapchainSupport.capabilities.currentExtent;

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
      imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo(
        vk::SwapchainCreateFlagsKHR(),
        surface,
        imageCount,
        viewportInfo->surfaceFormat.format,
        viewportInfo->surfaceFormat.colorSpace,
        viewportInfo->surfaceExtent,
        1, // imageArrayLayers
        vk::ImageUsageFlagBits::eColorAttachment
    );

    QueueFamilyIndices indices = device->findQueueFamilies();
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
      createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
      createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = vk::SwapchainKHR(nullptr);

    try {
      swapchain = (*device)->createSwapchainKHR(createInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create swap chain!");
    }

    swapchainImages = (*device)->getSwapchainImagesKHR(swapchain);

    std::cout << "Created swapchain with extent " << viewportInfo->surfaceExtent.width << " "
              << viewportInfo->surfaceExtent.height << std::endl;
  }

  void createImageViews()
  {
    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); i++) {
      vk::ImageViewCreateInfo createInfo = {};
      createInfo.image = swapchainImages[i];
      createInfo.viewType = vk::ImageViewType::e2D;
      createInfo.format = viewportInfo->surfaceFormat.format;
      createInfo.components.r = vk::ComponentSwizzle::eIdentity;
      createInfo.components.g = vk::ComponentSwizzle::eIdentity;
      createInfo.components.b = vk::ComponentSwizzle::eIdentity;
      createInfo.components.a = vk::ComponentSwizzle::eIdentity;
      createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      try {
        swapchainImageViews[i] = (*device)->createImageView(createInfo);
      }
      catch (vk::SystemError) {
        throw std::runtime_error("failed to create image views!");
      }
    }
  }

  SurfaceSupportDetails querySurfaceSupport(const vk::SurfaceKHR &surface)
  {
    vk::PhysicalDevice &physicalDevice = *device->getPhysicalDevice();
    SurfaceSupportDetails details;
    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    details.formats = physicalDevice.getSurfaceFormatsKHR(surface);
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    return details;
  }

  void cleanupSwapchain()
  {
    Device &d = *device;
    for (auto framebuffer : swapchainFramebuffers) {
      d->destroyFramebuffer(framebuffer);
    }

    // d->freeCommandBuffers(commandPool, commandBuffers);

    // d->destroyPipeline(graphicsPipeline);
    // d->destroyPipelineLayout(pipelineLayout);
    // d->destroyRenderPass(renderPass);

    for (auto imageView : swapchainImageViews) {
      d->destroyImageView(imageView);
    }

    d->destroySwapchainKHR(swapchain);
  }
};

#endif
