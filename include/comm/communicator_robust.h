/*!
 *  Copyright (c) 2018 by Contributors
 * \file allreduce_base.h
 * @brief Basic implementation of communication primitives
 *   include non-block send recv, allreduce, broadcast and allgather
 *   using TCP non-block socket or RDMA for communication.
 *
 *   This implementation provides robust version of Communication Primitives
 *   by considering node failure
 *
 * \author Ankun Zheng
 */
#include "comm/checkpointer.h"
#include "comm/communicator_base.h"
namespace rdc {
namespace comm {
class CommunicatorRobust : public Communicator {
public:
    /*!
     * @brief checkpoint the model, meaning we finished a stage of execution
     *  every time we call check point, there is a version number which will
     * increase by one
     *
     * @param global_model pointer to the globally shared model/state
     *   when calling this function, the caller need to gauranttees that
     * global_model is the same in all nodes @param local_model pointer to local
     * model, that is specific to current node/rank this can be NULL when no
     * local state is needed
     */
    void CheckPoint(const Serializable* global_model,
                    const Serializable* local_model = NULL) {
        version_number_ += 1;
    }
    /*!
     * @brief load latest check point
     * @param global_model pointer to the globally shared model/state
     *   when calling this function, the caller need to gauranttees that
     * global_model is the same in all nodes @param local_model pointer to local
     * model, that is specific to current node/rank this can be NULL when no
     * local model is needed
     *
     * @return the version number of check point loaded
     *
     * @sa CheckPoint, VersionNumber
     */
    int LoadCheckPoint(Serializable* global_model,
                       Serializable* local_model = nullptr) {
        return 0;
    }

    /*!
     * @brief This function can be used to replace CheckPoint for global_model
     * only, when certain condition is met(see detailed expplaination).
     *
     *
     * @param global_model pointer to the globally shared model/state
     *   when calling this function, the caller need to gauranttees that
     * global_model is the same in all nodes \sa LoadCheckPoint, CheckPoint,
     * VersionNumber
     */
    void LazyCheckPoint(const Serializable* global_model) {
        version_number_ += 1;
    }
    /*!
     * @return version number of current stored model,
     *         which means how many calls to CheckPoint we made so far
     * @sa LoadCheckPoint, CheckPoint
     */
    int VersionNumber() {
        return version_number_;
    }

private:
    // call sequence counter, records how many calls we made so far
    // from last call to CheckPoint, LoadCheckPoint
    int seq_counter;
    int version_number_;
};
}  // namespace comm
}  // namespace rdc
