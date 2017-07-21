#ifndef _DP_INTERFACE_H
#define _DP_INTERFACE_H

/*!
 *
 * SST_DP_Interface is the set of data format descriptions and function
 * pointers that define a dataplane interface to control plane.
 *
 */
typedef struct _SST_DP_Interface *SST_DP_Interface;

typedef SST_DP_Interface (*SST_DP_LoadFunc)();

/*!
 * SST_services is the type of a pointer to a struct of function pointers
 * that give data plane access to control plane routines and functions.
 * Generally it is the first argument to all DP functions invoked by the
 * control plane.
 */
typedef struct _SST_services *SST_services;

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
 * SST_peerCohort is a value provided to the data plane that acts as a
 * handle to the opposite (reader or writer) cohort.  It is used in the
 * sst_send_to_peer service and helps the dataplane leverage existing
 * control plane messaging capabilities.
 */
typedef void *SST_peerCohort;

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
typedef DP_RS_stream (*SST_DP_InitReaderFunc)(SST_services svcs, void *CP_stream,
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
typedef DP_WS_stream (*SST_DP_InitWriterFunc)(SST_services svcs, void *CP_stream);

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
 * argument to TBD?  The `peerCohort` argument is a handle to the
 * reader-side peer cohort for use in peer-to-peer messaging.
 */
typedef DP_WSR_stream (*SST_DP_WriterPerReaderInitFunc)(
    SST_services svcs, DP_WS_stream stream, int readerCohortSize, SST_peerCohort peerCohort, void **providedReaderInfo,
    void **initWriterInfo);

/*
 * SST_DP_ReaderProvideWriterDataFunc is the type of a dataplane reader-side
 * function that provides information about the newly-connected writer-side
 * stream.  The `stream` parameter was that which was returned by a call to
 * the SST_DP_InitReaderFunc.  `writerCohortSize` is the size of the
 * writer's MPI cohort.  `providedWriterInfo` is a pointer to an array of
 * void* pointers with array size `writerCohortSize`.  The Nth element of
 * the array is a pointer to the value returned in initWriterInfo by writer
 * rank N (with type described by writerContactFormats).  `peerCohort`
 * argument is a handle to writer-side peer cohort for use in peer-to-peer
 * messaging.
 */
typedef void (*SST_DP_ReaderProvideWriterDataFunc)(
    SST_services svcs, DP_RS_stream stream, int writerCohortSize, SST_peerCohort peerCohort, void **providedWriterInfo);


/*
 *  DP_completionHandle an externally opaque pointer-sized value that is
 *  returned by the asynchronous DpReadRemoteMemory() call and which can be
 *  used to wait for the compelteion of the read.
 */
typedef void *DP_completionHandle;

/*!
 * SST_DP_ReadRemoteMemoryFunc is the type of a dataplane function that reads
 * into a local buffer the data contained in the data block associated with
 * a specific writer `rank` and a specific `timestep`.  The data should be
 * placed in the area pointed to by `buffer`.  The returned data should
 * start at offset `offset` from the beginning of the writers data block and
 * continue fur `length` bytes.
 */
typedef DP_completionHandle (*SST_DP_ReadRemoteMemoryFunc)(SST_services svcs, DP_RS_stream s, int rank, long timestep,
                                                       size_t offset, size_t length, void
                                                       *buffer);

/*!
 * SST_DP_WaitForCompletionFunc is the type of a dataplane function that
 * suspends the execution of the current thread until the asynchronous
 * SST_DP_ReadRemoteMemory call that returned its `handle` parameter.
 */
typedef void (*SST_DP_WaitForCompletionFunc)(SST_services svcs, DP_completionHandle handle);

/*!
 * SST_DP_ProvideTimestepFunc is the type of a dataplane function that
 * delivers a block of data associated with timestep `timestep` to the
 * dataplane, where it should be available for remote read requests until it
 * is released with SST_DP_ReleaseTimestep.
 */
typedef void (*SST_DP_ProvideTimestepFunc)(SST_services svcs, DP_WS_stream stream, void *data,
                                       long timestep);

/*!
 * SST_DP_ReleaseTimestepFunc is the type of a dataplane function that
 * informs the dataplane that the data associated with timestep `timestep`
 * will no longer be the subject of remote read requests, so its resources
 * may be released.
 */
typedef void (*SST_DP_ReleaseTimestepFunc)(SST_services svcs, DP_WS_stream stream, long timestep);

struct _SST_DP_Interface {
    FMStructDescList readerContactFormats;
    FMStructDescList writerContactFormats;

    SST_DP_InitReaderFunc InitReader;
    SST_DP_InitWriterFunc InitWriter;
    SST_DP_WriterPerReaderInitFunc WriterPerReaderInit;
    SST_DP_ReaderProvideWriterDataFunc ReaderProvideWriterData;

    SST_DP_ReadRemoteMemoryFunc ReadRemoteMemory;
    SST_DP_WaitForCompletionFunc WaitForCompletion;

    SST_DP_ProvideTimestepFunc ProvideTimestep;
    SST_DP_ReleaseTimestepFunc ReleaseTimestep;
};

/* SstPerformReads(adios2_stream s, uint64_t timestep); */
/* SstReleaseStep(adios2_stream s, uint64_t timestep); */
/* SstAdvanceStep(adios2_stream s, uint64_t timestep); */

typedef void (*SST_verboseFunc)(void *CP_stream, char *format, ...);
typedef CManager (*SST_getCManagerFunc)(void *CP_stream);
typedef int (*SST_myRankFunc)(void *CP_stream);
typedef int (*SST_sendToPeerFunc)(void* CP_stream, SST_peerCohort peerCohort, int rank, CMFormat format, void *data);
struct _SST_services {
    SST_verboseFunc verbose;
    SST_getCManagerFunc getCManager;
    SST_sendToPeerFunc sendToPeer;
    SST_myRankFunc myRank;
};
#endif
