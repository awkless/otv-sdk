# SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
# SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(Engine)
add_library(OtvSdk::Engine ALIAS Engine)

set_target_properties(
    Engine
    PROPERTIES VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_sources(
    Engine
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/otv_sdk/engine.cpp"
)

target_include_directories(
    Engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_property(
    TARGET Engine
    PROPERTY CXX_VISIBILITY_PRESET "hidden"
)

set_property(
    TARGET Engine
    PROPERTY VISIBILITY_INLINES_HIDDEN TRUE
)

include(GenerateExportHeader)
generate_export_header(
    Engine
    EXPORT_FILE_NAME
        "${CMAKE_CURRENT_BINARY_DIR}/otv_sdk/engine/export.hpp"
)

target_include_directories(
    Engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(
    Engine
    PUBLIC
        ${OpenCV_LIBS}
        $<BUILD_INTERFACE:tomlplusplus::tomlplusplus>
    PRIVATE
        $<BUILD_INTERFACE:fmt::fmt>
        $<BUILD_INTERFACE:spdlog::spdlog>
)

include(GNUInstallDirs)
install(
    TARGETS Engine
    EXPORT OtvEngineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/otv_sdk/engine/export.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/otv_sdk/engine
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "OtvSdkConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/OtvSdkConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/OtvSdkConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/OtvSdk
)

install(
    EXPORT OtvEngineTargets
    FILE OtvEngineTargets.cmake
    NAMESPACE OtvSdk::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/OtvSdk
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/OtvSdkConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/OtvSdkConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/OtvSdk
)
