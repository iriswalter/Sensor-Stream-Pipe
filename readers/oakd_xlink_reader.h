//
// Created by adammpolak on 26-09-2021.
//

#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <vector>

#include "../utils/logger.h"

#include <cereal/archives/binary.hpp>

#include "../structs/frame_struct.h"
#include "../utils/image_decoder.h"
#include "../utils/video_utils.h"
#include "ireader.h"

//Header for making dummy bodies
#include "dummy_body_reader.h"

#include "human_poses.h"

//Depth AI header
#include "depthai/depthai.hpp"
//Done Depth AI header

// OPENVINO HEADERS
/**
 * @brief Define names based depends on Unicode path support
 */
#if defined(ENABLE_UNICODE_PATH_SUPPORT) && defined(_WIN32)
    #define tcout                  std::wcout
    #define file_name_t            std::wstring
    #define imread_t               imreadW
    #define ClassificationResult_t ClassificationResultW
#else
    #define tcout                  std::cout
    #define file_name_t            std::string
    #define imread_t               cv::imread
    #define ClassificationResult_t ClassificationResult
#endif
#include <samples/classification_results.h>
#include <inference_engine.hpp>
#include <iterator>
#include <memory>
#include <samples/common.hpp>
#include <samples/ocv_common.hpp>
#include <string>
#include <vector>

namespace moetsi::ssp { 

using namespace InferenceEngine;
//DONE OPENVINO HEADERS

class OakdXlinkReader : public IReader {
private:

  bool stream_rgb = false;
  bool stream_depth = false;
  bool stream_bodies = false;

  int current_frame_counter_;
  unsigned int fps;
  FrameStruct frame_template_;

  //We use this dictionary to grab pairs of rgb and depth frames that caame from same point in time
  struct color_and_depth { std::shared_ptr<dai::ImgFrame> rgb; std::shared_ptr<dai::ImgFrame> depth;};
  std::unordered_map<int, color_and_depth> frames_dictionary;

  std::vector<std::shared_ptr<FrameStruct>> current_frame_;

  //oakd info
  std::shared_ptr<dai::Pipeline> pipeline;
  std::shared_ptr<dai::node::ColorCamera> camRgb;
  std::shared_ptr<dai::node::MonoCamera> left;
  std::shared_ptr<dai::node::MonoCamera> right;
  std::shared_ptr<dai::node::StereoDepth> stereo;
  std::shared_ptr<dai::node::XLinkOut> rgbOut;
  std::shared_ptr<dai::node::XLinkOut> depthOut;

  std::shared_ptr<dai::DataOutputQueue> q;
  std::shared_ptr<dai::DataOutputQueue> qRgb;
  std::shared_ptr<dai::DataOutputQueue> qDepth;
  std::shared_ptr<dai::node::StereoDepth> depth;
  std::shared_ptr<dai::DeviceInfo> device_info;
  std::shared_ptr<dai::Device> device;
  std::shared_ptr<dai::CalibrationHandler> deviceCalib;
  std::vector<std::vector<float>> cameraIntrinsics;
  float horizontalFocalLengthPixels;
  float verticalFocalLengthPixels;
  float cameraHFOVInRadians;
  
  //openvino info
  const file_name_t input_model = "../models/human-pose-estimation-3d-0001.xml";
  const file_name_t input_image_path= "../models/pointing_middle_of_view.jpg";
  const std::string device_name = "CPU";
  std::shared_ptr<Core> ie;
  std::shared_ptr<CNNNetwork> network;
  std::shared_ptr<InputInfo::Ptr> input_info;
  std::string input_name;
  std::shared_ptr<DataPtr> features_output_info;
  std::shared_ptr<DataPtr> heatmaps_output_info;
  std::shared_ptr<DataPtr> pafs_output_info;
  std::string output_name;
  std::shared_ptr<ExecutableNetwork> executable_network;
  std::shared_ptr<InferRequest> infer_request;

  std::vector<human_pose_estimation::Pose> previous_poses_2d;
  human_pose_estimation::PoseCommon common;

  void SetOrResetInternals();
  // init only vars
  unsigned int rgb_res;
  unsigned int rgb_dai_preview_y;
  unsigned int rgb_dai_preview_x; 
  unsigned int rgb_dai_fps;
  unsigned int depth_res;
  dai::ColorCameraProperties::SensorResolution rgb_dai_res;
  dai::MonoCameraProperties::SensorResolution depth_dai_res;
  unsigned int depth_dai_preview_y;
  unsigned int depth_dai_preview_x;
  unsigned int depth_dai_fps;
  bool depth_dai_sf;
  unsigned int depth_dai_sf_hfr;
  unsigned int depth_dai_sf_num_it;
  std::string ip_name;
  std::string model_path;
  std::shared_ptr<dai::RawStereoDepthConfig> oakdConfig;

  bool failed;
public:
  OakdXlinkReader(YAML::Node config);

  ~OakdXlinkReader();

  void Reset();

  bool HasNextFrame();

  void NextFrame();

  std::vector<std::shared_ptr<FrameStruct>> GetCurrentFrame();

  unsigned int GetCurrentFrameId();

  virtual void GoToFrame(unsigned int frame_id);

  unsigned int GetFps();

  std::vector<FrameType> GetType();
};

} // namespace