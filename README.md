<!--
SPDX-FileCopyrightText: 2026 Jason Pena <jpalex@nmsu.edu>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# OTV SDK

A simple development kit for water surface velocity tracking via the
optical tracking velocimetry (OTV).

## Architecture

The project codebase follows the [pitchfork][std-pitchfork] layout for C++
projects. Pitchfork is a build system agnostic project structure standard that
makes it easy to both setup and navigate large C++ codebases. This project
mainly uses [submodules][std-pitchfork-submodules] to structure libraries and
binaries. Each submodule component in the active codebase comes with its own
README that describes what its purpose is, and how to use it.

The following is a list of all current submodules apart of the active codebase:

- `otv-engine` [library] - OTV analysis engine API.
- `otv-analyze` [binary] - OTV analysis driver over video files.
- `otv-stream` [binary] - OTV analysis driver using a server-client framework
   on Rpi 4B hardware.

## Install

Please, ensure that the following software has been installed beforehand. Do
note that this project mainly targets Unix-like environments.

- GNU Compiler Collection (GCC) [>= 8.0.0]
- CMake [>= 3.26.0]
- Ninja [>= 1.12.1]
- OpenCV Library [>= 4.10.0]

The project leverages CMake presets to make building and installing the project
easy. There are four major build targets: host-debug, host-release, rpi4b-debug,
and rpi4b-release. The host-debug and host-release targets will build software
components that function natively on desktop hardware. This will build the
`otv-analyze` binary. The rpi4b-debug and rpi4b-release targets will build
software components that function only on Rpi 4B hardware. This will build the
server-client framework provided by the `otv-stream` submodule. Regardless of
the preset target, the `otv-engine` submodule will always be built given that it
provides the OTV analysis engine used by both `otv-analyze` and `otv-stream`.

For a release or debug build on host, execute the following commands:

```
cmake --preset host-release # or host-debug
cmake --build --preset host-release # or host-debug
sudo cmake install
```

For a release or debug build on the Rpi4B, execute the following commands:

```
cmake --preset rpi4b-release # or rpi4b-debug
cmake --build --preset release # or rpi4b-debug
sudo cmake install
```

Maintainers may also want to enable unit testing to ensure build quality.
Execute the following commands to activate the test suite:

```
# You can also build the test suite using the rpi4b-debug preset target as well.
cmake --preset host-debug -DENABLE_TESTS=ON
cmake --build --preset host-debug
ctest --test-dir ./build/host-debug
```

## Usage

Please refer to the README of a given submodule to learn how to use it.

## License

Copyright (C) 2025-2026 Jason Pena <<jpalex@nmsu.edu>>, Huidae Cho <<hcho@nmsu.edu>>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.

[std-pitchfork]: https://joholl.github.io/pitchfork-website/
[std-pitchfork-submodules]: https://joholl.github.io/pitchfork-website/#submod
