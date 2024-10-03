#pragma once
#ifndef DEVICE_HH
#define DEVICE_HH

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp> // possible bug: VK_USE_PLATFORM_WIN32_KHR not defined here
#include <set>
#include <string>
#include <iostream>
#include <bitset>

#include "debugging.hpp"

const std::vector<const char *> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

const VkPhysicalDeviceFeatures requiredFeatures {
  .multiViewport = VK_TRUE,
};

// These structs feel like they're in the wrong place.
struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapchainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
};

class Device {
public:
  Device(vk::Instance instance_) : instance(instance_) {
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
  }

  ~Device() {
    device.destroyCommandPool(commandPool);
    device.destroy();
  }

  operator vk::Device&() { 
    return device;
  }

  vk::Device* operator->() { return &device; }

  vk::Queue graphicsQueue;
  vk::Queue presentQueue;
  vk::CommandPool commandPool;

private:
  vk::PhysicalDevice physicalDevice;
  vk::Device device;
  vk::Instance instance;


  void pickPhysicalDevice()
  {
    auto devices = instance.enumeratePhysicalDevices();
    if (devices.size() == 0) {
      throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    for (const auto &device : devices) {
      if (isDeviceSuitable(device)) {
        physicalDevice = device;
        break;
      }
    }

    if (!physicalDevice) {
      throw std::runtime_error("failed to find a suitable GPU!");
    }
  }

  void createLogicalDevice()
  {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
      queueCreateInfos.push_back({ vk::DeviceQueueCreateFlags(),
                                   queueFamily,
                                   1, // queueCount
                                   &queuePriority });
    }

    auto deviceFeatures = vk::PhysicalDeviceFeatures(requiredFeatures);
    auto createInfo = vk::DeviceCreateInfo(
        vk::DeviceCreateFlags(),
        static_cast<uint32_t>(queueCreateInfos.size()),
        queueCreateInfos.data()
    );
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vdb::enableValidationLayers) {
      createInfo.enabledLayerCount = static_cast<uint32_t>(vdb::validationLayers.size());
      createInfo.ppEnabledLayerNames = vdb::validationLayers.data();
    }

    try {
      device = physicalDevice.createDevice(createInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create logical device!");
    }

    graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
    presentQueue = device.getQueue(indices.presentFamily.value(), 0);
  }

  void createCommandPool()
  {
    QueueFamilyIndices queueFamilyIndices = Device::findQueueFamilies(physicalDevice);

    vk::CommandPoolCreateInfo poolInfo = {};
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    try {
      commandPool = device.createCommandPool(poolInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create command pool!");
    }
  }

  bool isDeviceSuitable(const vk::PhysicalDevice &device)
  {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool featuresSupported = checkDeviceFeatureSupport(device);

    bool swapchainAdequate = true;
    // if (extensionsSupported) {
    //   SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device, *surface);
    //   swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
    // }

    return indices.isComplete() && extensionsSupported && featuresSupported && swapchainAdequate;
  }

  bool checkDeviceExtensionSupport(const vk::PhysicalDevice &device)
  {
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto &extension : device.enumerateDeviceExtensionProperties()) {
      requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
  }

  bool checkDeviceFeatureSupport(const vk::PhysicalDevice &device)
  {
    const VkPhysicalDeviceFeatures deviceFeatures = device.getFeatures();
    // NOTE: This functions implementation is a bit sketchy. It depends on the structure memory being contiguous and
    // there being no padding.

    // Bitwise compare the features we have to the features we require.
    // We can have extra features, but required features must be present.
    size_t size = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
    const VkBool32 *d = reinterpret_cast<const VkBool32 *>(&deviceFeatures);   // Device feature bytes
    const VkBool32 *r = reinterpret_cast<const VkBool32 *>(&requiredFeatures); // Required feature bytes

    for (size_t i = 0; i < size; i++) {
      std::bitset<sizeof(VkBool32) * 8> bs(d[i]);
      if ((~d[i] & r[i]) != 0) {
        std::cout << "Required feature " << i << " missing!" << std::endl;
        return false;
      }
    }
    return true;
  }

public:

  QueueFamilyIndices findQueueFamilies() { return findQueueFamilies(physicalDevice); }
  QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &physicalDevice)
  {
    QueueFamilyIndices indices;

    auto queueFamilies = physicalDevice.getQueueFamilyProperties();

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
      if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
        indices.graphicsFamily = i;
      }

      if (queueFamily.queueCount > 0 && physicalDevice.getWin32PresentationSupportKHR(i)) {
        indices.presentFamily = i;
      }

      if (indices.isComplete()) {
        break;
      }
      i++;
    }
    return indices;
  }

  SwapchainSupportDetails querySwapchainSupport(const vk::SurfaceKHR &surface)
  {
    SwapchainSupportDetails details;
    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    details.formats = physicalDevice.getSurfaceFormatsKHR(surface);
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    return details;
  }

  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
  {
    // vk::PhysicalDevice &physicalDevice = device;
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
      if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }

    throw std::runtime_error("failed to find suitable memory type!");
  }
};

#endif
