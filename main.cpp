#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1002000
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include "frag.h"
#include "vert.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>
#include <chrono>

#define HEIGHT 1000
#define WIDTH 1000
#define MAX_FRAMES_IN_FLIGHT 3

class VulkanRenderer {
private:
  uint8_t m_currentFrame = 0;
  GLFWwindow *m_window;
  vk::Instance m_instance;
  vk::PhysicalDevice m_physicalDevice;
  vk::Device m_device;
  VmaAllocator m_allocator;
  vk::Queue m_queue;
  vk::SurfaceKHR m_surface;
  vk::SwapchainKHR m_swapchain;
  std::vector<vk::Image> m_swapchainImages;
  std::vector<vk::ImageView> m_swapchainImageViews;
  vk::RenderPass m_renderPass;
  vk::PipelineLayout m_pipelineLayout;
  vk::Pipeline m_graphicsPipeline;
  std::vector<vk::Framebuffer> m_framebuffers;
  vk::CommandPool m_commandPool;
  vk::CommandBuffer m_commandBuffer[MAX_FRAMES_IN_FLIGHT];
  vk::Semaphore m_imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
  vk::Semaphore m_renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
  vk::Fence m_inFlightFences[MAX_FRAMES_IN_FLIGHT];
  vk::Buffer m_vertexBuffers[MAX_FRAMES_IN_FLIGHT];
  VmaAllocation m_vertexBufferAllocations[MAX_FRAMES_IN_FLIGHT];
  vk::Buffer m_indexBuffers[MAX_FRAMES_IN_FLIGHT];
  VmaAllocation m_indexBufferAllocations[MAX_FRAMES_IN_FLIGHT];
  vk::Buffer m_instanceBuffers[MAX_FRAMES_IN_FLIGHT];
  VmaAllocation m_instanceBufferAllocations[MAX_FRAMES_IN_FLIGHT];
  vk::Buffer m_stagingBuffers[MAX_FRAMES_IN_FLIGHT];
  VmaAllocation m_stagingBufferAllocations[MAX_FRAMES_IN_FLIGHT];
  void *m_mappedStagingBuffers[MAX_FRAMES_IN_FLIGHT];
  float m_vertices[54];
  uint32_t m_indices[78];
  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window =
        glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Window", nullptr, nullptr);
  };
  void createInstance() {
    vk::ApplicationInfo appInfo("VulkanApp", 1, "No Engine", 1,
                                VK_API_VERSION_1_2);
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions,
                                         glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    std::vector<const char *> layers = {"VK_LAYER_KHRONOS_validation"};
    vk::InstanceCreateInfo createInfo(
        vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR, &appInfo,
        layers.size(), layers.data(), extensions.size(), extensions.data());
    m_instance = vk::createInstance(createInfo);
  };
  void createPhysicalDevice() {
    std::vector<vk::PhysicalDevice> devices =
        m_instance.enumeratePhysicalDevices();
    for (const auto &deviceType :
         {vk::PhysicalDeviceType::eDiscreteGpu,
          vk::PhysicalDeviceType::eIntegratedGpu,
          vk::PhysicalDeviceType::eVirtualGpu, vk::PhysicalDeviceType::eCpu}) {
      for (const auto &device : devices) {
        if (device.getProperties().deviceType == deviceType) {
          m_physicalDevice = device;
          break;
        }
      }
    }
  };
  void createLogicalDevice() {
    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo({}, 0, 1, &queuePriority);
    std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};
    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    m_device = m_physicalDevice.createDevice(createInfo);
    m_queue = m_device.getQueue(0, 0);
  };
  void createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    vmaCreateAllocator(&allocatorInfo, &m_allocator);
  };
  void createSurface() {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create window surface");
    }
    m_surface = surface;
  };
  void createSwapchain() {
    vk::SurfaceCapabilitiesKHR surfaceCapabilities =
        m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
    vk::SurfaceFormatKHR surfaceFormat = vk::Format::eB8G8R8A8Srgb;
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    vk::Extent2D extent = vk::Extent2D{WIDTH, HEIGHT};
    vk::SwapchainCreateInfoKHR createInfo(
        {}, m_surface, surfaceCapabilities.minImageCount, surfaceFormat.format,
        surfaceFormat.colorSpace, extent, 1,
        vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive,
        0, nullptr, surfaceCapabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode, VK_TRUE);
    m_swapchain = m_device.createSwapchainKHR(createInfo);
  };
  void createImageViews() {
    m_swapchainImages = m_device.getSwapchainImagesKHR(m_swapchain);
    for (const auto &image : m_swapchainImages) {
      vk::ImageViewCreateInfo createInfo(
          {}, image, vk::ImageViewType::e2D, vk::Format::eB8G8R8A8Srgb,
          vk::ComponentMapping{},
          vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0,
                                    1});
      m_swapchainImageViews.push_back(m_device.createImageView(createInfo));
    }
  };
  void createRenderPass() {
    vk::AttachmentDescription attachmentDescription{};
    attachmentDescription.format = vk::Format::eB8G8R8A8Srgb;
    attachmentDescription.samples = vk::SampleCountFlagBits::e1;
    attachmentDescription.loadOp = vk::AttachmentLoadOp::eClear;
    attachmentDescription.storeOp = vk::AttachmentStoreOp::eStore;
    attachmentDescription.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachmentDescription.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachmentDescription.initialLayout = vk::ImageLayout::eUndefined;
    attachmentDescription.finalLayout = vk::ImageLayout::ePresentSrcKHR;
    vk::AttachmentReference colorAttachmentRef(
        0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::SubpassDescription subpassDescription(
        {}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1,
        &colorAttachmentRef, nullptr, nullptr, 0, nullptr);
    vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, {},
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::DependencyFlagBits::eByRegion);
    vk::RenderPassCreateInfo createInfo({}, 1, &attachmentDescription, 1,
                                        &subpassDescription, 1, &dependency);
    m_renderPass = m_device.createRenderPass(createInfo);
  }
  void createGraphicsPipeline() {
    vk::ShaderModuleCreateInfo vertShaderCreateInfo{};
    vertShaderCreateInfo.codeSize = sizeof(vert_spv);
    vertShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(vert_spv);
    vk::ShaderModule vertShaderModule =
        m_device.createShaderModule(vertShaderCreateInfo);
    vk::ShaderModuleCreateInfo fragShaderCreateInfo{};
    fragShaderCreateInfo.codeSize = sizeof(frag_spv);
    fragShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(frag_spv);
    vk::ShaderModule fragShaderModule =
        m_device.createShaderModule(fragShaderCreateInfo);
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo(
        {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo(
        {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main");
    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};
    vk::VertexInputBindingDescription bindingDescription[2];
    bindingDescription[0].binding = 0;
    bindingDescription[0].stride = sizeof(float) * 2;
    bindingDescription[0].inputRate = vk::VertexInputRate::eVertex;
    bindingDescription[1].binding = 1;
    bindingDescription[1].stride = sizeof(float) * 3;
    bindingDescription[1].inputRate = vk::VertexInputRate::eInstance;
    vk::VertexInputAttributeDescription attributeDescription[2];
    attributeDescription[0].binding = 0;
    attributeDescription[0].location = 0;
    attributeDescription[0].format = vk::Format::eR32G32Sfloat;
    attributeDescription[0].offset = 0;
    attributeDescription[1].binding = 1;
    attributeDescription[1].location = 1;
    attributeDescription[1].format = vk::Format::eR32G32B32Sfloat;
    attributeDescription[1].offset = 0;
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 2;
    vertexInputInfo.pVertexBindingDescriptions = bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescription;
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo(
        {}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);
    vk::Viewport viewport(0.0f, 0.0f, WIDTH, HEIGHT, 0.0f, 1.0f);
    vk::Rect2D scissor({0, 0}, {WIDTH, HEIGHT});
    vk::PipelineViewportStateCreateInfo viewportStateInfo({}, 1, &viewport, 1,
                                                          &scissor);
    vk::PipelineRasterizationStateCreateInfo rasterizerInfo(
        {}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, VK_FALSE,
        0.0f, 0.0f, 0.0f, 1.0f);
    vk::PipelineMultisampleStateCreateInfo multisamplingInfo;
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;
    vk::PipelineColorBlendStateCreateInfo colorBlendingInfo(
        {}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutInfo);
    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportStateInfo;
    pipelineInfo.pRasterizationState = &rasterizerInfo;
    pipelineInfo.pMultisampleState = &multisamplingInfo;
    pipelineInfo.pColorBlendState = &colorBlendingInfo;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    m_graphicsPipeline =
        m_device.createGraphicsPipeline({}, pipelineInfo).value;
    m_device.destroyShaderModule(vertShaderModule);
    m_device.destroyShaderModule(fragShaderModule);
  };
  void createFramebuffers() {
    for (const auto &imageView : m_swapchainImageViews) {
      vk::FramebufferCreateInfo createInfo({}, m_renderPass, 1, &imageView,
                                           WIDTH, HEIGHT, 1);
      m_framebuffers.push_back(m_device.createFramebuffer(createInfo));
    }
  };
  void createCommandPool() {
    vk::CommandPoolCreateInfo createInfo{};
    createInfo.queueFamilyIndex = 0;
    createInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    m_commandPool = m_device.createCommandPool(createInfo);
  };
  void createCommandBuffers() {
    vk::CommandBufferAllocateInfo allocateInfo(
        m_commandPool, vk::CommandBufferLevel::ePrimary, MAX_FRAMES_IN_FLIGHT);
    std::vector<vk::CommandBuffer> buffers =
        m_device.allocateCommandBuffers(allocateInfo);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      m_commandBuffer[i] = buffers[i];
    }
  };
  void createSyncObjects() {
    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{vk::FenceCreateFlagBits::eSignaled};
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      m_imageAvailableSemaphores[i] = m_device.createSemaphore(semaphoreInfo);
      m_renderFinishedSemaphores[i] = m_device.createSemaphore(semaphoreInfo);
      m_inFlightFences[i] = m_device.createFence(fenceInfo);
    }
  };
  void createVertexBuffers() {
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = sizeof(float) * 54;
    bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer |
                       vk::BufferUsageFlagBits::eTransferDst;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vmaCreateBuffer(
          m_allocator, reinterpret_cast<VkBufferCreateInfo *>(&bufferInfo),
          &allocInfo, reinterpret_cast<VkBuffer *>(&m_vertexBuffers[i]),
          &m_vertexBufferAllocations[i], nullptr);
    }
  };
  void createIndexBuffers() {
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = sizeof(uint32_t) * 78;
    bufferInfo.usage = vk::BufferUsageFlagBits::eIndexBuffer |
                       vk::BufferUsageFlagBits::eTransferDst;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vmaCreateBuffer(
          m_allocator, reinterpret_cast<VkBufferCreateInfo *>(&bufferInfo),
          &allocInfo, reinterpret_cast<VkBuffer *>(&m_indexBuffers[i]),
          &m_indexBufferAllocations[i], nullptr);
    }
  };
  void createInstanceBuffers() {
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = sizeof(float) * 3;
    bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer |
                       vk::BufferUsageFlagBits::eTransferDst;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vmaCreateBuffer(
          m_allocator, reinterpret_cast<VkBufferCreateInfo *>(&bufferInfo),
          &allocInfo, reinterpret_cast<VkBuffer *>(&m_instanceBuffers[i]),
          &m_instanceBufferAllocations[i], nullptr);
    }
  };
  void createStagingBuffers() {
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.size = sizeof(uint32_t) * 78;
    bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      VmaAllocationInfo info;
      vmaCreateBuffer(
          m_allocator, reinterpret_cast<VkBufferCreateInfo *>(&bufferInfo),
          &allocInfo, reinterpret_cast<VkBuffer *>(&m_stagingBuffers[i]),
          &m_stagingBufferAllocations[i], &info);
      m_mappedStagingBuffers[i] = info.pMappedData;
    }
  };
  void generateVertices(){
      for (uint32_t i = 0; i < 25; i++) {
        m_vertices[2*i] = std::cos(2 * M_PI * i / 25);
        m_vertices[2*i+1] = std::sin(2 * M_PI * i / 25);
      }
      m_vertices[52] = 0.0f;
      m_vertices[53] = 0.0f;
  };
  void generateIndices(){
      for(uint32_t i = 0; i < 25; i++){
          m_indices[3*i] = i;
          m_indices[3*i+1] = (i+1)%25;
          m_indices[3*i+2] = 26;
      }
  };
  void transferVertexBuffers(){
      m_commandBuffer[m_currentFrame].begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
      for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
          memcpy(m_mappedStagingBuffers[i], m_vertices, sizeof(float)*54);
          vk::BufferCopy copyRegion(0, 0, sizeof(float)*54);
          m_commandBuffer[m_currentFrame].copyBuffer(m_stagingBuffers[i], m_vertexBuffers[i], copyRegion);
      }
      m_commandBuffer[m_currentFrame].end();
      vk::SubmitInfo submitInfo{};
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &m_commandBuffer[m_currentFrame];
      submitInfo.signalSemaphoreCount = 0;
      submitInfo.waitSemaphoreCount = 0;
      submitInfo.pWaitSemaphores = nullptr;
      submitInfo.pSignalSemaphores = nullptr;
      submitInfo.pWaitDstStageMask = nullptr;
      m_queue.submit(submitInfo, nullptr);
      m_queue.waitIdle();
  };
  void transferIndexBuffers(){
      m_commandBuffer[m_currentFrame].begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
      for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
          memcpy(m_mappedStagingBuffers[i], m_indices, sizeof(uint32_t)*78);
          vk::BufferCopy copyRegion(0, 0, sizeof(uint32_t)*78);
          m_commandBuffer[m_currentFrame].copyBuffer(m_stagingBuffers[i], m_indexBuffers[i], copyRegion);
      }
      m_commandBuffer[m_currentFrame].end();
      vk::SubmitInfo submitInfo{};
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &m_commandBuffer[m_currentFrame];
      submitInfo.signalSemaphoreCount = 0;
      submitInfo.waitSemaphoreCount = 0;
      submitInfo.pWaitSemaphores = nullptr;
      submitInfo.pSignalSemaphores = nullptr;
      submitInfo.pWaitDstStageMask = nullptr;
      m_queue.submit(submitInfo, nullptr);
      m_queue.waitIdle();
  };

