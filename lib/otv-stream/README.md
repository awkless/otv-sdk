<!--
SPDX-FileCopyrightText: 2026 Jason Pena <jpalex@nmsu.edu>
SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# OTV Real-Time Server-Client Framework

Perform real-time OTV analysis through server-client framework using WebSockets.

## Install

Follow the instructions in the top-level README of the project. The tooling
produced by `otv-stream` is built for Rpi 4B hardware targets only.

## Usage

### The `otv-server` binary

```
otv-server <port> <config> [-? | -h | --help] [-m | --mask <mask>]
           [-c | --calibrate] --viz-port <port>
```

TODO

### The `otv-client` binary

```
otv-client <rpi4b_ip_addr> <port> [-? | -h | --help] [-c | --calibrate]
           --viz-port <port>
```

TODO

## Acknowledgements

The OTV engine is based on the work done by [Arsal-Hanif, Andrea Conti, Luca
Foianesi, et al.][otv-paper] for OTV analysis on embedded hardware.  The
original code used by their paper can be found [here][otv-code].

## License

Copyright (C) 2025-2026 Jason Pena <jpalex@nmsu.edu> and Huidae Cho <hcho@nmsu.edu>

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.

[otv-code]: https://github.com/fabiotosi92/Optical-Tracking-Velocimetry
[otv-paper]: https://www.mdpi.com/2076-3417/11/15/7027?utm_source=researchgate.net&utm_medium=article
