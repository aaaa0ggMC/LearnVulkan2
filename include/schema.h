#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <cstdint>
#include <meta>

import alib6;

namespace defaults {
    inline constexpr bool enable = true;
    inline constexpr bool disable = false;
    inline constexpr std::string_view default_language = "zh_cn";
    inline constexpr std::string_view default_app_name = "LearnVulkan2";
    inline constexpr size_t default_consumer_count = 1;
    inline constexpr size_t default_fetch_msg_max = 128;
    inline constexpr size_t default_bp_multiply = 4;
    inline constexpr uint32_t default_window_width = 800;
    inline constexpr uint32_t default_window_height = 600;
    inline constexpr double default_score_api = 0.6;
    inline constexpr double default_score_discrete = 2.0;
    inline constexpr double default_score_img2d = 0.2;
    inline constexpr uint32_t default_shader_detail_count = 128;
    inline constexpr std::string_view default_engine_name = "No Engine";
    inline constexpr std::string_view default_empty_str = "";
    inline constexpr std::array<uint32_t, 3> default_version_1_0_0 = {1, 0, 0};
    inline constexpr std::array<uint32_t, 4> default_api_version = {0, 1, 3, 0};
}

namespace cfg {
    struct Logger {
        [[=alib6::attr::alias<"enable_back_pressure">{}]]
        [[=alib6::attr::schema::default_value{defaults::disable}]]
        bool back_pressure{false};

        [[=alib6::attr::schema::range{0.0, alib6::attr::no_range}]]
        [[=alib6::attr::schema::default_value{defaults::default_consumer_count}]]
        size_t consumer_count{defaults::default_consumer_count};

        [[=alib6::attr::schema::range{0.0, alib6::attr::no_range}]]
        [[=alib6::attr::schema::default_value{defaults::default_fetch_msg_max}]]
        size_t fetch_message_count_max{defaults::default_fetch_msg_max};

        [[=alib6::attr::schema::range{0.0, alib6::attr::no_range}]]
        [[=alib6::attr::schema::default_value{defaults::default_bp_multiply}]]
        size_t back_pressure_multiply{defaults::default_bp_multiply};

        // 借助 alib6 Alias > Rename > Origin 优先级，直接通过 alias<"header"> 异类填充至 LogFactoryConfig::header
        [[=alib6::attr::alias<"header">{}]]
        [[=alib6::attr::schema::default_value{defaults::default_app_name}]]
        std::string app_name{defaults::default_app_name};
    };

    struct Window {
        [[=alib6::attr::schema::range{0.0, alib6::attr::no_range}]]
        [[=alib6::attr::schema::default_value{defaults::default_window_width}]]
        uint32_t width{defaults::default_window_width};

        [[=alib6::attr::schema::range{0.0, alib6::attr::no_range}]]
        [[=alib6::attr::schema::default_value{defaults::default_window_height}]]
        uint32_t height{defaults::default_window_height};

        [[=alib6::attr::schema::default_value{defaults::default_app_name}]]
        std::string title{defaults::default_app_name};
    };

    struct DebugAllow {
        [[=alib6::attr::schema::default_value{defaults::enable}]]
        bool verbose{true};

        [[=alib6::attr::schema::default_value{defaults::enable}]]
        bool info{true};

        [[=alib6::attr::schema::default_value{defaults::enable}]]
        bool warn{true};

        [[=alib6::attr::schema::default_value{defaults::enable}]]
        bool error{true};

        [[=alib6::attr::schema::default_value{defaults::enable}]]
        bool others{true};
    };

    struct ScoreMultiplier {
        [[=alib6::attr::schema::default_value{defaults::default_score_api}]]
        double api_version{0.6};

        [[=alib6::attr::schema::default_value{defaults::default_score_discrete}]]
        double discrete_gpu{2.0};

        [[=alib6::attr::schema::default_value{defaults::default_score_img2d}]]
        double image_dim2d{0.2};

        [[=alib6::attr::schema::default_value{defaults::enable}]]
        bool need_geometry{true};

        [[=alib6::attr::schema::default_value{defaults::disable}]]
        bool fail_load{false};
    };

    struct ShaderItem {
        std::string vert{""};
        std::string vert_raw{""};

        [[=alib6::attr::schema::default_value{defaults::default_empty_str}]]
        std::string frag{""};

        [[=alib6::attr::schema::default_value{defaults::default_empty_str}]]
        std::string frag_raw{""};
    };

    struct Vulkan {
        [[=alib6::attr::schema::default_value{defaults::default_app_name}]]
        std::string app_name{defaults::default_app_name};

        [[=alib6::attr::schema::default_value{defaults::default_version_1_0_0}]]
        std::vector<uint32_t> app_version{1, 0, 0};

        [[=alib6::attr::schema::default_value{defaults::default_version_1_0_0}]]
        std::vector<uint32_t> engine_version{1, 0, 0};

        [[=alib6::attr::schema::default_value{defaults::default_api_version}]]
        std::vector<uint32_t> api_version{0, 1, 3, 0};

        [[=alib6::attr::schema::default_value{defaults::default_engine_name}]]
        std::string engine_name{defaults::default_engine_name};

        std::vector<std::string> instance_extensions{};
        std::vector<std::string> device_extensions{};
        std::vector<std::string> layers{};

        DebugAllow debug_allow{};
        ScoreMultiplier score_multiplier{};

        [[=alib6::attr::schema::default_value{defaults::disable}]]
        bool verbose_extensions{false};

        std::vector<ShaderItem> shaders{};

        [[=alib6::attr::schema::range{0.0, alib6::attr::no_range}]]
        [[=alib6::attr::schema::default_value{defaults::default_shader_detail_count}]]
        uint32_t shader_detail_count{128};
    };
}

struct ApplicationConfig {
    [[=alib6::attr::schema::default_value{defaults::default_language}]]
    std::string language{defaults::default_language};

    cfg::Logger logger{};
    cfg::Window window{};
    cfg::Vulkan vulkan{};

    // 异类填充: 自动将 logger 注入到实际 alib6::log::LoggerConfig / alib6::log::LogFactoryConfig
    [[=alib6::attr::fill_by<"logger">{}]]
    alib6::log::LoggerConfig actual_logger{};

    [[=alib6::attr::fill_by<"logger">{}]]
    alib6::log::LogFactoryConfig actual_factory{};
};
