/*!
 *  Copyright (c) 2018 by Contributors
 * \file comm.cc
 * \brief this file governs which implementation of comm we are actually using
 *  provides an singleton of comm interface
 *
 * \author Ankun Zheng
 */
#include "comm/communicator.h"
#include <memory>
#include "comm/communicator_base.h"
#include "comm/communicator_manager.h"
#include "common/thread_local.h"
namespace rdc {
namespace comm {
// singleton sync manager
#ifndef RDC_USE_BASE
typedef CommunicatorRobust Comm;
#else
typedef Communicator Comm;
#endif

// perform in-place allreduce, on sendrecvbuf
void Allreduce_(Buffer sendrecvbuf, ReduceFunction red, mpi::DataType dtype,
                mpi::OpType op, const std::string& name) {
    CommunicatorManager::Get()->GetCommunicator(name)->Allreduce(sendrecvbuf,
                                                                 red);
}

}  // namespace comm
}  // namespace rdc
