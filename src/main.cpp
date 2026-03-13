/**
 * @file main.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief 概念性项目,展示alib5的实力,同时尝试涉足Vulkan
 * @version 5.0
 * @date 2026-03-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <app.h>
#include <schema.h>

using namespace alib5;

int main(){
    //// 内存池 ////
    std::pmr::synchronized_pool_resource _res;
    std::pmr::memory_resource * res = &_res;
    //// 读取配置文件 ////
    // 解析器
    auto parser = data::JSON(
        data::JSONConfig{
            .rapidjson_recursive = false,
            .allow_comments = true
        }
    );
    // 校验器
    alib5::Validator validator(res);
    alib5::AData schema(res);
    schema.load_from_memory(str_schema,parser);
    {
        auto msg = validator.from_adata(schema);
        // 理论上不会出问题
        panic_if(msg.size(),msg);
    }
    // 核心配置
    alib5::AData config(res);
    config.load_from_file("./data/config.json",parser);
    // 校验
    {
        auto result = validator.validate(config);
        panicf_if(!result.success,"{}",result.recorded_errors);
    }

    // 日志配置
    LoggerConfig logger_cfg;
    LogFactoryConfig lg_cfg;

    {
        auto & cfg = config["logger"].object();
        logger_cfg.enable_back_pressure = cfg["back_pressure"].to<bool>();
        logger_cfg.consumer_count = cfg["consumer_count"].to<uint32_t>();
        logger_cfg.fetch_message_count_max = cfg["fetch_message_count_max"].to<uint32_t>();
        logger_cfg.back_pressure_multiply = cfg["back_pressure_multiply"].to<uint32_t>();
        lg_cfg.header = cfg["header"].to<std::string_view>();
    }


    try{
        App app(config,logger_cfg,lg_cfg,res);
        app.setup();
        return app.run();
    }catch(const std::exception & e){
        std::cerr << e.what() << std::endl;
        return -1;
    }catch(...){
        return -1;
    }
}