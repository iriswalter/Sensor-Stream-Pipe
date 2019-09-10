//
// Created by amourao on 07/08/19.
//

#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>

#include <zmq.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include "../encoders/FrameEncoder.h"
#include "../readers/FrameReader.h"
#include "../structs/FrameStruct.hpp"
#include <cv.hpp>

#include "SimilarityMeasures.h"
#include "Utils.h"
#include "VideoUtils.h"
#include "KinectUtils.h"
#include "../readers/KinectReader.h"

int main(int argc, char *argv[]) {
  srand(time(NULL) * getpid());
  // srand(getpid());

  double psnr = 0;
  cv::Scalar mssim;
  int i = 0;
  int j = 0;

  uint64_t original_size = 0;
  uint64_t compressed_size = 0;

  std::unordered_map<std::string, AVCodec *> pCodecs;
  std::unordered_map<std::string, AVCodecContext *> pCodecContexts;
  std::unordered_map<std::string, AVCodecParameters *> pCodecParameters;

  AVPacket *pPacket = av_packet_alloc();
  AVFrame *pFrame = av_frame_alloc();

  cv::Mat img;
  bool imgChanged = false;

  av_register_all();

  if (argc < 3) {
    std::cerr << "Usage: kinect_encoder_test <codec parameters> <time in seconds>"
              << std::endl;
    return 1;
  }
  std::string codec_parameters_file = std::string(argv[1]);
  uint time_in_seconds = std::stoul(argv[2]);

  YAML::Node codec_parameters = YAML::LoadFile(codec_parameters_file);

  ExtendedAzureConfig c =
          buildKinectConfigFromYAML(codec_parameters["kinect_parameters"][0]);

  KinectReader reader(0, c);

  // TODO: use smarter pointers
  std::unordered_map<uint, FrameEncoder *> encoders;

  std::vector<uint> types;
  types.push_back(0);
  types.push_back(1);
  types.push_back(2);

  for (uint type : types) {
    YAML::Node v = codec_parameters["video_encoder"][type];
    FrameEncoder *fe = new FrameEncoder(v, reader.getFps());
    encoders[type] = fe;
  }

  std::queue<FrameStruct> buffer;

  uint64_t start_time = currentTimeMs();
  // This class only reads the file once
  while ((currentTimeMs() - start_time) <= time_in_seconds * 1000) {

    std::vector<FrameStruct> v;

    // TODO: document what is happening with the Encoders and Queue
    while (v.empty()) {

      std::vector<FrameStruct *> frameStruct = reader.currentFrame();
      for (FrameStruct *frameStruct : frameStruct) {
        FrameEncoder *frameEncoder = encoders[frameStruct->frameType];

        frameEncoder->addFrameStruct(frameStruct);
        if (frameEncoder->hasNextPacket()) {
          FrameStruct f = *frameEncoder->currentFrame();
          FrameStruct fo = *frameEncoder->currentFrameOriginal();
          original_size += fo.frame.size();
          compressed_size += f.frame.size();
          v.push_back(f);
          buffer.push(fo);

          frameEncoder->nextPacket();

        }

      }

      reader.nextFrame();
    }

    FrameStruct f = v.front();

    if (pCodecs.find(f.streamId + std::to_string(f.sensorId)) ==
        pCodecs.end()) {
      prepareDecodingStruct(&f, pCodecs, pCodecContexts, pCodecParameters);
    }

    AVCodecContext *pCodecContext =
            pCodecContexts[f.streamId + std::to_string(f.sensorId)];

    pPacket->data = &f.frame[0];
    pPacket->size = f.frame.size();

    int response = avcodec_send_packet(pCodecContext, pPacket);
    // std::cout << "avcodec_send_packet: " << response << " " <<
    // av_err2str(response) << std::endl;
    while (response >= 0) {
      // Return decoded output data (into a frame) from a decoder
      response = avcodec_receive_frame(pCodecContext, pFrame);
      // std::cout << "avcodec_receive_frame: " << response << " " <<
      // av_err2str(response) << std::endl;
      if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
        break;
      }

      if (response >= 0) {
        i++;

        if (pCodecContext->pix_fmt == AV_PIX_FMT_GRAY12LE || pCodecContext->pix_fmt == AV_PIX_FMT_GRAY16BE) {
          avframeToMatGray(pFrame, img);
        } else {
          avframeToMatYUV(pFrame, img);
        }

        imgChanged = true;
      }
    }
    std::cout << f.deviceId << ";" << f.sensorId << ";" << f.frameId
              << " received; size " << f.frame.size() << std::endl;

    if (!img.empty() && imgChanged) {
      FrameStruct fo = buffer.front();
      buffer.pop();
      cv::Mat frameOri;
      cv::Mat frameDiff;
      if (fo.frameDataType == 0) {
        ImageDecoder id;
        AVFrame *frame = av_frame_alloc();
        id.imageBufferToAVFrame(fo.frame, frame);
        int width = frame->width;
        int height = frame->height;

        SwsContext *conversion;

        frameOri = cv::Mat(height, width, CV_8UC3);
        conversion =
                sws_getContext(width, height, (AVPixelFormat) frame->format, width, height,
                               AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
        int cvLinesizes[1];
        cvLinesizes[0] = img.step1();

        // Convert the colour format and write directly to the opencv matrix
        sws_scale(conversion, frame->data, frame->linesize, 0, height, &frameOri.data,
                  cvLinesizes);
        sws_freeContext(conversion);
        av_frame_free(&frame);

      } else if (fo.frameDataType == 2) {
        int rows, cols;
        memcpy(&cols, &fo.frame[0], sizeof(int));
        memcpy(&rows, &fo.frame[4], sizeof(int));
        frameOri = cv::Mat(rows, cols, CV_8UC4, (void *) &fo.frame[8], cv::Mat::AUTO_STEP);
      } else if (fo.frameDataType == 3) {
        int rows, cols;
        memcpy(&cols, &fo.frame[0], sizeof(int));
        memcpy(&rows, &fo.frame[4], sizeof(int));
        frameOri = cv::Mat(rows, cols, CV_16UC1, (void *) &fo.frame[8], cv::Mat::AUTO_STEP);
      }

      if (f.frameType == 1 && pCodecContext->pix_fmt == AV_PIX_FMT_GRAY12LE) {

        cv::Mat frameOriSquached;
        minMaxFilter<ushort>(frameOri, frameOriSquached, 0,
                             MAX_DEPTH_VALUE_12_BITS);
        psnr += getPSNR(frameOriSquached, img, MAX_DEPTH_VALUE_12_BITS);

        img *= (MAX_DEPTH_VALUE_8_BITS / (float) MAX_DEPTH_VALUE_12_BITS);
        frameOri *= (MAX_DEPTH_VALUE_8_BITS / (float) MAX_DEPTH_VALUE_12_BITS);

        img.convertTo(img, CV_8U);
        frameOri.convertTo(frameOri, CV_8U);

        absdiff(frameOri, img, frameDiff);
        mssim += getMSSIM(frameOri, img);

        cv::applyColorMap(img, img, cv::COLORMAP_JET);
        cv::applyColorMap(frameOri, frameOri, cv::COLORMAP_JET);
        cv::applyColorMap(frameDiff, frameDiff, cv::COLORMAP_HOT);
      } else if (f.frameType == 1 && pCodecContext->pix_fmt == AV_PIX_FMT_GRAY16BE) {

        psnr += getPSNR(frameOri, img, MAX_DEPTH_VALUE_12_BITS);
        img *= (MAX_DEPTH_VALUE_8_BITS / (float) MAX_DEPTH_VALUE_12_BITS);
        frameOri *= (MAX_DEPTH_VALUE_8_BITS / (float) MAX_DEPTH_VALUE_12_BITS);

        img.convertTo(img, CV_8U);
        frameOri.convertTo(frameOri, CV_8U);

        absdiff(frameOri, img, frameDiff);
        mssim += getMSSIM(frameOri, img);

        cv::applyColorMap(img, img, cv::COLORMAP_JET);
        cv::applyColorMap(frameOri, frameOri, cv::COLORMAP_JET);
        cv::applyColorMap(frameDiff, frameDiff, cv::COLORMAP_HOT);


      } else if (f.frameType == 1) {

        cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);

        img.convertTo(img, CV_16U);
        //img *= MAX_DEPTH_VALUE_12_BITS/MAX_DEPTH_VALUE_8_BITS;

        cv::Mat frameOriSquached;
        minMaxFilter<ushort>(frameOri, frameOriSquached, 0,
                             MAX_DEPTH_VALUE_12_BITS);
        psnr += getPSNR(frameOriSquached, img, MAX_DEPTH_VALUE_12_BITS);
        mssim += getMSSIM(frameOriSquached, img);
        img.convertTo(img, CV_8U);

        frameOri *= (MAX_DEPTH_VALUE_8_BITS / (float) MAX_DEPTH_VALUE_12_BITS);

        frameOri.convertTo(frameOri, CV_8U);

        absdiff(frameOri, img, frameDiff);

        cv::applyColorMap(img, img, cv::COLORMAP_JET);
        cv::applyColorMap(frameOri, frameOri, cv::COLORMAP_JET);
        cv::applyColorMap(frameDiff, frameDiff, cv::COLORMAP_HOT);

      } else if (f.frameType == 2) {

        if (img.type() == CV_8UC3) {
          cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
          img.convertTo(img, CV_16UC1);
        }

        //img *= MAX_DEPTH_VALUE_12_BITS/MAX_DEPTH_VALUE_8_BITS;

        psnr += getPSNR(frameOri, img, MAX_DEPTH_VALUE_12_BITS);
        mssim += getMSSIM(frameOri, img);
        img.convertTo(img, CV_8U);

        frameOri *= (MAX_DEPTH_VALUE_8_BITS / (float) MAX_DEPTH_VALUE_12_BITS);

        frameOri.convertTo(frameOri, CV_8U);

        absdiff(frameOri, img, frameDiff);

      } else {
        if (frameOri.channels() == 4)
          cv::cvtColor(frameOri, frameOri, COLOR_BGRA2BGR);
        absdiff(frameOri, img, frameDiff);
        psnr += getPSNR(frameOri, img, MAX_DEPTH_VALUE_8_BITS);
        mssim += getMSSIM(frameOri, img);
      }

      cv::namedWindow("Original");
      cv::imshow("Original", frameOri);
      cv::namedWindow("Encoded");
      cv::imshow("Encoded", img);
      cv::namedWindow("Diff");

      cv::imshow("Diff", frameDiff);

      cv::waitKey(1);
      imgChanged = false;
    }
  }
  std::cout << "Avg PSNR: " << psnr / i << std::endl;
  std::cout << "Avg MSSIM: " << mssim / i << std::endl;
  std::cout << "original_size: " << original_size << " bytes" << std::endl;
  std::cout << "compressed_size: " << compressed_size << " bytes, fps: " << (i / time_in_seconds) << ", bitrate: "
            << (8 * compressed_size) / (1000000.0 * (i / time_in_seconds)) << " Mbps" << std::endl;
  std::cout << "ratio: " << original_size / compressed_size << "x" << std::endl;
  return 0;
}