# h264-decoder

A toy H.264 decoder written in C.

## *** WIP ***
## Project Philosophy 
### Skip the shower, ship the decoder. Priorities.

## Current Status
Achieving the Intra8x8 side quest

## Features and Scope
Baseline profile, Main profile without CABAC :
- Single threaded
- CAVLC entropy coding
- I/P/B slices
- Weighted prediction
- Deblocking filter
- Memory management control operations (MMCOs)
- Reference picture list modifications
- YUV 4:2:0 output

Does not support yet : 
- CABAC entropy coding
- High profile (intra8x8 modes, custom quantization scaling matrices)

Does and will not support :
- MBAFF/PAFF
- Picture field coding
- FMO (multiple slice groups)
- ASO (arbitrary slice ordering)
- Any format other than 4:2:0
- Any profile other than Baseline, Main or High


## Performance
Not great for now.  
It can reach ~22fps on a single thread on my Intel I5-10300H, on 1080p streams, without dumping the frames. When writing to the output YUV file, it's slowly lurking around at ~16fps.  
Compiler optimizations help a lot : without -O3, it : 
  - runs at ~5fps (feel free to laugh at me)    
  - looks like color planes are corrupted (feel free to laugh at me harder)  

But, correctness and robustness first, performance second.

I'd like to understand how the GCC managed to achieve that 5 -> 26 jump, but I'd have to learn assembly and that's a different learning path which I will eventually take when this project is finished. 


## Usage
As simple as, for example : 
```shell
./h264_decoder videos/256x256_radial.h264 output.yuv
```
I made a few synthetic gradient streams located in the videos/h264/ folder, but they suck, so :  
If you want to use it on your own video, you'll likely have to re-encode it, because virtually all H.264 streams (except professional editing content) are coded using either Main or High profile, which this decoder does not fully support yet.  
Therefore you'd need to run a command like this one : 
```shell
ffmpeg -i input.264 -c:v libx264 -profile:v main -x264-params "no-cabac:1" -preset slow -pix_fmt yuv420p -an output.264
```
This will output a stream in Main profile, without CABAC.  
Use  ```--preset veryslow``` if you want to minimize quality loss.


## License
MIT, see LICENSE file for details.