public:
  VulkanRenderer() {
    initWindow();
    createInstance();
    createPhysicalDevice();
    createLogicalDevice();
    createAllocator();
    createSurface();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
    createVertexBuffers();
    createIndexBuffers();
    createInstanceBuffers();
    createStagingBuffers();
    generateVertices();
    generateIndices();
    transferVertexBuffers();
    transferIndexBuffers();
  };
  ~VulkanRenderer() {
      m_device.waitIdle();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vmaDestroyBuffer(m_allocator, m_stagingBuffers[i],
                       m_stagingBufferAllocations[i]);
      vmaDestroyBuffer(m_allocator, m_instanceBuffers[i],
                       m_instanceBufferAllocations[i]);
      vmaDestroyBuffer(m_allocator, m_indexBuffers[i],
                       m_indexBufferAllocations[i]);
      vmaDestroyBuffer(m_allocator, m_vertexBuffers[i],
                       m_vertexBufferAllocations[i]);
      m_device.destroySemaphore(m_imageAvailableSemaphores[i]);
      m_device.destroySemaphore(m_renderFinishedSemaphores[i]);
      m_device.destroyFence(m_inFlightFences[i]);
      m_device.freeCommandBuffers(m_commandPool, m_commandBuffer[i]);
    }
    m_device.destroyCommandPool(m_commandPool);
    for (const auto &framebuffer : m_framebuffers) {
      m_device.destroyFramebuffer(framebuffer);
    }
    m_device.destroyPipeline(m_graphicsPipeline);
    m_device.destroyPipelineLayout(m_pipelineLayout);
    m_device.destroyRenderPass(m_renderPass);
    for (const auto &imageView : m_swapchainImageViews) {
      m_device.destroyImageView(imageView);
    }
    m_device.destroySwapchainKHR(m_swapchain);
    m_instance.destroySurfaceKHR(m_surface);
    vmaDestroyAllocator(m_allocator);
    m_device.destroy();
    m_instance.destroy();
    glfwDestroyWindow(m_window);
    glfwTerminate();
  };
  bool shouldQuit() { return glfwWindowShouldClose(m_window); };
  void uploadInstanceData(float* data, uint32_t instanceCount){
      float* currentStagingBuffer = reinterpret_cast<float*>(m_mappedStagingBuffers[m_currentFrame]);
      memcpy(currentStagingBuffer, data, sizeof(float)*3*instanceCount);
      vk::BufferCopy copyRegion(0,0,sizeof(float)*3*instanceCount);
      m_commandBuffer[m_currentFrame].copyBuffer(m_stagingBuffers[m_currentFrame], m_instanceBuffers[m_currentFrame], copyRegion);
      vk::MemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eVertexAttributeRead);
      vk::BufferMemoryBarrier bufferBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eVertexAttributeRead, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_instanceBuffers[m_currentFrame], 0, VK_WHOLE_SIZE);
      m_commandBuffer[m_currentFrame].pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput, {}, barrier, bufferBarrier, {});
  }
  void drawFrame(float* instanceData, uint32_t instanceCount = 1) {
      m_device.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
      m_device.resetFences(1, &m_inFlightFences[m_currentFrame]);
      uint32_t imageIndex = m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame]).value;
      m_commandBuffer[m_currentFrame].begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
      uploadInstanceData(instanceData, instanceCount);
      vk::ClearValue clearValue{};
      clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
      vk::RenderPassBeginInfo renderPassBeginInfo{};
      renderPassBeginInfo.clearValueCount = 1;
      renderPassBeginInfo.pClearValues = &clearValue;
      renderPassBeginInfo.framebuffer = m_framebuffers[imageIndex];
      renderPassBeginInfo.renderArea = vk::Rect2D{{0,0}, {WIDTH, HEIGHT}};
      renderPassBeginInfo.renderPass = m_renderPass;
      m_commandBuffer[m_currentFrame].beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
      m_commandBuffer[m_currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
      m_commandBuffer[m_currentFrame].bindVertexBuffers(0, {m_vertexBuffers[m_currentFrame], m_instanceBuffers[m_currentFrame]}, {0, 0});
      m_commandBuffer[m_currentFrame].bindIndexBuffer(m_indexBuffers[m_currentFrame], 0, vk::IndexType::eUint32);
      m_commandBuffer[m_currentFrame].drawIndexed(78, instanceCount, 0, 0, 0);
      m_commandBuffer[m_currentFrame].endRenderPass();
      m_commandBuffer[m_currentFrame].end();
      vk::SubmitInfo submitInfo{};
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &m_commandBuffer[m_currentFrame];
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrame];
      vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
      submitInfo.pWaitDstStageMask = waitStages;
      m_queue.submit(submitInfo, m_inFlightFences[m_currentFrame]);
      vk::PresentInfoKHR presentInfo{};
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame];
      presentInfo.swapchainCount = 1;
      presentInfo.pSwapchains = &m_swapchain;
      presentInfo.pImageIndices = &imageIndex;
      presentInfo.pResults = nullptr;
      m_queue.presentKHR(presentInfo);
      m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  };
};

