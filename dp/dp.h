// void
// DpProvideTimestep(void * s, adios2_data data, long timestep);

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
