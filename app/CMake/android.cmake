# Target Android 11 (API 30)
set(ANDROID_PLATFORM 30)

# Use shared C++ standard library (libc++_shared.so)
set(ANDROID_STL c++_shared)

# Use Clang compiler
set(ANDROID_TOOLCHAIN clang)

# Enables C++ exceptions and Run-Time Type Information
set(ANDROID_CPP_FEATURES exceptions rtti)

# Enable ARM NEON (Advanced SIMD) optimizations for ARM CPUs
set(ANDROID_ARM_NEON TRUE)