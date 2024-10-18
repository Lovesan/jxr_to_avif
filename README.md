# About
This is a simple command line tool for converting HDR JPEG-XR files, such as Windows HDR screenshots, to AVIF.

The output format defaults to 12 bit 4:4:4 for maximum quality. Unfortunately, these files cannot be decoded natively by Windows's AV1 extension, as it only seems to do 8 bit up to 4:4:4 or 10/12 bit up to 4:2:0. However, the files open fine in Chromium.

# Usage
```
Usage: jxr_to_avif [options] input.jxr [output.avif]
Options:
  --help              Print this message.
  --speed <n>         AVIF encoding speed.
                      Must be in range of 0 to 10. Defaults to 6.
  --without-tiling    Do not use tiling.
                      Tiling means slightly larger file size
                      but faster encoding and decoding.
  --depth <n>         Output color depth. May equal 10 or 12.
                      Defaults to 12 bits.
  --format            Output pixel format. Defaults to yuv444.
                      Must be one of:
                        rgb, yuv444, yuv422, yuv420, yuv400
  --real-maxcll      Calculate real MaxCLL
                     instead of top percentile.
```

# HDR metadata
The MaxCLL value is calculated almost identically to [HDR + WCG Image Viewer](https://github.com/13thsymphony/HDRImageViewer) by taking the light level of the 99.99 percentile brightest pixel. This is an underestimate of the "real" MaxCLL value calculated according to H.274, so it technically causes some clipping when tone mapping. However, following the spec can lead to a much higher MaxCLL value, which causes e.g. Chromium's tone mapping to significantly dim the entire image, so this trade-off seems to be worth it.

# Building with MSVC++

You will need **CMake**, **NASM**, **Perl** and **Visual Studio 2022 build tools**.

Make sure that git, cmake, nasm and perl are in your PATH.

First, you need to clone and build **libavif** dependencies.

Open PowerShell, cd into the repo directory.

Change dir to `ext` directory inside `./libavif`.
````powershell
cd ./libavif/ext
````

Build **libyuv**:
````powershell
git clone --single-branch https://chromium.googlesource.com/libyuv/libyuv
cd ./libyuv
git checkout a6a2ec65
cd ..

cmake -B ./libyuv/build -S ./libyuv -G 'Visual Studio 17 2022'

cmake --build ./libyuv/build --parallel --config Release --target yuv

cp ./libyuv/build/Release/*.lib ./libyuv/build
````

Build **libaom**:
````powershell
git clone -b v3.9.1 --depth 1 https://aomedia.googlesource.com/aom

cmake -B ./aom/build.libavif -S ./aom `
      -G 'Visual Studio 17 2022' `
      -DENABLE_NASM=ON -DBUILD_SHARED_LIBS=OFF -DCONFIG_PIC=1 -DENABLE_DOCS=0 `
      -DENABLE_EXAMPLES=0 -DENABLE_TESTDATA=0 -DENABLE_TESTS=0 -DENABLE_TOOLS=0

cmake --build ./aom/build.libavif --parallel --config Release

cp ./aom/build.libavif/Release/*.lib ./aom/build.libavif
````

Now configure and build the program itself:

````powershell
cd ../../
cmake --preset MSVC
cmake --build --preset MSVC --parallel --config Release
````

Now you have the progam at `./build/MSVC/Release/jxr_to_avif.exe`
