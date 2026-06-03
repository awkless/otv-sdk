<!--
SPDX-FileCopyrightText: 2026 Jason Pena <jpalex@nmsu.edu>
SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# OTV Analysis Driver

A simple binary that uses `otv-engine` to perform OTV analysis on video data.

## Install

Follow the instructions in the top-level README of the project. The
`otv-analyze` binary is built with the rest of the active codebase by default.

## Usage

The `otv-analyze` binary provides the following command-line interface:

```
otv-analyze [-? | -h | --help] [-m | --mask <mask>] [-o | --output <output>]
        [-v | --visualize] <video> <config>
```

### Options

Lets go over the options for `otv-analyze`. The `[-m | --mask <mask>]` option
allows the user to feed a black and white image mask to `otv-analyze`. Where
white pixels tell `otv-analyze` to track particles in that region only, and black
pixels tell `otv-analyze` to ignore those regions.

The `[-o | --output]` option allows the user to control where `otv-analyze` places
its analysis output. By default, `otv-analyze` creates a directory called `output/`
relative to where the user invoked the binary itself. This option changes that
behavior to create the output directory at whatever path they specified.

The `[-v | --visualize]` option makes `otv-analyze` reproduce and visualize the
trajectories of the particles it has detected and tracked throughout the input
video data. This visualized paritcle trajectory data is placed into a video that
is saved into the output directory.

### Configuration File

The `otv-analyze` binary requires a special configuration file in order to perform
the OTV analysis. This configuration file uses the TOML data exchange format,
and requires the user to define all key/value pairs for the following tables:
`[calibration]`, `[lukas_kanade]`, and `[video_info]`.

The `[calibration]` table houses configuration settings that manipulates the
the behavior of the OTV engine. The `[lukas_kanade]` table includes various
settings that adjusts the behavior of OpenCV's Lukas-Kanade algorithm
implementation, which affects the performance and accuracy of particle tracking.
Finally, the `[video_info]` table contains configuration settings regarding how
`otv-analyze` should process the input video data into the OTV engine.

#### Key/Value Pair Settings for Calibration Table

- `pixel_to_real = <float>`- The conversion factor from pixel displacement to
  real-world units in meters. Used to determine velocity by multiplying raw
  pixel magnitude and effective FPS over time. Scaled further by `resolution`
  setting at runtime.

- `partial_min_angle = <float>` / `partial_max_angle = <float>` - These two
  settings determine the angular bounds for the partial filter of the OTV engine
  in degrees. The partial filter is a per-frame check that decides whether a
  point is still worth tracking. Any optical flow vector whose angle falls
  outside this range is dropped immediately.

- `final_min_angle = <float>` / `final_max_angle = <float>` - These two settings
  determine a tighter angular window used by the final filter inside the OTV
  engine in degrees. The final filter rejects trajectories that drifted outside
  the expected heading over their full lifetime, and is only applied when the
  trajectory ends.

- `final_min_distance = <float>` - The minimum pixel displacement a trajectory
  must accumulate from its start point to its end point before it is accepted
  by the final filter. Scaled by the `resolution` setting at runtime.

- `max_features = <integer>` - The maximum cap on simultaneously tracked key
  points. This is mainly used by the OTV engine to compute a budget of
  `max_features - current_point_count`, which ensures that the engine only
  adds newly detected points up to that limit.

- `resolution = ["full_frame" | "half_frame" | "quarter_frame"]` - Controls
  scale factor applied to several geometry-dependent thresholds.

- `region_step = <float>` - The width in pixels of each horizontal sub-region.
  Completed trajectories are bucketed into whichever sub-region their start
  and end x-cooridnates both fall in, allowing per-zone velocity statistics.

- `feature_detector = ["FAST" | "ORB" | "SIFT" | "GFTT"]` - Selects the OpenCV
  feature detector to determine the key points that the OTV engine must track
  using the Lukas-Kanade opitcal flow algorithm.

#### Key/Value Pair Settings for Lukas Kanade Table

- `max_level = <integer>` - The number of pyramid levels for multi-scale optical
  flow. Higher values help track larger displacements but increase computation.

- `radious = <integer>` - Radius of search window where search window width and
  height is `(2 * radius + 1)` in pixels.

- `max_count = <integer>` - The maximum number of iterations for the
  Lucas-Kanade algorithm to solve per pyramid level.

- `epsilon = <float>` - The convergence threshold for the Lucas-Kanade
  algorithm. Iteration stops when the solution changes by less than this
  amount.

- `flags = 0` - Currently unused. Need to update the TOML parser to ignore
  this key/value pair. For now include it to ensure that the TOML parser does
  not complain about it missing.

- `min_eig_threshold = <float>` - Points whose optical flow solution has a
  minimum eigen value below this threshold are rejected as poorly conditioned,
  i.e, textureless.

#### Key/Value Pair Settings for Video Info Table

- `fps = <integer>` - The source video fame rate.

- `width = <integer>` / `height = <integer>` - These settings determine the
  target video frame dimensions. Used to size the feature detector, and to
  resize any supplied mask.

- `jump = <integer>` - The number of source video frames to be skipped between
  processed frames.

### Examples

The sub-module contains example data that you can use in the `examples/`
directory. Here is a possible way you can feed that data into the `otv-analyze`
binary:

```
otv-analyze -vm examples/mask.png examples/video.m4v examples/config.toml
```

## Acknowledgements

The `otv-analyze` binary is based on the work done by [Arsal-Hanif, Andrea
Conti, Luca Foianesi, et al.][otv-paper] for OTV analysis on embedded hardware.
The original code used by their paper can be found [here][otv-code].

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
