/**
 * \file libav_decoder.h @brief Jpeg/Mpeg decoder
 */
// Created by amourao on 12-09-2019.
#pragma once

extern "C" {
#ifdef FFMPEG_AS_FRAMEWORK
#include <FFmpeg/avcodec.h>
#include <FFmpeg/avformat.h>
#include <FFmpeg/avutil.h>
#include <FFmpeg/pixdesc.h>
#include <FFmpeg/swscale.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#endif
}

#include "../utils/logger.h"
#include <iostream>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

#include "../utils/video_utils.h"
#include "../utils/libav_types.h"

#include "idecoder.h"

namespace moetsi::ssp {

/**
 * @brief AV (Jpeg/Mpeg) decoder
 */
class LibAvDecoder : public IDecoder {
private:
  AVCodecSafeP codec_;
  AVCodecContextSafeP codec_context_;
public:
  /** @brief constructor */
  LibAvDecoder();
  /** @brief destructor */
  ~LibAvDecoder();
  /**
   *  @brief Initialize. 
   *  \param codec_parameters parameters
   */  
  void Init(AVCodecParameters *codec_parameters);
  /**
   * @brief Extract an opencv image from a FrameStruct
   * \param data FrameStruct
   * \return OpenCV matrix/image
   */  
  cv::Mat Decode(FrameStruct& frame_struct);
  /**
   * @brief Decode frame to libav AVFrame structure
   * \param frame_struct SSP FrameStruct
   * \return Libav AVFrame structure
   */
  AVFrameSharedP DecodeFrame(FrameStruct &frame_struct);
};

} // namespace moetsi::ssp
