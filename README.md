# perlinbg

`perlinbg` is a tool for generating animated backgrounds from 3D Perlin noise. It provides a graphical version for live preview and a command-line tool to export animations to GIF or WebM files.

## Features

*   Generates animated 3D Perlin noise.
*   GUI mode for live preview using `raylib`.
*   CLI mode for rendering to files.
*   Supports GIF and WebM (VP9) output formats.
*   Multiple color modes for the noise visualization.

## Building

### Dependencies

To build `perlinbg`, you need the following libraries installed:

*   `raylib`
*   `ffmpeg` (libavformat, libavcodec, libavutil, libswscale)

### Compilation

A `Makefile` is provided for easy compilation.

*   To build both the GUI and CLI versions:
    ```bash
    make all
    ```
*   To build only the GUI version (`main`):
    ```bash
    make main
    ```
*   To build only the CLI version (`main_cli`):
    ```bash
    make main_cli
    ```

## Usage

### GUI Version

To run the graphical version, execute the `main` binary:

```bash
./main
```

This will open a window displaying the animated noise.

### CLI Version

The CLI version (`main_cli`) allows you to generate and save animations.

```bash
./main_cli [options]
```

**Options:**

| Flag | Description                                   | Default     |
| :--- | :-------------------------------------------- | :---------- |
| `-w` | Output width in pixels.                       | `720`       |
| `-h` | Output height in pixels.                      | `480`       |
| `-m` | Color mode (0: dual, 1: wave, 2: rainbow).   | `0`         |
| `-d` | Step distance in the noise Z-axis.            | `0.3`       |
| `-l` | Length of the animation in seconds.           | `10`        |
| `-i` | Frames per second (fps).                      | `30`        |
| `-b` | Bits per pixel for GIF output (1-16).         | `16`        |
| `-f` | Noise frequency.                              | `0.01`      |
| `-O` | Output format (`gif` or `webm`).              | `gif`       |
| `-F` | Output filename.                              | `noise.gif` |

**Example:**

To generate a 10-second, 720x480 GIF at 30 fps with the "rainbow" color mode:

```bash
./main_cli -w 720 -h 480 -l 10 -i 30 -m 2 -F rainbow_noise.gif
```

## License

This project is licensed under the **GNU General Public License v3.0**. See the `LICENSE` file for details.

### Third-Party Libraries

This project utilizes the following third-party libraries:

*   **FastNoiseLite:** Licensed under the MIT License. The license is available in `external/FastNoiseLite.h`.
*   **msf_gif:** Available under the MIT License or Public Domain. The license is available in `external/msf_gif.h`.
*   **raylib:** Licensed under the zlib/libpng license.
*   **FFmpeg:** Licensed under the LGPL/GPL.

The licenses of these dependencies are compatible with this project's GPL-3.0 license.
