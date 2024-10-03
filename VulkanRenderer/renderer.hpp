#define GLM_FORCE_RADIANS
#define NOMINMAX
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>

#include "windef.h" // For HWND


#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include "interop.h"
#include "timing.hpp"

#include "debugging.hpp"
#include "device.hpp"
// #include "fps.hh"

const int MAX_FRAMES_IN_FLIGHT = 2;

struct SurfaceInfo {
  int width;
  int height;
  bool isResized = false;
  HWND hwnd;
  HINSTANCE hinstance;

  std::vector<const char *> glfwExtensions;
};


struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static vk::VertexInputBindingDescription getBindingDescription()
  {
    vk::VertexInputBindingDescription bindingDescription {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;

    return bindingDescription;
  }

  static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
  {
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    return attributeDescriptions;
  }
};

const std::vector<Vertex> vertices = {
  {{ -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }},
  { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }},
  {  { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }},
  { { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f }}
};

const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

class Renderer {
public:
  Renderer(DebugCallback debugCallback)
  {
    vdb::externalDebugCallback = debugCallback;
    vdb::debugOutput("External debug callback installed!");
    init();
  }

  Renderer() {
    init();
  }

  ~Renderer() {
    cleanup();
  }

  void init() {
    createInstance();
    vdb::setupDebugCallback(instance);
    m_device = new Device(instance);
    device = &static_cast<vk::Device&>(*m_device);
  }

  void attach(SurfaceInfo &surfaceInfo)
  {
    try {
      m_surfaceInfo = surfaceInfo;
      initVulkan();
      isRunning = true;
      m_thread = std::thread([this]() {
        try {
          mainLoop();
        }
        catch (std::exception &e) {
        vdb::debugOutput(e.what());
          detach();
        }
      });
      vdb::debugOutput("Vulkan Renderer attached!");
    }
    catch (std::exception &e) {
      vdb::debugOutput(e.what());
    }
  }

  void detach()
  {
    isRunning = false;
    m_thread.join();
    cleanupSwapchain();
    vdb::debugOutput("Vulkan Renderer detached!");
  }

  void setMonitorCallback(PerformanceMonitorCallback callback)
  {
    performanceMonitorCallback = callback; // TODO mutex this so it's not called while its being set.
  }

  void setSimpleCallback(SimpleCallback callback) { simpleCallback = callback; }

  // bool framebufferResized = false;
  bool isRunning = false; // This should probably be atomic

private:
  PerformanceMonitorCallback performanceMonitorCallback = nullptr;
  SimpleCallback simpleCallback = nullptr;

  sPerf m_perf;

  // GLFWwindow *window;
  std::thread m_thread;
  SurfaceInfo m_surfaceInfo;

  Device *m_device = nullptr;
  vk::Device *device = nullptr;

  vk::Instance instance;
  vk::SurfaceKHR surface;

  vk::SwapchainKHR swapchain;
  std::vector<vk::Image> swapchainImages;
  vk::Format swapchainImageFormat;
  vk::Extent2D swapchainExtent;
  std::vector<vk::ImageView> swapchainImageViews;
  std::vector<vk::Framebuffer> swapchainFramebuffers;

  vk::RenderPass renderPass;
  vk::DescriptorSetLayout descriptorSetLayout;
  vk::PipelineLayout pipelineLayout;
  vk::Pipeline graphicsPipeline;

  vk::DescriptorPool descriptorPool;
  std::vector<vk::DescriptorSet> descriptorSets;

  vk::Buffer vertexBuffer;
  vk::DeviceMemory vertexBufferMemory;
  vk::Buffer indexBuffer;
  vk::DeviceMemory indexBufferMemory;

  std::vector<vk::Buffer> uniformBuffers;
  std::vector<vk::DeviceMemory> uniformBuffersMemory;
  std::vector<void *> uniformBuffersMapped;

  std::vector<vk::CommandBuffer, std::allocator<vk::CommandBuffer>> commandBuffers;

  std::vector<vk::Semaphore> imageAvailableSemaphores;
  std::vector<vk::Semaphore> renderFinishedSemaphores;
  std::vector<vk::Fence> inFlightFences;
  size_t currentFrame = 0;

