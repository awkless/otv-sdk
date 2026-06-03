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

## Install

Please, ensure that the following software has been installed beforehand. Do
note that this project mainly targets Unix-like environments.

- GNU Compiler Collection (GCC) [>= 8.0.0]
- CMake [>= 3.26.0]
- Ninja [>= 1.12.1]
- OpenCV Library [>= 4.10.0]

There are two major build targets: debug and release. A debug build retains
special symbols so the built binaries can be fed into a debugger. A release
build strips out all debug symbols, and heavily optimizes for reduced binary
size and increased binary speed. In general, users will want to use a release
build, while developers and maintainers will want to use a debug build.

For a release build, execute the following commands:

```
cmake --preset debug
cmake --build --preset debug
sudo cmake install
```

For a debug build, execute the following commands:

```
cmake --preset release
cmake --build --preset release
sudo cmake install
```

Maintainers may also want to enable unit testing to ensure build quality.
Execute the following commands to activate the test suite:

```
cmake --preset debug -DENABLE_TESTS=ON
cmake --build --preset debug
ctest --test-dir ./build/debug
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
