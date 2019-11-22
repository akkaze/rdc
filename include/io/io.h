#pragma once
#include <inttypes.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "core/logging.h"

/*! @brief namespace for rdc */
namespace rdc {
/*!
 * @brief interface of stream I/O for serialization
 */
class Stream {
public:
    /*!
     * @brief reads data from a stream
     * @param ptr pointer to a memory buffer
     * @param size block size
     * @return the size of data read
     */
    virtual size_t Read(void *ptr, size_t size) = 0;
    /*!
     * @brief writes data to a stream
     * @param ptr pointer to a memory buffer
     * @param size block size
     */
    virtual void Write(const void *ptr, size_t size) = 0;
    /*! @brief virtual destructor */
    virtual ~Stream(void) {}
    /*!
     * @brief generic factory function
     *  create an stream, the stream will close the underlying files upon
     * deletion
     *
     * @param uri the uri of the input currently we support
     *            hdfs://, s3://, and file:// by default file:// will be used
     * @param flag can be "w", "r", "a"
     * @param allow_null whether NULL can be returned, or directly report error
     * @return the created stream, can be NULL when allow_null == true and file
     * do not exist
     */
    static Stream *Create(const char *uri, const char *const flag,
                          bool allow_null = false);
    // helper functions to write/read different data structures
    /*!
     * @brief writes a data to stream
     * support Write/Read of most STL
     * composites and base types.
     * If the data type is not supported, a compile time error will
     * be issued.
     *
     * @param data data to be written
     * \tparam T the data type to be written
     */
    template <typename T>
    void Write(const T &data);
    /*!
     * @brief loads a data from stream.
     * support Write/Read of most STL
     * composites and base types.
     * If the data type is not supported, a compile time error will
     * be issued.
     *
     * @param out_data place holder of data to be deserialized
     * @return whether the load was successful
     */
    template <typename T>
    bool Read(T *out_data);
};

/*! @brief interface of i/o stream that support seek */
class SeekStream : public Stream {
public:
    // virtual destructor
    virtual ~SeekStream(void) {}
    /*! @brief seek to certain position of the file */
    virtual void Seek(size_t pos) = 0;
    /*! @brief tell the position of the stream */
    virtual size_t Tell(void) = 0;
    /*! @brief generic factory function
     *  create an SeekStream for read only,
     *  the stream will close the underlying files upon deletion
     *  error will be reported and the system will exit when create failed
     * @param uri the uri of the input currently we support
     *            hdfs://, s3://, and file:// by default file:// will be used
     * @param allow_null whether NULL can be returned, or directly report error
     * @return the created stream, can be NULL when allow_null == true and file
     * do not exist
     */
    static SeekStream *CreateForRead(const char *uri, bool allow_null = false);
};

/*! @brief interface for serializable objects */
class Serializable {
public:
    /*! @brief virtual destructor */
    virtual ~Serializable() {}
    /*!
     * @brief load the model from a stream
     * @param fi stream where to load the model from
     */
    virtual void Load(Stream *fi) = 0;
    /*!
     * @brief saves the model to a stream
     * @param fo stream where to save the model to
     */
    virtual void Save(Stream *fo) const = 0;
};

/*!
 * @brief input split creates that allows reading
 *  of records from split of data,
 *  independent part that covers all the dataset
 *
 *  see InputSplit::Create for definition of record
 */
class InputSplit {
public:
    /*! @brief a blob of memory region */
    struct Blob {
        /*! @brief points to start of the memory region */
        void *dptr;
        /*! @brief size of the memory region */
        size_t size;
    };
    /*!
     * @brief hint the inputsplit how large the chunk size
     *  it should return when implementing NextChunk
     *  this is a hint so may not be enforced,
     *  but InputSplit will try adjust its internal buffer
     *  size to the hinted value
     * @param chunk_size the chunk size
     */
    virtual void HintChunkSize(size_t chunk_size) {}
    /*! @brief get the total size of the InputSplit */
    virtual size_t GetTotalSize(void) = 0;
    /*! @brief reset the position of InputSplit to beginning */
    virtual void BeforeFirst(void) = 0;
    /*!
     * @brief get the next record, the returning value
     *   is valid until next call to NextRecord or NextChunk
     *   caller can modify the memory content of out_rec
     *
     *   For text, out_rec contains a single line
     *   For recordio, out_rec contains one record content(with header striped)
     *
     * @param out_rec used to store the result
     * @return true if we can successfully get next record
     *     false if we reached end of split
     * \sa InputSplit::Create for definition of record
     */
    virtual bool NextRecord(Blob *out_rec) = 0;
    /*!
     * @brief get a chunk of memory that can contain multiple records,
     *  the caller needs to parse the content of the resulting chunk,
     *  for text file, out_chunk can contain data of multiple lines
     *  for recordio, out_chunk can contain multiple records(including headers)
     *
     *  This function ensures there won't be partial record in the chunk
     *  caller can modify the memory content of out_chunk,
     *  the memory is valid until next call to NextRecord or NextChunk
     *
     *  Usually NextRecord is sufficient, NextChunk can be used by some
     *  multi-threaded parsers to parse the input content
     *
     * @param out_chunk used to store the result
     * @return true if we can successfully get next record
     *     false if we reached end of split
     * \sa InputSplit::Create for definition of record
     * \sa RecordIOChunkReader to parse recordio content from out_chunk
     */
    virtual bool NextChunk(Blob *out_chunk) = 0;
    /*! @brief destructor*/
    virtual ~InputSplit(void) {}
    /*!
     * @brief reset the Input split to a certain part id,
     *  The InputSplit will be pointed to the head of the new specified segment.
     *  This feature may not be supported by every implementation of InputSplit.
     * @param part_index The part id of the new input.
     * @param num_parts The total number of parts.
     */
    virtual void ResetPartition(unsigned part_index, unsigned num_parts) = 0;
    /*!
     * @brief factory function:
     *  create input split given a uri
     * @param uri the uri of the input, can contain hdfs prefix
     * @param part_index the part id of current input
     * @param num_parts total number of splits
     * @param type type of record
     * @return a new input split
     */
    static InputSplit *Create(const char *uri, unsigned part_index,
                              unsigned num_parts, const char *type);
};

}  // namespace rdc
#include "io/serializer.h"
namespace rdc {
// implementations of functions
template <typename T>
void Stream::Write(const T &data) {
    serializer::Handler<T>::Write(this, data);
}

template <typename T>
bool Stream::Read(T *out_data) {
    return serializer::Handler<T>::Read(this, out_data);
}

/*! @brief fixed size memory buffer */
}  // namespace rdc
