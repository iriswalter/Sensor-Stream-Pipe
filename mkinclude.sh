#!/bin/bash

rm -Rvf include
mkdir include
cp -fv ssp.h include
mkdir include/decoders
cp -fv decoders/idecoder.h include/decoders
mkdir include/encoders
cp -fv encoders/iencoder.h include/encoders
mkdir include/readers
cp -fv readers/ireader.h include/readers
mkdir include/structs 
cp -fv structs/frame_struct.h include/structs
mkdir include/utils
cp -fv utils/image_converter.h include/utils