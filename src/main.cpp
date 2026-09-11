/**
 * @file main.cpp
 * @author aaaa0ggmc (lovelinux@yslwd.eu.org)
 * @brief LearnVulkan2 应用程序入口，全面应用 alib6 模块与静态反射机制
 * @version 6.0
 * @date 2026-08-27
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <iostream>
#include <memory_resource>
#include <print>
#include "app.h"
#include "schema.h"

import alib6;

namespace pmr = std::pmr;
using namespace alib6;

int main() {
    //// 1. 初始化 PMR 内存池 ////
    pmr::synchronized_pool_resource _res;
    pmr::memory_resource* res = &_res;

    //// 2. 读取并解析原始 JSON 配置文件 ////
    auto parser = data::JSON(
        data::JSONConfig{
            .rapidjson_recursive = false,
            .allow_comments = true
        }
    );

    pmr::string config_content(res);
    usize bytes_read = io::read_all("./data/config.json", config_content);
    if (bytes_read == std::numeric_limits<usize>::max()) {
        panicf("Failed to read configuration file: ./data/config.json");
    }

    AData config(res);
    bool parsed = parser.parse(config_content, config);
    if (!parsed) {
        panicf("Failed to parse JSON configuration file: ./data/config.json");
    }

    //// 3. 静态反射生成 Schema 规则树 (废弃旧版硬编码 str_schema) ////
    AData schema = generate_schema<ApplicationConfig>(res);

    //// 4. 校验与默认值自动注入 ////
    Validator validator(schema, res);
    auto val_result = validator.validate(config);
    if (!val_result.success) {
        for (const auto& err_msg : val_result.recorded_errors) {
            std::println(std::cerr, "[Config Error] {}", err_msg);
        }
        panicf("Configuration validation failed against statically reflected schema!");
    }

    //// 5. 静态反射反序列化为强类型 C++ 结构体 ApplicationConfig ////
    // 同时自动执行 [[=alib6::attr::fill_by<"logger">{}]] 将 logger 异类对齐注入 actual_logger 与 actual_factory
    ApplicationConfig app_cfg{};
    bool deseri_ok = from_adata(app_cfg, config);
    if (!deseri_ok) {
        panicf("Failed to reflectively deserialize AData into ApplicationConfig!");
    }

    //// 6. 反射双向序列化一致性验证 (排查 alib6 反射潜在 BUG) ////
    AData re_exported = to_adata(app_cfg, res);
    if (!re_exported.is_object()) {
        panicf("Reflective to_adata validation failed!");
    }

    try {
        App app(app_cfg, config, res);
        app.setup();
        return app.run();
    } catch (const std::exception& e) {
        std::println(std::cerr, "Exception caught in main: {}", e.what());
        return -1;
    } catch (const char* msg) {
        std::println(std::cerr, "Error caught in main: {}", msg);
        return -1;
    } catch (...) {
        std::println(std::cerr, "Unknown error caught in main");
        return -1;
    }
}
