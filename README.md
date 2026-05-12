# h264-decoder

A toy H.264 decoder written from scratch in C.

## *** WIP ***
### I'm actively working on this!

## Scope
It can just decode streams that use Baseline profile for now, and just supports 4:2:0 planar input/output.   
My primary goal is to work up to the full High profile. 


## Performance
Not great for now.  
It can reach ~43fps on a single thread, on 1080p streams, without dumping the frames. When writing to the output YUV file, it reaches ~32fps.  
Compiler optimizations help a lot : without -O3, it runs at ~8fps (feel free to laugh at me)    

I'd like to understand how the GCC managed to achieve that 8 -> 35 jump, but I'd have to learn assembly and that's a different learning path which I will eventually take when this project is finished. 


## Usage
As simple as, for example : 
```shell
./h264_decoder videos/256x256_radial.h264 output.yuv
```
I made a few synthetic gradient streams located in the videos/h264/ folder.

Though, if you want to use it on your own video, you'll likely have to re-encode it, because virtually all H.264 streams (except professional editing content) are coded using either Main or High profile, which this decoder does not support yet.  
Therefore you'd need to run a command like this one : 
```shell
ffmpeg -i input.h264 -c:v libx264 -profile:v baseline -preset slow -pix_fmt yuv420p -an output.h264
```
This will output a stream in Baseline profile, without CABAC.  
Use  ```--preset veryslow``` if you want to minimize quality loss.