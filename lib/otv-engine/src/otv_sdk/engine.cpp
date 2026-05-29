// SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
// SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "otv_sdk/engine.hpp"

namespace otv_sdk {
Config Config::from_file(std::filesystem::path& path)
{
    (void)path;

    return {};
}

Config Config::from_str(std::string_view string)
{
    (void)string;

    return {};
}
} // namespace otv
