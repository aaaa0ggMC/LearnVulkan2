#include <app.h>
#include <alib5/aclock.h>
#include <vulkan/vulkan.h>

using namespace alib5;
using enum Severity;

void App::setup(){
    _setup_logger();
    _setup_language();
    _setup_glfw();
    _setup_vulkan();
}

void App::endup(){
    lg << translator->translate("cleanup") << endlog;
}

void App::_setup_glfw(){
    glfwInit();
    defer_mgr.defer([]{
        glfwTerminate();
    });

    // 检查vulkan支持
    if(glfwVulkanSupported()){
        lg << translator->translate("ok.vulkan") << endlog;
    }else{
        lg(Error) << translator->translate("bad.vulkan") << endlog;
        throw "Bad GUY!";
    }

    // 初始化GLFW
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API);
    // 目前处理resize比较复杂
    ///  @todo 未来学习过程中尝试支持
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE );

    // 创建窗口
    auto width = config.jump("/window/width").to<uint32_t>();
    auto height = config.jump("/window/height").to<uint32_t>();
    auto title = config.jump("/window/title").to<std::string_view>();

    window = glfwCreateWindow(
        width,
        height,
        title.data(),
        nullptr,
        nullptr
    );

    if(!window){
        const char * desc;
        int code = glfwGetError(&desc);
        lg(Error) << translator->translate("bad.window",width,height,title,code,desc) << endlog;
        throw "Bad GUY!";
    }
    defer_mgr.defer([this]{
        glfwDestroyWindow(window);
    });

    lg(Info) << translator->translate("ok.window",width,height,title) << endlog;
}

void App::_setup_language(){
    full_translator.load_from_entry(
        io::load_entry("./data/translations")
    );

    full_translator.switch_language(config["language"].to<std::string_view>());
    auto t = full_translator.flatten_dots();
    if(!t){
        std::vector<std::string> supports;
        for(auto proxy : full_translator.data().object()){
            supports.emplace_back(proxy.first());
        }
        lg << "Translations failed to load!Supported languages:" << supports << alib5::endlog;
        throw "BAD GUY!";
    }
    translator.emplace(*t);
    lg(Info) << translator->translate<false>(
        "test",
        translator->translate("title")
    ) << endlog;
}

void App::_setup_logger(){
    logger.append_mod<lot::Console>("console");
    logger.append_mod<lot::RotateFile>("file",lot::RotateFileConfig("latest{1}.log"));

    lg << "Log system has initialized." << alib5::endlog;
}