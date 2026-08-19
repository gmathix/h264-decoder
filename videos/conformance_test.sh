#!/usr/bin/env bash
#
# Usage: ./run_conformance.sh <input_dir>
#   <input_dir> is e.g. AVCv1/ or FRExt/, containing one subdirectory per
#   conformance stream, each with a .264 file inside.
#
# For each subdirectory found:
#   - locates the .264 bitstream
#   - decodes it with your decoder  -> <stream>_mine.yuv
#   - decodes it with FFmpeg        -> <stream>_ffmpeg.yuv (native pix_fmt, no resampling)
#   - byte-compares the two; if not bit-exact, runs FFmpeg's psnr filter and
#     reports y/u/v/average PSNR   -> <stream>_psnr.log (+ per-frame stats)
# All outputs are written into the same subdirectory as the source .264.

set -uo pipefail

DECODER="../cmake-build-debug/h264_decoder"

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <input_dir>" >&2
    exit 1
fi

INPUT_DIR="$1"

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

# Iterate over immediate subdirectories only
# shellcheck disable=SC2095
while IFS= read -r -d '' subdir; do
    stream_name="$(basename "$subdir")"

    # Find .264 or .jsv file in this subdir (case-insensitive extension), take the first match
    mapfile -d '' bitstreams < <(find "$subdir" -maxdepth 1 \(  -iname '*264' -o -iname '*.jsv' -o -iname '*.jvt' -o -iname '*.26l' \) -print0)

    if [[ ${#bitstreams[@]} -eq 0 ]]; then
        echo "[SKIP] $stream_name: no .264/.jsv file found"
        continue
    fi
    if [[ ${#bitstreams[@]} -gt 1 ]]; then
        echo "[WARN] $stream_name: multiple bitstream files found, using first: ${bitstreams[0]}"
    fi

    bitstream="${bitstreams[0]}"
    out_mine="${subdir}/${stream_name}_mine.yuv"
    out_ffmpeg="${subdir}/${stream_name}_ffmpeg.yuv"
    out_ref="${subdir}/${stream_name}.yuv"

    echo
    echo "== $stream_name =="

    # --- decode with your decoder ---
    if "$DECODER" "$bitstream" "$out_mine" > "${subdir}/${stream_name}_mine.log" 2>&1; then
        echo "  [OK]   custom decoder -> $(basename "$out_mine")"
    else
        echo "  [FAIL] custom decoder (see ${stream_name}_mine.log)"
        ((fail_count++))
        continue
    fi

    # --- decode with ffmpeg, preserving native pixel format (no resampling) ---
    pix_fmt="$(ffprobe -v error -select_streams v:0 -show_entries stream=pix_fmt \
                -of default=noprint_wrappers=1:nokey=1 "$bitstream")"

    if [[ -z "$pix_fmt" ]]; then
        echo "  [FAIL] ffprobe could not determine pix_fmt"
        ((fail_count++))
        continue
    fi

    if ! ffmpeg -y -v error -vsync 0 -i "$bitstream" -f rawvideo -pix_fmt "$pix_fmt" \
        "$out_ffmpeg" > "${subdir}/${stream_name}_ffmpeg.log" 2>&1; then
        echo "  [FAIL] ffmpeg (see ${stream_name}_ffmpeg.log)"
        ((fail_count++))
        continue
    fi
    echo "  [OK]   ffmpeg ($pix_fmt) -> $(basename "$out_ffmpeg")"
    ((ok_count++))

    # --- compare the two raw YUVs ---
    if cmp -s "$out_mine" "$out_ffmpeg"; then
        echo "  [EXACT] bit-exact match"
        ((exact_count++))
        continue
    fi

    ((mismatch_count++))

    if [[ -s "$out_mine" ]]; then
        # not bit-exact: get PSNR from ffmpeg's psnr filter, feeding both raw
        # YUVs back in with explicit format/size so no guessing is involved
        resolution="$(ffprobe -v error -select_streams v:0 -show_entries stream=width,height \
                        -of csv=s=x:p=0 "$bitstream")"
        psnr_log="${subdir}/${stream_name}_psnr.log"
        perframe_log="${subdir}/${stream_name}_psnr_perframe.log"

        ffmpeg -y -f rawvideo -pix_fmt "$pix_fmt" -s "$resolution" -i "$out_mine" \
                  -f rawvideo -pix_fmt "$pix_fmt" -s "$resolution" -i "$out_ffmpeg" \
                  -lavfi "psnr=stats_file=${perframe_log}" -f null - > "$psnr_log" 2>&1

        summary="$(grep -o 'PSNR .*' "$psnr_log" | tail -1)"
        if [[ -n "$summary" ]]; then
            echo "  [DIFF]  not bit-exact: $summary"
        else
            echo "  [DIFF]  not bit-exact, PSNR filter failed (likely size/frame-count mismatch, see $psnr_log)"
        fi
    else
        echo "  [DIFF]  not bit-exact ($(basename "$out_mine") is empty)"
    fi

done < <(find "$INPUT_DIR" -mindepth 1 -maxdepth 1 -type d -print0 | sort -z)

echo
echo "Done. $ok_count streams fully decoded, $fail_count failures."
echo "Of the decoded streams: $exact_count bit-exact, $mismatch_count differ from ffmpeg."
[[ $fail_count -eq 0 && $mismatch_count -eq 0 ]]