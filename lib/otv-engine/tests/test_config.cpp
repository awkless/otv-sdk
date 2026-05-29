// SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
// SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <otv_sdk/engine.hpp>

TEST_CASE("otv_sdk::Config::from_file - parse from file", "[config]")
{
    auto path = std::filesystem::path("./tests/fixtures/good_config.toml");
    auto config = otv_sdk::Config::from_file(path);

    REQUIRE_THAT( config.calibration.pixel_to_real, Catch::Matchers::WithinAbs(0.004965, 0.000005));
    REQUIRE_THAT(config.calibration.partial_min_angle, Catch::Matchers::WithinAbs(-135.00, 0.0005));
    REQUIRE_THAT(config.calibration.partial_max_angle, Catch::Matchers::WithinAbs(-45.00, 0.0005));
    REQUIRE_THAT(config.calibration.final_min_angle, Catch::Matchers::WithinAbs(-110.00, 0.0005));
    REQUIRE_THAT(config.calibration.final_max_angle, Catch::Matchers::WithinAbs(-80.00, 0.0005));
    REQUIRE_THAT(config.calibration.final_min_distance, Catch::Matchers::WithinAbs(216.00, 0.0005));
    REQUIRE(config.calibration.max_features == 15000);
    REQUIRE_THAT(config.calibration.resolution, Catch::Matchers::WithinAbs(0.25, 0.0005));
    REQUIRE_THAT(config.calibration.region_step, Catch::Matchers::WithinAbs(100.00, 0.0005));
    REQUIRE(config.calibration.feature_detector == otv_sdk::FeatureDetectorOption::GFTT);

    REQUIRE(config.lucas_kanade.radius == 4);
    REQUIRE(config.lucas_kanade.max_level == 3);
    REQUIRE(config.lucas_kanade.max_count == 20);
    REQUIRE_THAT(config.lucas_kanade.epsilon, Catch::Matchers::WithinAbs(0.03, 0.00005));
    REQUIRE_THAT(config.lucas_kanade.min_eig_threshold, Catch::Matchers::WithinAbs(0.001, 0.00005));

    REQUIRE(config.video.fps == 25);
    REQUIRE(config.video.width == 1430);
    REQUIRE(config.video.height == 1080);
    REQUIRE(config.video.jump == 1);
}

TEST_CASE("otv_sdk::Config::from_file - catch invalid files", "[config]")
{
    auto bad_path = std::filesystem::path("./does/not/exist.toml");
    auto bad_config = std::filesystem::path("./tests/fixtures/bad_config.toml");

    REQUIRE_THROWS_AS(otv_sdk::Config::from_file(bad_path), otv_sdk::ConfigError);
    REQUIRE_THROWS_AS(otv_sdk::Config::from_file(bad_config), otv_sdk::ConfigError);
}

TEST_CASE("otv_sdk::Config::from_str - parse from string", "[config]")
{
    std::string input = R"(
        [calibration]
	pixel_to_real = 0.004965
	partial_min_angle = -135.00
	partial_max_angle = -45.00
	final_min_angle = -110.00
	final_max_angle = -80.00
	final_min_distance = 216.00
	max_features = 15000
	resolution = "full_frame"
	region_step = 100.00
	feature_detector = "FAST"

        [lukas_kanade]
	max_level = 3
	radius = 4
	max_count = 20
	epsilon = 0.03
	min_eig_threshold = 0.001

	[video_info]
	fps = 25
	width = 1430
	height = 1080
	jump = 1
    )";

    auto config = otv_sdk::Config::from_str(input);

    REQUIRE_THAT( config.calibration.pixel_to_real, Catch::Matchers::WithinAbs(0.004965, 0.000005));
    REQUIRE_THAT(config.calibration.partial_min_angle, Catch::Matchers::WithinAbs(-135.00, 0.0005));
    REQUIRE_THAT(config.calibration.partial_max_angle, Catch::Matchers::WithinAbs(-45.00, 0.0005));
    REQUIRE_THAT(config.calibration.final_min_angle, Catch::Matchers::WithinAbs(-110.00, 0.0005));
    REQUIRE_THAT(config.calibration.final_max_angle, Catch::Matchers::WithinAbs(-80.00, 0.0005));
    REQUIRE_THAT(config.calibration.final_min_distance, Catch::Matchers::WithinAbs(216.00, 0.0005));
    REQUIRE(config.calibration.max_features == 15000);
    REQUIRE_THAT(config.calibration.resolution, Catch::Matchers::WithinAbs(1.0, 0.0005));
    REQUIRE_THAT(config.calibration.region_step, Catch::Matchers::WithinAbs(100.00, 0.0005));
    REQUIRE(config.calibration.feature_detector == otv_sdk::FeatureDetectorOption::FAST);

    REQUIRE(config.lucas_kanade.radius == 4);
    REQUIRE(config.lucas_kanade.max_level == 3);
    REQUIRE(config.lucas_kanade.max_count == 20);
    REQUIRE_THAT(config.lucas_kanade.epsilon, Catch::Matchers::WithinAbs(0.03, 0.00005));
    REQUIRE_THAT(config.lucas_kanade.min_eig_threshold, Catch::Matchers::WithinAbs(0.001, 0.00005));

    REQUIRE(config.video.fps == 25);
    REQUIRE(config.video.width == 1430);
    REQUIRE(config.video.height == 1080);
    REQUIRE(config.video.jump == 1);
}

TEST_CASE("otv_sdk::Config::from_str - catch invalid formatting", "[config]")
{
    std::string input1 = R"(
	[fail
        right now 1234
    )";

    std::string input2 = R"(
        [calibration]
	pixel_to_real = 0.004965
	partial_min_angle = "not a string!"
    )";

    REQUIRE_THROWS_AS(otv_sdk::Config::from_str(input1), otv_sdk::ConfigError);
    REQUIRE_THROWS_AS(otv_sdk::Config::from_str(input2), otv_sdk::ConfigError);
}
