/*!
 *  Copyright (c) 2018 by Contributors
 * \file basic.cc
 * \brief This is an example demonstrating setup and finalize of rdc
 *
 * \author AnkunZheng
 */
#include <vector>
#include "rdc.h"
#include <chrono>
#include <thread>
using namespace rdc;
int main(int argc, char *argv[]) {
    rdc::Init(argc, argv);
    rdc::NewCommunicator(kMainCommName);
    //std::this_thread::sleep_for(std::chrono::seconds(4));
    rdc::Finalize();
    return 0;
}
