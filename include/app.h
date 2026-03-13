#ifndef LEARN_VK_APP_H
#define LEARN_VK_APP_H
#include <alib5/alogger.h>
#include <alib5/adata.h>
#include <alib5/atranslator.h>
#include <GLFW/glfw3.h>

struct App{
    /// 配置文件
    alib5::AData & config;

    /// 日志
    alib5::Logger logger;
    alib5::LogFactory lg;

    /// 语言类
    alib5::Translator full_translator;
    std::optional<alib5::FlattenTranslator> translator;

    /// 窗口
    GLFWwindow * window;

    App(alib5::AData & iconfig,alib5::LoggerConfig cfg0,alib5::LogFactoryConfig cfg1,std::pmr::memory_resource * __a = ALIB5_DEFAULT_MEMORY_RESOURCE)
    :config(iconfig)
    ,logger(cfg0)
    ,lg(logger,cfg1)
    ,full_translator(__a){}

    ~App(){
        endup();
    }

    void setup();
    void _setup_logger();
    void _setup_language();
    void _setup_glfw();

    int run();
    void endup();
};

#endif