  void initVulkan()
  {
    createSurface();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
  }


  // std::chrono::high_resolution_clock::time_point currentTime;
  std::chrono::high_resolution_clock::time_point previousFrameTime = std::chrono::high_resolution_clock::now();
  // #define TARGET_FRAMERATE 30
  void mainLoop()
  {
    Timing<std::chrono::duration<double, std::ratio<1>>> t;
    while (isRunning) {

      // Cap framerate
#ifdef TARGET_FRAMERATE
      std::chrono::high_resolution_clock::time_point currentTime;
      do {
        currentTime = std::chrono::high_resolution_clock::now();
      } while (currentTime - previousFrameTime < std::chrono::milliseconds(1000 / TARGET_FRAMERATE));
      previousFrameTime = currentTime;
#endif

      drawFrame();

      auto delta = t.tock().count();

      m_perf.frameDelta[m_perf.currentIndex] = delta;
      m_perf.currentIndex = (m_perf.currentIndex + 1) % FRAME_DELTA_COUNT;
      try {
        if (performanceMonitorCallback) {
          performanceMonitorCallback(m_perf);
        }
      }
      catch (std::exception &e) {
        vdb::debugOutput(e.what());
      }
    }

    device->waitIdle();
  }

  void cleanupSwapchain()
  {
    for (auto framebuffer : swapchainFramebuffers) {
      device->destroyFramebuffer(framebuffer);
    }

    device->freeCommandBuffers(m_device->commandPool, commandBuffers);

    device->destroyPipeline(graphicsPipeline);
    device->destroyPipelineLayout(pipelineLayout);
    device->destroyRenderPass(renderPass);

    for (auto imageView : swapchainImageViews) {
      device->destroyImageView(imageView);
    }

    device->destroySwapchainKHR(swapchain);
  }

  void cleanup()
  {
    // cleanupSwapchain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      device->destroyBuffer(uniformBuffers[i]);
      device->freeMemory(uniformBuffersMemory[i]);
    }

    device->destroyDescriptorPool(descriptorPool);
    device->destroyDescriptorSetLayout(descriptorSetLayout);

    device->destroyBuffer(vertexBuffer);
    device->freeMemory(vertexBufferMemory);

