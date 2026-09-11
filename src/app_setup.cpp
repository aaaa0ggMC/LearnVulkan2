#include "app.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

import alib6;

using namespace alib6;

void App::setup() {
    _setup_logger();
    _setup_language();
    _setup_glfw();
    _setup_vulkan();
}

void App::endup() {
    if (translator) {
        lg << translator->translate("cleanup") << endlog;
    }
}

void App::_setup_glfw() {
    glfwInit();
    defer_mgr.defer([] {
        glfwTerminate();
    });

    // 检查 Vulkan 支持
    if (glfwVulkanSupported()) {
        lg << translator->translate("ok.vulkan") << endlog;
    } else {
        lg(LogLevel::Error) << translator->translate("bad.vulkan") << endlog;
        throw "Bad GLFW Vulkan support!";
    }

    // 初始化 GLFW 配置
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // 基于强类型反射配置创建窗口
    uint32_t width = app_cfg.window.width;
    uint32_t height = app_cfg.window.height;
    const std::string& title = app_cfg.window.title;

    window = glfwCreateWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        title.c_str(),
        nullptr,
        nullptr
    );

    if (!window) {
        const char* desc = nullptr;
        int code = glfwGetError(&desc);
        lg(LogLevel::Error) << translator->translate("bad.window", width, height, title, code, desc ? desc : "Unknown") << endlog;
        throw "Failed to create GLFW window!";
    }

    defer_mgr.defer([this] {
        if (window) glfwDestroyWindow(window);
    });

    lg(LogLevel::Info) << translator->translate("ok.window", width, height, title) << endlog;
}

void App::_setup_language() {
    full_translator.load_from_entry(
        io::load_entry("./data/translations")
    );

    full_translator.switch_language(app_cfg.language);
    auto t = full_translator.flatten_dots(app_cfg.language);
    if (!t) {
        std::vector<std::string> supports;
        for (auto proxy : full_translator.data().object()) {
            supports.emplace_back(proxy.first());
        }
        lg(LogLevel::Error) << "Translations failed to load! Supported languages: " << supports << endlog;
        throw "Translations failed to load!";
    }
    translator.emplace(std::move(*t));
    lg(LogLevel::Info) << translator->translate(
        "test",
        translator->translate("title")
    ) << endlog;
}

void App::_setup_logger() {
    logger.append_mod<alib6::lot::Console>("console");
    logger.append_mod<alib6::lot::RotateFile>("file", alib6::lot::RotateFileConfig("latest{1}.log"));

    lg << "Log system has initialized." << endlog;
}
