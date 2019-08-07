//
// Created by amourao on 26-06-2019.
//

#include <unistd.h>


#include <ctime>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <thread>

#include <zmq.hpp>

#include "../structs/FrameStruct.hpp"
#include "../readers/FrameReader.h"
#include "../utils/Utils.h"

int main(int argc, char *argv[]) {

    srand(time(NULL) * getpid());

    try {


        if (argc < 4) {
            std::cerr << "Usage: server <host> <port> <frame_list> (<stop after>)" << std::endl;
            return 1;
        }

        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_PUSH);


        std::string host = std::string(argv[1]);
        uint port = std::stoul(argv[2]);

        std::string name = std::string(argv[3]);

        int stopAfter = INT_MAX;
        if (argc >= 5) {
            stopAfter = std::stoi(argv[4]);
        }

        FrameReader reader(name);


        uint fps = reader.getFps();


        uint64_t last_time = currentTimeMs();
        uint64_t start_time = last_time;
        uint64_t start_frame_time = last_time;
        uint64_t sent_frames = 0;
        uint64_t processing_time = 0;

        double sent_mbytes = 0;

        socket.connect("tcp://" + host + ":" + std::string(argv[2]));

        while (stopAfter > 0) {
            // maintain constant FPS by ignoring processing time
            uint64_t sleep_time = (1000 / fps) - processing_time;


            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
            start_frame_time = currentTimeMs();

            if (sent_frames == 0) {
                last_time = currentTimeMs();
                start_time = last_time;
            }

            FrameStruct f = reader.currentFrame();
            f.streamId = reader.getStreamId();

            std::vector<FrameStruct> v;
            v.push_back(f);

            std::string message = cerealStructToString(v);

            zmq::message_t request(message.size());
            memcpy(request.data(), message.c_str(), message.size());

            uint frameId = reader.currentFrameId();
            if (reader.hasNextFrame())
                reader.nextFrame();
            else {
                reader.reset();
                stopAfter--;
            }
            socket.send(request);
            sent_frames += 1;
            sent_mbytes += message.size()/1000.0;

            uint64_t diff_time = currentTimeMs() - last_time;

            double diff_start_time = (currentTimeMs() - start_time);
            int64_t avg_fps;
            if (diff_start_time == 0)
                avg_fps = -1;
            else {
                double avg_time_per_frame_sent_ms = diff_start_time / (double) sent_frames;
                avg_fps = 1000 / avg_time_per_frame_sent_ms;
            }

            last_time = currentTimeMs();
            processing_time = last_time - start_frame_time;

            std::cout << f.deviceId << ";" << f.sensorId << ";" << f.frameId << " sent, took " << diff_time << " ms; size " << message.size()
                      << "; avg " << avg_fps << " fps; " << 8*(sent_mbytes/diff_start_time) << " Mbps" << std::endl;



        }
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
