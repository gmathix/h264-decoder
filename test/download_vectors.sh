#!/bin/bash


# Download official ITU-T conformance vectors


if [ -d "AVCv1" ]; then
  rm -rf "AVCv1"
fi
if [ -d "FRExt" ]; then
  rm -rf "FRExt"
fi

wget "https://www.itu.int/wftp3/public/t/testsignal/SpeVideo/H264-1/v2016_02/ITU-T_H.264.1(2016-02)_AVCv1_bitstreams.zip" -O AVCv1.zip
wget "https://www.itu.int/wftp3/public/t/testsignal/SpeVideo/H264-1/v2016_02/ITU-T_H.264.1(2016-02)_FRExt_bitstreams.zip" -O FRExt.zip



# parse both directories and unzip each subfolder
unzip_subdirs() {
  while read -r; do
    name=$(echo $REPLY | cut -d"." -f 1)
    extension=$(echo $REPLY | rev | cut -d"." -f 1 | rev)
    if [ "$extension" = "zip" ]; then
      mkdir "$1/$name"
      unzip "$1/$REPLY" -d "$1/$name" 1> /dev/null
      rm "$1/$REPLY"
    fi
  done <<< "$(ls $1)"
}


echo "Decompressing AVCv1..."
unzip "AVCv1.zip" 1> /dev/null
unzip_subdirs AVCv1

echo "Decompressing FRExt..."
unzip "FRExt.zip" 1> /dev/null
unzip_subdirs FRExt