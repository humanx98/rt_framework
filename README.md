# rt_framework

- git clone --recurse-submodules https://github.com/humanx98/rt_framework.git
- bash scripts/build_hiprt.sh

# Build with Ninja
- bash scripts/ninja/configure.sh
- bash scripts/ninja/build_and_run.sh

# Build with VS
- bash scripts/vs/configure.sh
- open build/vs/rt_framework.sln
- build solution
- set rt_framework as startup project and run

## OptiX renderer

- Install the NVIDIA CUDA Toolkit (12.3 or newer) so `find_package(CUDAToolkit)` can succeed.
- Install the OptiX SDK and set `OPTIX_INSTALL_DIR` (or `OPTIX_ROOT`) to the SDK root that contains `include/optix.h`.
- Optional: set `OPTIX_COMPUTE_CAPABILITY` (default `75`) to control the PTX target passed to `nvcc`.
- Re-run CMake; the configure step will build the `optix_device_ptx` target to generate `OptixMotionBlur.ptx` and link the CUDA renderer into `rt_framework`.

