# undo264

A H.264 decoder written in C


## Features and Scope
Baseline profile, Main and High profile:
- Single threaded
- CAVLC (Context-Adaptive Variable Length Coding)
- CABAC (Context-Adaptive Binary-Arithmetic Coding)
- I/P/B slices
- Weighted prediction
- 8x8 transforms 
- Custom scaling lists
- Deblocking filter
- Memory management control operations (MMCOs)
- Long-term reference pictures
- YUV 4:2:0 and monochrome output

Does and will not support :
- MBAFF/PAFF
- Picture field coding
- FMO (multiple slice groups)
- ASO (arbitrary slice ordering)
- Any format other than 4:2:0 or monochrome
- Profiles: CAVLC 4:4:4, High 10-bit, High 4:2:2, High 4:4:4


## Correctness and Testing

Undo264 gives bit-exact output on all official ITU-T conformance bitstreams for Baseline, Main and High profiles
(AVCv1 and FRExt suites), excluding bitstreams that are out of this decoder's scope. 

A script for downloading the AVCv1 (Baseline, Main) and FRExt (High) suites is available in the test/ folder, 
as well as an automated script to run those test suites 
and compare undo264's output either against given reference decoded files or against FFmpeg's H.264 decoder output, 
which is supposed to be bit-exact. 

To run the test suites in the test/ folder : 
```shell
chmod +x download_vectors.sh && chmod +d conformance_test.sh
./download_vectors.sh # may take a while to download and extract
./conformance_test.sh AVCv1
./conformance_test.sh FRExt
```


## Performance
It can reach ~22fps on a single thread on my Intel I5-10300H, on 1080p streams, without dumping the frames. 
The deblocking filter is the primary bottleneck ; without it, undo264 runs at 90fps on High profile content, 
and at 130fps on Baseline profile content. 

Hence, current optimization work is focused on the deblocking filter (architectural improvements + SIMD rewrite)
reducing as much as possible unnecessary memory movement.

## Building

Requirements: CMake >= 3.20, a C11 compiler (GCC or Clang) and optionally Ninja.

### Quick start
```shell
cmake --preset release
cmake --build --preset release
./build/release/undo264 <input.264> <output.yuv>
```

### Available presets

| Preset    | Build type      | Notes                                  |
|-----------|------------------|-----------------------------------------|
| `release` | Release (-O3)    | Default for normal use                  |
| `debug`   | Debug + ASan/UBSan | Use while developing / hunting bugs   |

### Options

| Option            | Default | Effect                                     |
|--------------------|---------|--------------------------------------------|
| `USE_NATIVE_ARCH`  | OFF     | Adds `-march=native`. Only for local builds |
| `USE_SANITIZERS`   | OFF     | Enables ASan + UBSan                       |
| `BUILD_TOOLS`      | ON      | Builds `gen_rgb_video` and `compare_streams` |

### Installing

```shell
cmake --install build/release --prefix /usr/local
```


## License
MIT, see LICENSE file for details.