# Building with CMake

## Dependencies

[vcpkg](https://github.com/microsoft/vcpkg) supplies all the dependencies.
[vcpkg.json](vcpkg.json) lists them. The [ports](ports) directory holds
overlay ports for the libraries that the vcpkg registry does not have.
[vcpkg-configuration.json](vcpkg-configuration.json) adds that directory to
vcpkg.

Set the `VCPKG_ROOT` environment variable to the vcpkg directory. Then give
CMake the vcpkg toolchain file. vcpkg installs the dependencies during the
configure step.

## Build

Here are the steps for a release build with a single-configuration generator,
such as Ninja or Unix Makefiles:

```sh
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D CMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

Here are the steps for a release build with a multi-configuration generator,
such as Visual Studio:

```sh
cmake -S . -B build -D CMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

### Static dependencies on Windows

The default `x64-windows` triplet builds the dependencies as DLLs. Add
`-D VCPKG_TARGET_TRIPLET=x64-windows-static-md` to the configure command to
link them statically. The result is one executable with no DLLs to copy.

### Building with MSVC

By default, MSVC does not conform to the standard. You must give it some flags
to make it conform. The `flags-msvc` preset in
[CMakePresets.json](CMakePresets.json) has these flags and the variable that
holds them.

## Install

The project must be built before you install it. These commands need CMake
3.15 or newer.

Here is the command to install a release build from a single-configuration
generator:

```sh
cmake --install build
```

Here is the command to install a release build from a multi-configuration
generator:

```sh
cmake --install build --config Release
```
