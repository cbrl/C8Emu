# Hacking

This file tells you how to build this project as a developer.

## Developer mode

The `c8emu_DEVELOPER_MODE` option controls the targets that only developers
use. When the option is on, CMake adds the developer targets. When the option is off, you are a consumer of the project and these
targets are not necessary.

## Presets

This project uses [presets][1] to make the configure step simple. Install the
[latest CMake version][2] to get all the preset features.

Create a `CMakeUserPresets.json` file at the root of the project. Git ignores
this file, so each developer can keep a different one:

```json
{
  "version": 3,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 21,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "dev",
      "binaryDir": "${sourceDir}/build/dev",
      "inherits": ["dev-mode", "vcpkg", "ci-<os>"],
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "dev",
      "configurePreset": "dev",
      "configuration": "Debug"
    }
  ]
}
```

Replace `<os>` with `win64`, `linux` or `darwin`.
[CMakePresets.json](CMakePresets.json) shows what each one sets.

On Windows, `ci-win64` does not set a generator. CMake then uses the newest
Visual Studio that is installed. To use Ninja, set the `CMAKE_GENERATOR`
environment variable and configure from a Visual Studio developer prompt.

> **Note**
> Some editors configure a project with presets as soon as they open it. Make
> sure that your editor configures only when you tell it to. In CLion, enable
> only the `dev` profile in
> `File > Settings... > Build, Execution, Deployment > CMake`. In Visual
> Studio, set `Never run configure step automatically` in
> `Tools > Options > CMake` **before you open the project**. Then configure
> with `Project > Configure Cache`.

## Dependency manager

The `vcpkg` preset uses [vcpkg][vcpkg]. Set the `VCPKG_ROOT` environment
variable to the directory that holds the vcpkg executable.

On Windows, also inherit from the `vcpkg-win64-static` preset. This preset
links the dependencies statically. Then the emulator runs without a change to
`PATH`.

[vcpkg.json](vcpkg.json) sets a `builtin-baseline`. The baseline is a commit
of the vcpkg registry, and it sets the version of each dependency. To upgrade
the dependencies, run `vcpkg x-update-baseline` in the project root.

The [ports](ports) directory holds overlay ports for ImGuiColorTextEdit and
the imgui_club memory editor. The baseline does not control these ports. To
upgrade one, follow these steps:

- `imgui-color-text-edit` uses a release tag of the fork. Change `version` in
  its `vcpkg.json` to the new tag without the `v`. Then change `SHA512` in its
  `portfile.cmake`.
- `imgui-memory-editor` uses a commit. Change `REF` and `SHA512` in its
  `portfile.cmake`. Then change `version-date` in its `vcpkg.json`.

The fork of ImGuiColorTextEdit releases one tag for each Dear ImGui version.
Upgrade it together with the `imgui` port.

## Configure and build

Use these commands from the project root to configure and build the project:

```sh
cmake --preset=dev
cmake --build --preset=dev
```

An editor or IDE with preset support (VS Code, CLion, Visual Studio) can also
use these presets.

The build command accepts a `-j` flag that sets the number of jobs.
You can also put a `jobs` property in your preset. See the
[presets documentation][1].

## Developer mode targets

Use the build command with `-t <target>` to run these targets:

### `run-exe`

This target runs the `c8emu_exe` target from the project root, so the ROM
selector starts next to the `roms` directory.

[1]: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
[2]: https://cmake.org/download/
[vcpkg]: https://github.com/microsoft/vcpkg
