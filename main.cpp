#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

class VulkanRenderer {
private:
    GLFWwindow* m_window;
    vk::Instance m_instance;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    vk::Queue m_queue;
    vk::SurfaceKHR m_surface;
    vk::SwapchainKHR m_swapchain;
    void initWindow(){
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(800, 600, "Vulkan Window", nullptr, nullptr);
    };
    void createInstance(){
        vk::ApplicationInfo appInfo("VulkanApp", 1, "No Engine", 1, VK_API_VERSION_1_2);
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        std::vector<const char*> layers={"VK_LAYER_KHRONOS_validation"};
        vk::InstanceCreateInfo createInfo(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR, &appInfo, layers.size(), layers.data(), extensions.size(), extensions.data());
        m_instance = vk::createInstance(createInfo);
    };
    void createPhysicalDevice(){
        std::vector<vk::PhysicalDevice> devices = m_instance.enumeratePhysicalDevices();
        for(const auto& deviceType: {vk::PhysicalDeviceType::eDiscreteGpu, vk::PhysicalDeviceType::eIntegratedGpu, vk::PhysicalDeviceType::eVirtualGpu, vk::PhysicalDeviceType::eCpu}){
            for(const auto& device: devices){
                if(device.getProperties().deviceType == deviceType){
                    m_physicalDevice = device;
                    break;
                }
            }
        }
    };
    void createLogicalDevice(){
        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo({}, 0, 1, &queuePriority);
        std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};
        vk::DeviceCreateInfo createInfo{};
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.enabledExtensionCount = deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        m_device = m_physicalDevice.createDevice(createInfo);
        m_queue = m_device.getQueue(0, 0);
    };
    void createSurface(){
        VkSurfaceKHR surface;
        if(glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface) != VK_SUCCESS){
            throw std::runtime_error("Failed to create window surface");
        }
        m_surface = surface;
    };
    void createSwapchain(){
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
        vk::SurfaceFormatKHR surfaceFormat = vk::Format::eR8G8B8A8Unorm;
        vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
        vk::Extent2D extent = surfaceCapabilities.currentExtent;
        vk::SwapchainCreateInfoKHR createInfo({}, m_surface, surfaceCapabilities.minImageCount, surfaceFormat.format, surfaceFormat.colorSpace, extent, 1, vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, 0, nullptr, surfaceCapabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode, VK_TRUE);
        m_swapchain = m_device.createSwapchainKHR(createInfo);
    };

public:
    VulkanRenderer(){
        initWindow();
        createInstance();
        createPhysicalDevice();
        createLogicalDevice();
        createSurface();
        createSwapchain();
    };
    ~VulkanRenderer(){
        m_device.destroySwapchainKHR(m_swapchain);
        m_instance.destroySurfaceKHR(m_surface);
        m_device.destroy();
        m_instance.destroy();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    };
    void run(){
        while(!glfwWindowShouldClose(m_window)){
            glfwPollEvents();
        }
    };
};

int main() {
    VulkanRenderer app;
    app.run();
    return 0;
}
