#pragma once
namespace rdc {
/*!
 * @brief prints the msg to the tracker,
 *    this function can be used to communicate progress information to
 *    the user who monitors the tracker
 * @param msg the message to be printed
 */
void TrackerPrint(const std::string& msg);
void Send(void* send_data, size_t size, int dest);
void Recv(void* recv_data, size_t size, int src);
void Send(const Buffer& send_buf, size_t size, int dest);
void Recv(Buffer& recv_buf, size_t size, int src);
void Barrier();
/*!
 * @brief broadcasts a memory region to every node from the root
 *
 *     Example: int a = 1; Broadcast(&a, sizeof(a), root);
 * @param sendrecv_data the pointer to the send/receive buffer,
 * @param size the data size
 * @param root the process root
 */
void Broadcast(void* sendrecv_data, size_t size, int root,
               const std::string& comm_name = kMainCommName);
void Broadcast(Buffer& buf, int root,
               const std::string& comm_name = kMainCommName);
/*!
 * @brief broadcasts an std::vector<DType> to every node from root
 * @param sendrecv_data the send/receive vector,
 *        for the receiver, the vector does not need to be pre-allocated
 * @param root the process root
 * @tparam DType the data type stored in the vector, has to be a simple data
 * type that can be directly transmitted by sending the sizeof(DType)
 */
template <typename DType>
void Broadcast(std::vector<DType>& sendrecv_data, int root,
               const std::string& comm_name = kMainCommName);
/*!
 * @brief broadcasts a std::string to every node from the root
 * @param sendrecv_data the the send/receive buffer,
 *        for the receiver, the vector does not need to be pre-allocated
 * @param root the process root
 */
void Broadcast(std::string& sendrecv_data, int root,
               const std::string& comm_name = kMainCommName);

template <typename DType>
void Allgather(std::vector<std::vector<DType>>& sendrecv_data,
               const std::string& comm_name = kMainCommName);

void Allgather(void** sendrecv_data, size_t type_nbyes, size_t* counts,
               const std::string& comm_name = kMainCommName);

/*!
 * @brief performs in-place Allreduce, on sendrecvbuf
 *        with a prepare function specified by a lambda function
 * @param sendrecvbuf buffer for both sending and receiving data
 * @param count number of elements to be reduced
 * @tparam OP see namespace op, reduce operator
 * @tparam DType data type
 */
template <typename OP, typename DType>
void Allreduce(DType* sendrecvbuf, uint64_t count,
               const std::string& comm_name = kMainCommName);
template <typename OP>
void Allreduce(Buffer& sendrecvbuf);
/*!
 * @brief loads the latest check point
 * @param global_model pointer to the globally shared model/state
 *   when calling this function, the caller needs to guarantee that the
 * global_model is the same in every node @param local_model pointer to the
 * local model that is specific to the current node/rank this can be NULL when
 * no local model is needed
 *
 * @return the version number of the check point loaded
 *     if returned version == 0, this means no model has been CheckPointed
 *     the p_model is not touched, users should do the necessary initialization
 * by themselves
 *
 * \sa CheckPoint, VersionNumber
 */
int LoadCheckPoint(Serializable* global_model,
                   Serializable* local_model = NULL);
/*!
 * @brief checkpoints the model, meaning a stage of execution has finished.
 *  every time we call check point, a version number will be increased by one
 *
 * @param global_model pointer to the globally shared model/state
 *   when calling this function, the caller needs to guarantee that the
 * global_model is the same in every node @param local_model pointer to the
 * local model that is specific to the current node/rank this can be NULL when
 * no local state is needed NOTE: local_model requires explicit replication of
 * the model for fault-tolerance, which will bring replication cost in the
 * CheckPoint function. global_model does not need explicit replication. So,
 * only CheckPoint with the global_model if possible \sa LoadCheckPoint,
 * VersionNumber
 */
void CheckPoint(const Serializable* global_model,
                const Serializable* local_model = NULL);
/*!
 * @brief This function can be used to replace CheckPoint for global_model only,
 *   when certain condition is met (see detailed explanation).
 *
 *   This is a "lazy" checkpoint such that only the pointer to the global_model
 * is remembered and no memory copy is taken. To use this function, the user
 * MUST ensure that: The global_model must remain unchanged until the last call
 * of Allreduce/Broadcast in the current version finishes. In other words, the
 * global_model model can be changed only between the last call of
 *   Allreduce/Broadcast and LazyCheckPoint, both in the same version
 *
 * @param global_model pointer to the globally shared model/state
 *   when calling this function, the caller needs to guarantee that the
 * global_model is the same in every node \sa LoadCheckPoint, CheckPoint,
 * VersionNumber
 */
void LazyCheckPoint(const Serializable* global_model);
/*!
 * @return version number of the current stored model,
 *         which means how many calls to CheckPoint we made so far
 * \sa LoadCheckPoint, CheckPoint
 */
int VersionNumber();

std::unique_ptr<comm::ICommunicator> CreateGroup(
    const std::vector<int>& ranks, const std::string& group_name = "");
// ----- extensions that allow customized reducer ------
/*!
 * @brief template class to make customized reduce and all reduce easy
 *  Do not use reducer directly in the function you call Finalize,
 *   because the destructor can execute after Finalize
 * @tparam DType data type that to be reduced
 * @tparam freduce the customized reduction function
 *  DType must be a struct, with no pointer
 */
template <typename DType,
          void (*freduce)(DType& dst, const DType& src)>  // NOLINT(*)
class Reducer {
public:
    Reducer();
    /*!
     * @brief customized in-place all reduce operation
     * @param sendrecvbuf the in place send-recv buffer
     * @param count number of elements to be reduced
     */
    void Allreduce(DType* sendrecvbuf, size_t count);
};
/*!
 * @brief template class to make customized reduce,
 *  this class defines complex reducer handles all the data structure that can
 * be serialized/deserialized into fixed size buffer Do not use reducer directly
 * in the function you call Finalize, because the destructor can execute after
 * Finalize
 *
 * @tparam DType data type that to be reduced, DType must contain the following
 * functions: @tparam freduce the customized reduction function (1) Save(IStream
 * &fs)  (2) Load(IStream &fs) (3) Reduce(const DType &src, size_t max_nbyte)
 */
template <typename DType>
class SerializeReducer {
public:
    SerializeReducer();
    /*!
     * @brief customized in-place all reduce operation
     * @param sendrecvobj pointer to the array of objects to be reduced
     * @param max_nbyte maximum amount of memory needed to serialize each object
     *        this includes budget limit for intermediate and final result
     * @param count number of elements to be reduced
     */
    void Allreduce(DType* sendrecvobj, size_t max_nbyte, size_t count);

private:
    /*! @brief temporal buffer used to do reduce*/
    std::string buffer_;
};
}  // namespace rdc