    device->destroyBuffer(indexBuffer);
    device->freeMemory(indexBufferMemory);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      device->destroySemaphore(renderFinishedSemaphores[i]);
      device->destroySemaphore(imageAvailableSemaphores[i]);
      device->destroyFence(inFlightFences[i]);
    }

    delete m_device;

    instance.destroySurfaceKHR(surface);

    if (vdb::enableValidationLayers) {
      vdb::DestroyDebugUtilsMessengerEXT(instance, vdb::callback, nullptr);
    }


    instance.destroy();
  }

  void recreateSwapchain()
  {
    int width = 0, height = 0;
    // while (width == 0 || height == 0) {
    //   width = m_surfaceInfo.width;
    //   height = m_surfaceInfo.height;
    // }
    std::cout << "Resized. new size: " << width << " " << height << std::endl;

    device->waitIdle();

    cleanupSwapchain();

    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandBuffers();
  }

  void createInstance()
  {
    if (vdb::enableValidationLayers && !vdb::checkValidationLayerSupport()) {
      throw std::runtime_error("validation layers requested, but not available!");
    }

    auto appInfo = vk::ApplicationInfo(
        "Hello Triangle",
        VK_MAKE_VERSION(1, 0, 0),
        "No Engine",
        VK_MAKE_VERSION(1, 0, 0),
        VK_API_VERSION_1_0
    );

    auto extensions = getRequiredExtensions();

    auto createInfo = vk::InstanceCreateInfo(
        vk::InstanceCreateFlags(),
        &appInfo,
        0,
        nullptr, // enabled layers
        static_cast<uint32_t>(extensions.size()),
        extensions.data() // enabled extensions
    );

    if (vdb::enableValidationLayers) {
      createInfo.enabledLayerCount = static_cast<uint32_t>(vdb::validationLayers.size());
      createInfo.ppEnabledLayerNames = vdb::validationLayers.data();
    }

    try {
      instance = vk::createInstance(createInfo, nullptr);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create instance!");
    }
  }

  void createSurface()
  {
    HWND hwnd = m_surfaceInfo.hwnd;
    vk::Win32SurfaceCreateInfoKHR createInfo {};
    createInfo.flags = {};
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd = hwnd;

    try {
      surface = instance.createWin32SurfaceKHR(createInfo, nullptr);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("Failed to create window surface!");
    }
  }

  void createSwapchain()
  {
    SwapchainSupportDetails swapchainSupport = m_device->querySwapchainSupport(surface);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
      imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo(
        vk::SwapchainCreateFlagsKHR(),
        surface,
        imageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        extent,
        1, // imageArrayLayers
        vk::ImageUsageFlagBits::eColorAttachment
    );

    QueueFamilyIndices indices = m_device->findQueueFamilies();
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
      swapchain = device->createSwapchainKHR(createInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create swap chain!");
    }

    swapchainImages = device->getSwapchainImagesKHR(swapchain);

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
    std::cout << "Created swapchain with extent " << extent.width << " " << extent.height << std::endl;
  }

  void createImageViews()
  {
    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); i++) {
      vk::ImageViewCreateInfo createInfo = {};
      createInfo.image = swapchainImages[i];
      createInfo.viewType = vk::ImageViewType::e2D;
      createInfo.format = swapchainImageFormat;
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
        swapchainImageViews[i] = device->createImageView(createInfo);
      }
      catch (vk::SystemError) {
        throw std::runtime_error("failed to create image views!");
      }
    }
  }

  void createRenderPass()
  {
    vk::AttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = vk::SampleCountFlagBits::e1;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
    colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    vk::SubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // dependency.srcAccessMask = 0;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    try {
      renderPass = device->createRenderPass(renderPassInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create render pass!");
    }
  }

  void createDescriptorSetLayout()
  {
    vk::DescriptorSetLayoutBinding uboLayoutBinding {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboLayoutBinding.descriptorCount = 1;

    uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

    vk::DescriptorSetLayoutCreateInfo layoutInfo {};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (device->createDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayout) != vk::Result::eSuccess) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  void createGraphicsPipeline()
  {
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    auto vertShaderModule = createShaderModule(vertShaderCode);
    auto fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo shaderStages[] = {
      {vk::PipelineShaderStageCreateFlags(),   vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main"},
      {vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main"}
    };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchainExtent.width;
    viewport.height = (float)swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor = {};
    scissor.offset = vk::Offset2D { 0, 0 };
    scissor.extent = swapchainExtent;

    vk::PipelineViewportStateCreateInfo viewportState = {};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    try {
      pipelineLayout = device->createPipelineLayout(pipelineLayoutInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create pipeline layout!");
    }

    vk::GraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = nullptr;

    try {
      graphicsPipeline = device->createGraphicsPipeline(nullptr, pipelineInfo).value;
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create graphics pipeline!");
    }
  }

  void createFramebuffers()
  {
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
      vk::ImageView attachments[] = { swapchainImageViews[i] };

      vk::FramebufferCreateInfo framebufferInfo = {};
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = attachments;
      framebufferInfo.width = swapchainExtent.width;
      framebufferInfo.height = swapchainExtent.height;
      framebufferInfo.layers = 1;

      try {
        swapchainFramebuffers[i] = device->createFramebuffer(framebufferInfo);
      }
      catch (vk::SystemError) {
        throw std::runtime_error("failed to create framebuffer!");
      }
    }
  }

  void createDescriptorPool()
  {
    vk::DescriptorPoolSize poolSize {};
    poolSize.type = vk::DescriptorType::eUniformBuffer;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    vk::DescriptorPoolCreateInfo poolInfo {};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (device->createDescriptorPool(&poolInfo, nullptr, &descriptorPool) != vk::Result::eSuccess) {
      throw std::runtime_error("failed to create descriptor pool!");
    }
  }

  void createDescriptorSets()
  {
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo {};
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (device->allocateDescriptorSets(&allocInfo, descriptorSets.data()) != vk::Result::eSuccess) {
      throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo bufferInfo {};
      bufferInfo.buffer = uniformBuffers[i];
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformBufferObject);

      vk::WriteDescriptorSet descriptorWrite {};
      descriptorWrite.dstSet = descriptorSets[i];
      descriptorWrite.dstBinding = 0;
      descriptorWrite.dstArrayElement = 0;

      descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
      descriptorWrite.descriptorCount = 1;

      descriptorWrite.pBufferInfo = &bufferInfo;
      descriptorWrite.pImageInfo = nullptr;
      descriptorWrite.pTexelBufferView = nullptr;

      device->updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
    }
  }

  void createVertexBuffer()
  {
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );

    void *data = device->mapMemory(stagingBufferMemory, 0, bufferSize);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    device->unmapMemory(stagingBufferMemory);

    createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vertexBuffer,
        vertexBufferMemory
    );

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    device->destroyBuffer(stagingBuffer);
    device->freeMemory(stagingBufferMemory);
  }

  // (TODO) this is very similar to createVertexBuffer. The two functions could
  // probably be merged
  void createIndexBuffer()
  {
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        stagingBuffer,
        stagingBufferMemory
    );

    void *data = device->mapMemory(stagingBufferMemory, 0, bufferSize);
    memcpy(data, indices.data(), (size_t)bufferSize);
    device->unmapMemory(stagingBufferMemory);

    createBuffer(
        bufferSize,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        indexBuffer,
        indexBufferMemory
    );

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    device->destroyBuffer(stagingBuffer);
    device->freeMemory(stagingBufferMemory);
  }

  void createUniformBuffers()
  {
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      createBuffer(
          bufferSize,
          vk::BufferUsageFlagBits::eUniformBuffer,
          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
          uniformBuffers[i],
          uniformBuffersMemory[i]
      );

      uniformBuffersMapped[i] = device->mapMemory(uniformBuffersMemory[i], 0, bufferSize);
    }
  }

  void updateUniformBuffer(uint32_t currentImage)
  {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();

    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo {};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.proj =
        glm::perspective(glm::radians(30.0f), swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.0f);

    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }

  void createBuffer(
      vk::DeviceSize size,
      vk::BufferUsageFlags usage,
      vk::MemoryPropertyFlags properties,
      vk::Buffer &buffer,
      vk::DeviceMemory &bufferMemory
  )
  {
    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = vk::SharingMode::eExclusive;

    try {
      buffer = device->createBuffer(bufferInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create buffer!");
    }

    vk::MemoryRequirements memRequirements = device->getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo allocInfo = {};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, properties);

    try {
      bufferMemory = device->allocateMemory(allocInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to allocate buffer memory!");
    }

    device->bindBufferMemory(buffer, bufferMemory, 0);
  }

  void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
  {
    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = m_device->commandPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer commandBuffer = device->allocateCommandBuffers(allocInfo)[0];

    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);

    vk::BufferCopy copyRegion = {};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

    commandBuffer.end();

    vk::SubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    m_device->graphicsQueue.submit(submitInfo, nullptr);
    m_device->graphicsQueue.waitIdle();

    device->freeCommandBuffers(m_device->commandPool, commandBuffer);
  }


  void createCommandBuffers()
  {
    commandBuffers.resize(swapchainFramebuffers.size());

    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.commandPool = m_device->commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    try {
      commandBuffers = device->allocateCommandBuffers(allocInfo);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to allocate command buffers!");
    }

    for (size_t i = 0; i < commandBuffers.size(); i++) {
      vk::CommandBufferBeginInfo beginInfo = {};
      beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

      try {
        commandBuffers[i].begin(beginInfo);
      }
      catch (vk::SystemError) {
        throw std::runtime_error("failed to begin recording command buffer!");
      }

      vk::RenderPassBeginInfo renderPassInfo = {};
      renderPassInfo.renderPass = renderPass;
      renderPassInfo.framebuffer = swapchainFramebuffers[i];
      renderPassInfo.renderArea.offset = vk::Offset2D { 0, 0 };
      renderPassInfo.renderArea.extent = swapchainExtent;

      vk::ClearValue clearColor = {
        std::array<float, 4> {0.0f, 0.0f, 0.0f, 1.0f}
      };
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues = &clearColor;

      commandBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

      commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

      vk::Buffer vertexBuffers[] = { vertexBuffer };
      vk::DeviceSize offsets[] = { 0 };
      commandBuffers[i].bindVertexBuffers(0, 1, vertexBuffers, offsets);

      commandBuffers[i].bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

      commandBuffers[i].bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          pipelineLayout,
          0,
          1,
          &descriptorSets[currentFrame],
          0,
          nullptr
      );

      commandBuffers[i].drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

      commandBuffers[i].endRenderPass();

      try {
        commandBuffers[i].end();
      }
      catch (vk::SystemError) {
        throw std::runtime_error("failed to record command buffer!");
      }
    }
  }

  void createSyncObjects()
  {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    try {
      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        imageAvailableSemaphores[i] = device->createSemaphore({});
        renderFinishedSemaphores[i] = device->createSemaphore({});
        inFlightFences[i] = device->createFence({ vk::FenceCreateFlagBits::eSignaled });
      }
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create synchronization objects for a frame!");
    }
  }

  void drawFrame()
  {
    if (device->waitForFences(1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max()) !=
        vk::Result::eSuccess) {
      throw std::runtime_error("waitForFences timed out!");
    }

    uint32_t imageIndex;
    try {
      vk::ResultValue result = device->acquireNextImageKHR(
          swapchain,
          std::numeric_limits<uint64_t>::max(),
          imageAvailableSemaphores[currentFrame],
          nullptr
      );
      imageIndex = result.value;
    }

    catch (vk::OutOfDateKHRError) {
      recreateSwapchain();
      return;
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    updateUniformBuffer(static_cast<uint32_t>(currentFrame));

    vk::SubmitInfo submitInfo = {};

    vk::Semaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

    vk::Semaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (device->resetFences(1, &inFlightFences[currentFrame]) != vk::Result::eSuccess) {
      throw std::runtime_error("failed to reset fences!");
    }

    try {
      m_device->graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to submit draw command buffer!");
    }

    vk::PresentInfoKHR presentInfo = {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    vk::SwapchainKHR swapchains[] = { swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    vk::Result resultPresent;
    try {
      resultPresent = m_device->presentQueue.presentKHR(presentInfo);
    }
    catch (vk::OutOfDateKHRError) {
      resultPresent = vk::Result::eErrorOutOfDateKHR;
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to present swap chain image!");
    }

    if (resultPresent == vk::Result::eErrorOutOfDateKHR || resultPresent == vk::Result::eSuboptimalKHR ||
        m_surfaceInfo.isResized) {
      m_surfaceInfo.isResized = false;
      recreateSwapchain();
      return;
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  vk::UniqueShaderModule createShaderModule(const std::vector<char> &code)
  {
    try {
      return device->createShaderModuleUnique(
          { vk::ShaderModuleCreateFlags(), code.size(), reinterpret_cast<const uint32_t *>(code.data()) }
      );
    }
    catch (vk::SystemError) {
      throw std::runtime_error("failed to create shader module!");
    }
  }

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
  {
    if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined) {
      return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
    }

    for (const auto &availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Unorm &&
          availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return availableFormat;
      }
    }

    return availableFormats[0];
  }

  vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> availablePresentModes)
  {
    vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;

    for (const auto &availablePresentMode : availablePresentModes) {
      if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
        return availablePresentMode;
      }
      else if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
        bestMode = availablePresentMode;
      }
    }

    return bestMode;
  }

  vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
  {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    else {
      int width, height;
      width = m_surfaceInfo.width;
      height = m_surfaceInfo.height;

      vk::Extent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

      actualExtent.width =
          std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
      actualExtent.height = std::max(
          capabilities.minImageExtent.height,
          std::min(capabilities.maxImageExtent.height, actualExtent.height)
      );

      return actualExtent;
    }
  }


  std::vector<const char *> getRequiredExtensions()
  {

    // std::vector<const char *> extensions = m_surfaceInfo.glfwExtensions;
    std::vector<const char *> extensions = { "VK_KHR_surface", "VK_KHR_win32_surface" };

    if (vdb::enableValidationLayers) {
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
  }


  static std::vector<char> readFile(const std::string &filename)
  {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
  }
};
