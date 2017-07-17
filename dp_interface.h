#ifndef _DP_INTERFACE_H
#define _DP_INTERFACE_H

/*!
 *
 * SST_DP_Interface is the set of data format descriptions and function
 * pointers that define a dataplane interface to control plane.
 *
 */
typedef struct _SST_DP_Interface *SST_DP_Interface;

typedef SST_DP_Interface (*SST_DP_InitFunc)();

/*!
 * DP_RS_stream is an externally opaque pointer-sized value that represents
 * the Reader Side DP stream.  It is returned by an init function and
 * provided back to the dataplane on every subsequent reader side call.
 */
typedef void *DP_RS_stream;

/*!
 * DP_WS_stream is an externally opaque pointer-sized value that represents
 * the Writer Side DP stream.  Because a stream might have multiple readers,
 * this value is only provided to the per-reader writer-side initialization
 * function, which returns its own opaque stream ID value.
 */
typedef void *DP_WS_stream;

/*!
 * DP_WSR_stream is an externally opaque pointer-sized value that represents
 * the Writer Side *per Reader* DP stream.  This value is returned by the
 * per-reader writer-side initialization and provided back to the dataplane
 * on any later reader-specific operations.
 */
typedef void *DP_WSR_stream;

/*!
 * SST_DP_InitReaderFunc is the type of a dataplane reader-side stream
 * initialization function.  Its return value is DP_RS_stream, an externally
 * opaque handle which is provided to the dataplane on all subsequent
 * operations for this stream.  'stream' is an input parameter and is the
 * control plane-level reader-side stream identifier.  This may be useful
 * for callbacks, access to MPI communicator, EVPath info, etc. so can be
 * associated with the DP_RS_stream.  'initReaderInfo' is a pointer to a
 * void*.  That void* should be filled in by the init function with a
 * pointer to reader-specific contact information for this process.  The
 * `readerContactFormats` FMStructDescList should describe the datastructure
 * pointed to by the void*.  The control plane will gather this information
 * for all reader ranks, transmit it to the writer cohort and provide it as
 * an array of pointers in the `providedReaderInfo` argument to
 * SST_DP_WriterPerReaderInitFunc.
 */
typedef DP_RS_stream (*SST_DP_InitReaderFunc)(void *stream,
                                              void **initReaderInfo);

/*!
 * SST_DP_InitWriterFunc is the type of a dataplane writer-side stream
 * initialization function.  Its return value is DP_WS_stream, an externally
 * opaque handle which is provided to the dataplane on all subsequent
 * stream-wide operations for this stream.  'stream' is an input parameter and
 * is the
 * control plane-level writer-side stream identifier.  This may be useful
 * for callbacks, access to MPI communicator, EVPath info, etc. so can be
 * associated with the DP_RS_stream.
 */
typedef DP_WS_stream (*SST_DP_InitWriterFunc)(void *stream);

/*!
 * SST_DP_WriterPerReaderInitFunc is the type of a dataplane writer-side
 * per-reader stream initialization function.  It is called when a new
 * reader joins an writer-side stream.  Its return value is DP_WSR_stream,
 * an externally opaque handle which is provided to the dataplane on all
 * operations on this stream that are specific to this reader.  operations
 * for this stream.  'stream' is an input parameter and is the DP_WS_stream
 * value that was returned when this stream was initialized via the
 * SST_DP_InitWriterFunc.  `readerCohortSize` is the size of the reader's
 * MPI cohort.  `providedReaderInfo` is a pointer to an array of void*
 * pointers with array size `readerCohortSize`.  The Nth element of the
 * array is a pointer to the value returned in initReaderInfo by reader rank
 * N (with type described by readerContactFormats).  'initWriterInfo' is a
 * pointer to a void*.  That void* should be filled in by the init function
 * with a pointer to writer-specific contact information for this process.
 * The `writerContactFormats` FMStructDescList should describe the
 * datastructure pointed to by the void*.  The control plane will gather
 * this information for all writer ranks, transmit it to the reader cohort
 * and provide it as an array of pointers in the `providedWriterInfo`
 * argument to WHAT?
 */
typedef DP_WSR_stream (*SST_DP_WriterPerReaderInitFunc)(
    DP_WS_stream stream, int readerCohortSize, void **providedReaderInfo,
    void **initWriterInfo);

typedef struct _SST_DP_Interface {
    FMStructDescList readerContactFormats;
    FMStructDescList writerContactFormats;

    SST_DP_InitReaderFunc InitReader;
    SST_DP_InitWriterFunc InitWriter;
    SST_DP_WriterPerReaderInitFunc WriterPerReaderInit;

} * SST_DP_Interface;

typedef void (*DpProvideTimestep)(DP_WS_stream stream, void *data,
                                  long timestep);

void DpReleaseTimestep(void *s, long timestep);

/* SstPerformReads(adios2_stream s, uint64_t timestep); */
/* SstReleaseStep(adios2_stream s, uint64_t timestep); */
/* SstAdvanceStep(adios2_stream s, uint64_t timestep); */

struct _dp_completion_handle;
typedef struct _dp_completion_handle *data_completion_handle;

// data_completion_handle DpRemoteMemoryRead(adios2_stream s, int rank, long
// timestep,
//                                          size_t offset, size_t length, void
//                                          *buffer);

void DpWaitForCompletion(data_completion_handle handle);

void *DpInitReader(void *s, void **init_exchange_info,
                   FMStructDescList *init_exchange_desc);

#endif
