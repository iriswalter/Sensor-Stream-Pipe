/**
 * \file network_reader.h @brief Network reader
 */
// Created by amourao on 27-09-2019.
#pragma once

#include <zmq.hpp>
#include "../structs/frame_struct.h"
#include "ireader.h"

#define POLL_TIMEOUT_MS 500

namespace moetsi::ssp {

/**
 * @brief Network reader
 */
class NetworkReader {

private:
  uint64_t last_time_;
  uint64_t start_time_;
  uint64_t rec_frames_;
  double rec_mbytes_;

  int current_frame_counter_;

  std::unordered_map<std::string, double> rec_mbytes_per_stream_;
  std::vector<FrameStruct> current_frame_internal_;

  // void *context = nullptr;
  // void *responder = nullptr;

  int port_;
  std::unique_ptr<zmq::context_t> context_;
  std::unique_ptr<zmq::socket_t> socket_;

#ifdef SSP_WITH_ZMQ_POLLING
  zmq::poller_t<> poller_;
#endif

public:
  NetworkReader(int port);
  void init();

  ~NetworkReader();

  bool HasNextFrame();

  void NextFrame();

  std::vector<FrameStruct> GetCurrentFrame();

  unsigned int GetCurrentFrameId();

};

} // namespace moetsi::ssp
