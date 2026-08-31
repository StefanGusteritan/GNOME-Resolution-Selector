# GNOME Resolution Selector

A small C++ command-line utility for **GNOME on Wayland** that automatically selects the closest available display mode to a requested resolution and applies it using [`gdctl`](https://gitlab.gnome.org/GNOME/mutter/-/blob/main/tools/gdctl.c).

The program was mainly created for use with **Sunshine/Moonlight remote desktop streaming**, where the host monitor should automatically switch to a resolution that is as close as possible to the client's resolution.

## Why?

When using Sunshine to stream a desktop or game, the client may have a different resolution or aspect ratio than the host's physical monitor.

For example, the client might request:

```text
1920x1200
```

while the physical monitor only supports:

```text
2560x1440
1920x1080
1680x1050
1440x900
1280x800
...
```

Rather than manually choosing a resolution every time, this program searches the modes reported by GNOME and chooses the closest one.

For example:

```text
Requested: 1920x1200

Available 16:10 modes:
1680x1050
1440x900
1280x800

Selected:
1680x1050
```

This is especially useful when a remote client uses an aspect ratio that the physical monitor does not natively support.

## How the resolution is selected

The program uses the following criteria, in order:

### 1. Aspect ratio

Modes with an aspect ratio closer to the requested resolution are preferred.

For example:

```text
1920x1200 = 16:10
1680x1050 = 16:10
1920x1080 = 16:9
```

A 16:10 mode will therefore be preferred over a 16:9 mode when `1920x1200` is requested.

### 2. Number of pixels

Among modes with the closest aspect ratio, the program chooses the resolution whose total number of pixels is closest to the requested resolution.

The number of pixels is calculated as:

```text
width × height
```

For example:

```text
1920×1200 = 2,304,000 pixels
1680×1050 = 1,764,000 pixels
1440×900  = 1,296,000 pixels
```

Therefore `1680x1050` is preferred.

### 3. Refresh rate

If multiple modes have the same aspect-ratio and pixel-count difference, the refresh rate is considered.

When no refresh rate is specified on the command line, the highest available refresh rate for the selected resolution is chosen.

A refresh rate can also be explicitly supplied as an additional preference.

## Installation

The program currently has no external C++ libraries or runtime dependencies other than `gdctl`, which is included with modern GNOME/Mutter installations.

Compile it with:

```bash
g++ main.cpp -o resolution-selector
```

Make sure `gdctl` is available:

```bash
command -v gdctl
```

## Usage

```text
resolution-selector <monitor_id> <width> <height>
resolution-selector <monitor_id> <width> <height> <refresh_rate>
```

### Examples

Request `1920x1200` on `HDMI-1`:

```bash
./resolution-selector HDMI-1 1920 1200
```

Request `1920x1080`:

```bash
./resolution-selector HDMI-1 1920 1080
```

Request a resolution with a preferred refresh rate:

```bash
./resolution-selector HDMI-1 1920 1080 120
```

The monitor ID is the connector reported by `gdctl`, such as:

```text
HDMI-1
DP-1
```

Available modes can be inspected with:

```bash
gdctl show --modes
```

## Example

Suppose the monitor provides:

```text
2560x1440@144.000
2560x1440@119.998
2560x1440@59.951
1920x1080@143.981
1920x1080@119.879
1920x1080@60.000
1680x1050@59.954
1440x900@59.887
1280x800@59.810
```

Running:

```bash
./resolution-selector HDMI-1 1920 1200
```

results in:

```text
Requested resolution: 1920x1200

HDMI-1 available resolutions:
---- 2560x1440 144Hz
...
---- 1680x1050 59.954Hz
---- 1440x900 59.887Hz
---- 1280x800 59.81Hz

Selected resolution: 1680x1050 59.954Hz.
```

The program then constructs and executes the corresponding `gdctl` command.

## Sunshine / Moonlight use case

The main reason this program was written is to use it together with **Sunshine**.

A remote client can have a resolution such as:

```text
1920x1200
```

while the host monitor does not support that exact resolution.

Instead of forcing the monitor to a completely different aspect ratio such as `1920x1080`, the program finds the closest supported resolution with the most appropriate aspect ratio.

A typical workflow can therefore be:

```text
Sunshine connection starts
        ↓
Get client resolution
        ↓
Run resolution-selector
        ↓
Monitor changes to closest supported mode
        ↓
Remote session
        ↓
Client disconnects
        ↓
Restore normal monitor configuration
```

For example:

```bash
./resolution-selector HDMI-1 1920 1200
```

may change the monitor to:

```text
1680x1050@59.954
```

because that is the closest available 16:10 mode.

## Restoring the normal configuration

The program intentionally does not attempt to act as a complete display-layout manager.

On my system, the normal configuration contains two monitors:

```text
HDMI-1
    2560x1440@144.000
    position: (1080,344)
    scale: 1.0
    transform: normal
    primary: yes

DP-1
    1920x1080@74.973
    position: (0,0)
    scale: 1.0
    transform: 90
    primary: no
```

After the remote session, I restore this configuration using:

```bash
gdctl set --layout-mode logical \
    --logical-monitor \
      --primary \
      --monitor HDMI-1 \
        --mode 2560x1440@144.000 \
      --scale 1.0 \
      --transform normal \
      --x 1080 \
      --y 344 \
    --logical-monitor \
      --monitor DP-1 \
        --mode 1920x1080@74.973 \
      --scale 1.0 \
      --transform 90 \
      --x 0 \
      --y 0
```

This approach keeps the resolution-selection program small and focused on its actual purpose instead of turning it into a complete monitor configuration utility.

## Why `gdctl`?

GNOME on Wayland does not use `xrandr` in the same way that an X11 session does, and `wlr-randr` is intended for compositors implementing wlroots' output-management protocol.

GNOME uses **Mutter**, and `gdctl` provides a command-line interface for configuring displays through Mutter.

The program therefore uses the tools already provided by GNOME instead of trying to directly manipulate DRM or create custom Wayland display management code.

## Why not add custom resolutions?

The program only selects modes that the monitor and GNOME already report through:

```bash
gdctl show --modes
```

It does not create new modelines or attempt to force unsupported resolutions.

This keeps the program relatively simple and avoids relying on monitor-specific timing information.

For example, if a monitor does not advertise `1920x1200` but does advertise `1680x1050`, the program will select `1680x1050` instead.

## Design

The program intentionally has very few dependencies.

The basic process is:

```text
Command-line arguments
        ↓
Parse requested resolution
        ↓
gdctl show -m
        ↓
Find target monitor
        ↓
Read available modes
        ↓
Select closest mode
        ↓
Construct gdctl command
        ↓
system()
```

`gdctl` output is used because it already provides the display modes that GNOME makes available to the compositor.

The program does not directly interact with Mutter's D-Bus interface. This keeps the project small and makes it possible to build it with a normal C++ compiler without additional libraries.

## Temporary files

The current implementation uses files in the directory containing the executable to store the output of `gdctl` while it is being parsed.

For example:

```text
monitorModes.txt
selctedMode.txt
```

These files are implementation details and may be removed or replaced in future versions.

## Limitations

This project is intentionally small and currently has several limitations:

- It is designed for **GNOME/Wayland** and depends on `gdctl`.
- It only selects modes that are already reported by the monitor.
- It currently relies on parsing the human-readable output of `gdctl`.
- It uses `system()` to execute `gdctl`.
- It does not manage the complete multi-monitor layout itself.
- Restoring the original display configuration is currently handled separately.
- The program assumes the input and monitor configuration are valid for the intended use.

The project is primarily intended as a lightweight personal utility and as an experiment in automating GNOME display configuration rather than as a replacement for GNOME Settings.

## Future improvements

Possible future improvements include:

- removing the temporary files and processing `gdctl` output directly;
- using `popen()` instead of redirecting command output to a file;
- using `std::string` and C++ containers instead of fixed-size character arrays;
- avoiding `system()` and using a safer process-execution method;
- reading the current monitor configuration automatically;
- automatically restoring the previous configuration;
- communicating directly with Mutter over D-Bus instead of parsing `gdctl` output;
- adding a dedicated Sunshine integration script.

## License

Add your preferred license here.

For example:

```text
MIT License
```

or create a `LICENSE` file in the repository.