class BouncingBall{
    private:
    const float GRAVITY = 0.5;
    float m_vx;
    float m_vy;
    float m_instanceData[3];
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTime;
    public:
    BouncingBall(float x, float y, float vx, float vy, float radius){
        m_instanceData[0] = x;
        m_instanceData[1] = y;
        m_instanceData[2] = radius;
        m_vx = vx;
        m_vy = vy;
        m_lastTime = std::chrono::high_resolution_clock::now();
    };
    float* getInstanceData(){
        return m_instanceData;
    };
    void update(){
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto dt = std::chrono::duration<float>(currentTime - m_lastTime).count();
        m_instanceData[0] += m_vx * dt;
        m_instanceData[1] -= m_vy * dt;
        m_vy -= GRAVITY * dt;
        m_lastTime = currentTime;
        if(m_instanceData[0] + m_instanceData[2] > 1.0){
            m_vx = -m_vx;
            m_instanceData[0] = 1.0 - m_instanceData[2];
        }
        if(m_instanceData[0] - m_instanceData[2] < -1.0){
            m_vx = -m_vx;
            m_instanceData[0] = -1.0 + m_instanceData[2];
        }
        if(m_instanceData[1] + m_instanceData[2] > 1.0){
            m_vy = -m_vy;
            m_instanceData[1] = 1.0 - m_instanceData[2];
        }
        if(m_instanceData[1] - m_instanceData[2] < -1.0){
            m_vy = -m_vy;
            m_instanceData[1] = -1.0 + m_instanceData[2];
        }
    };
};

int main() {
  VulkanRenderer app;
  BouncingBall ball(0, 0, 0.1, 0.2, 0.2);
  while (!app.shouldQuit()) {
    glfwPollEvents();
    app.drawFrame(ball.getInstanceData());
    ball.update();
  }
  return 0;
}
