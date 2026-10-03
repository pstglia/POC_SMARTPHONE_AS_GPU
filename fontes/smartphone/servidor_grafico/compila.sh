mkdir build ; cd build

# Altere o caminho abaixo para a pasta exata onde você descompactou o seu Android NDK
PATH_DO_NDK="$HOME/projetos/gpu_smartphone/tools/android-ndk-r26d"

cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$PATH_DO_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-29 \
  -DCMAKE_BUILD_TYPE=Release

make

