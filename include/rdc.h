/*!
 *  Copyright (c) 2018 by Contributors
 * \file rdc.h
 * @brief This file defines rdc's Allreduce/Broadcast interface
 *   The rdc engine contains the actual implementation
 *   Code that only uses this header can also be compiled with MPI Allreduce
 * (non fault-tolerant),
 *
 *   rdc.h and serializable.h is all what the user needs to use the rdc
 * interface \author Ankun Zheng
 */
#pragma once  // NOLINT(*)
#include <string>
#include <vector>

// optionally support of lambda functions in C++11, if available
#include <functional>
// comminicator definition of rdc, defines internal implementation
// to use rdc interface, there is no need to read engine.h
// rdc.h and serializable.h are enough to use the interface
#include "comm/communicator.h"
#include "core/work_request.h"
#include "transport/buffer.h"
#include "api.h"
/*! @brief rdc namespace */
namespace rdc {

/*!
 * @brief reduction operators namespace
 */
namespace op {
/*!
 * \class rdc::op::Max
 * @brief maximum reduction operator
 */
struct Max;
/*!
 * \class rdc::op::Min
 * @brief minimum reduction operator
 */
struct Min;
/*!
 * \class rdc::op::Sum
 * @brief sum reduction operator
 */
struct Sum;
/*!
 * \class rdc::op::BitOR
 * @brief bitwise OR reduction operator
 */
struct BitOR;
}  // namespace op
/*!
* @brief initializes rdc, call this once at the beginning of your program
* @param argc number of arguments in argv
* @param argv the array of input arguments
*/
void Init();
/*!
 * @brief create a new communicator with specific name
 * @param name the communicator name
 */
comm::ICommunicator* NewCommunicator(const std::string& name);
/*!
 * @brief get a existed communicator with specific name
 * @param name the communicator name
 */
comm::ICommunicator* GetCommunicator(const std::string& name);

/*!
 * @brief finalizes the rdc engine, call this function after you finished with
 * all the jobs
 */
void Finalize();
/*! @brief gets rank of the current process */
int GetRank();
/*! @brief gets total number of processes */
int GetWorldSize();
/*! @brief whether rdc env is in distributed mode */
bool IsDistributed();
}  // namespace rdc
// implementation of template functions
#include "core/rdc-inl.h"
