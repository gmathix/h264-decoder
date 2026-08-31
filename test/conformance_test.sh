#!/bin/bash

# Usage: ./conformance_test.sh <input_dir>
#   <input_dir> is e.g. AVCv1/ or FRExt/
#
# For each subdirectory found:
#   - locates the h264 bitstream
#   - decodes it with Undo264 if it is scoped to Undo264 (<stream_name>_undo264.yuv)
#   - decodes it with ffmpeg if a reference yuv file is not found or there are multiple (<stream_name>_ffmpeg.yuv)
#   - byte-compares the two, if not bit-exact runs ffmpeg's psnr filter and
#       reports y/u/v/average psnr   -> <stream_name>_psnr.log + <stream_name>_psnr_perframe.log
# All outputs are written into the same subdirectory as the source h264 bitstream
# Will be skipped :
#   - Field coding
#   - MBAFF/PAFF
#   - High 4:2:2
#   - High 10-bit
#   - POC type 1



DECODER="../build/release/undo264"
INPUT_DIR="$1"

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <input_dir>" >&2
    exit 1
fi


if [[ ! -d "$INPUT_DIR" ]]; then
    echo "Error: '$INPUT_DIR' is not a directory" >&2
    exit 1
fi

if [[ ! -x "$DECODER" ]]; then
    echo "Error: decoder not found or not executable at $DECODER" >&2
    exit 1
fi

fail_count=0
ok_count=0
exact_count=0
mismatch_count=0
outscope_bitstreams=0
inscope_bitstreams=0

# shellcheck disable=SC2095
while IFS= read -r -d '' subdir; do
    stream_name="$(basename "$subdir")"

    echo
    echo "== $stream_name =="


    # remove previous decoding traces
    mapfile -d '' prev_dec < <(find "$subdir" -maxdepth 2 -iname "*undo264.yuv" -print0)
    [[ ${#prev_dec[@]} -gt 0 ]] && rm -- "${prev_dec[@]}"


    # Find .264/.jsv/.jvt/.26l/.avc file in this subdir, take the first match
    mapfile -d '' bitstreams < <(find "$subdir" -maxdepth 2 \(  -iname '*264' -o -iname '*.jsv' \-o -iname '*.jvt' -o -iname '*.26l' -o -iname "*.avc" \) -print0)

    # Find decoded reference file (.yuv/.cif/.qcif) in this subdir
    mapfile -d '' dec_refs < <(find "$subdir" -maxdepth 2 \(  -iname '*.yuv' -o -iname '*.cif' -o -iname '*.qcif' \) -print0)
    use_ffmpeg=0

    if [[ ${#bitstreams[@]} -eq 0 ]]; then
        echo "  [SKIP] $stream_name: no .264/.jsv/.jvt/.26l/.avc file found"
        continue
    fi
    if [[ ${#bitstreams[@]} -gt 1 ]]; then
        echo "  [WARN] $stream_name: multiple bitstream files found, using first: ${bitstreams[0]}"
    fi

     if [[ ${#dec_refs[@]} -eq 0 ]]; then
          echo "  [WARN] $stream_name: no .yuv/.qcif/.cif file found, will use ffmpeg as reference"
          use_ffmpeg=1
      fi
      if [[ ${#dec_refs[@]} -gt 1 ]]; then
          echo "  [WARN] $stream_name: multiple decoded reference files found, using ffmpeg"
          use_ffmpeg=1
      fi



    bitstream="${bitstreams[0]}"
    undo264_out="${subdir}/${stream_name}_undo264.yuv"

    dec_ref=""
    if [[ $use_ffmpeg -eq 0 ]]; then
      dec_ref="${dec_refs[0]}"
    else
      dec_ref="${subdir}/${stream_name}_ffmpeg.yuv"
    fi



    # decode with undo264
    "$DECODER" "$bitstream" "$undo264_out" > "${subdir}/${stream_name}_undo264.log" 2>&1
    ret=$?
    if [[ $ret -eq 0 ]]; then
        echo "  [OK]    Undo264 -> $(basename "$undo264_out")"
        ((inscope_bitstreams++))
    elif [[ $ret -eq 67 ]]; then
        echo "  [SKIP] Out-of-scope bitstream (see ${stream_name}_undo264.log)"
        ((outscope_bitstreams++))
        continue
    else
        echo "  [FAIL] Undo264 (see ${stream_name}_undo264.log)"
        ((inscope_bitstreams++))
        ((fail_count++))
        continue
    fi


    pix_fmt="$(ffprobe -v error -select_streams v:0 -show_entries stream=pix_fmt \
        -of default=noprint_wrappers=1:nokey=1 "$bitstream")"
    if [[ -z "$pix_fmt" ]]; then
        echo "  [FAIL] ffprobe could not determine pix_fmt"
        ((fail_count++))
        continue
    fi

    # decode with ffmpeg if needed
    if [[ $use_ffmpeg -eq 1 ]]; then
        if ! ffmpeg -y -v error -i "$bitstream" -f rawvideo -fps_mode passthrough -pix_fmt "$pix_fmt" \
            "$dec_ref" > "${subdir}/${stream_name}_ffmpeg.log" 2>&1; then
            echo "  [FAIL] ffmpeg (see ${stream_name}_ffmpeg.log)"
            ((fail_count++))
            continue
        fi
        echo "  [OK]    ffmpeg ($pix_fmt) -> $(basename "$dec_ref")"
    else
        echo "  [OK]    ref $dec_ref"
    fi

    ((ok_count++))


    # compare the two raw YUVs
    if cmp -s "$undo264_out" "$dec_ref"; then
        echo "  [EXACT] bit-exact match"
        ((exact_count++))
        continue
    fi

    ((mismatch_count++))

    if [[ -s "$undo264_out" ]]; then
        # not bit-exact: get psnr from ffmpeg
        resolution="$(ffprobe -v error -select_streams v:0 -show_entries stream=width,height \
                        -of csv=s=x:p=0 "$bitstream")"
        psnr_log="${subdir}/${stream_name}_psnr.log"
        perframe_log="${subdir}/${stream_name}_psnr_perframe.log"

        ffmpeg -y -f rawvideo -pix_fmt "$pix_fmt" -s "$resolution" -i "$undo264_out" \
                  -f rawvideo -pix_fmt "$pix_fmt" -s "$resolution" -i "$dec_ref" \
                  -lavfi "psnr=stats_file=${perframe_log}" -f null - > "$psnr_log" 2>&1

        summary="$(grep -o 'PSNR .*' "$psnr_log" | tail -1)"
        if [[ -n "$summary" ]]; then
            echo "  [DIFF]  not bit-exact: $summary"
        else
            echo "  [DIFF]  not bit-exact, PSNR filter failed (probably size/frame-count mismatch, see $psnr_log)"
        fi
    else
        echo "  [DIFF]  not bit-exact ($(basename "$undo264_out") is empty)"
    fi

done < <(find "$INPUT_DIR" -mindepth 1 -maxdepth 1 -type d -print0 | sort -z)

echo
echo "Done"
echo "${inscope_bitstreams}/$((inscope_bitstreams+outscope_bitstreams)) in-scope bitstreams"
echo "${ok_count}/$((ok_count+fail_count)) fully decoded bitstreams"
echo "${exact_count}/$((exact_count+mismatch_count)) bit-exact among them"

if [[ $mismatch_count -eq 0 ]]; then
    exit 0
else
    exit 1
fi