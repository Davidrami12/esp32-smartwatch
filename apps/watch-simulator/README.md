# Watch desktop simulator

This Windows desktop app runs the same LVGL UI as the ESP32 firmware, using SDL2 for display and mouse/keyboard input. The simulator uses the PC clock and fixed mock Wi-Fi/battery values; it does not emulate ESP32 hardware. UI implementation is shared from `components/watch-ui/`.

## Windows prerequisites

- Visual Studio 2019 Build Tools (MSVC C/C++ toolchain and Windows SDK)
- CMake (the Visual Studio CMake installation works)
- vcpkg with the `x64-windows` SDL2 package installed

Example setup used for this repository:

```powershell
D:\PROYECTOS\vcpkg\vcpkg.exe install sdl2:x64-windows
```

Adjust the vcpkg root below if it is installed elsewhere. Make sure CMake is on `PATH`, or invoke the CMake executable installed with Visual Studio.

## Initialize LVGL

From the repository root, initialize the pinned LVGL submodule:

```powershell
git submodule update --init --recursive
```

## Configure and build

From `apps/watch-simulator` (replace the vcpkg path if needed):

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=D:/PROYECTOS/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Debug
```

The CMake project defaults to `USE_FREERTOS=OFF`. FreeRTOS simulation is disabled and is not part of the supported simulator setup.

## Run

From `apps/watch-simulator`:

```powershell
.\bin\Debug\main.exe
```

The simulator window is 410×502. Use mouse input to navigate Home and Settings, change brightness, and select a screen timeout; the simulator reports these setting changes to its console.
