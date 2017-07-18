struct _sst_stream;
typedef struct _sst_stream *adios2_stream;

struct _sst_metadata;
typedef struct _sst_metadata *adios2_metadata;

struct _sst_full_metadata;
typedef struct _sst_full_metadata *adios2_full_metadata;

struct _sst_data;
typedef struct _sst_data *adios2_data;

extern adios2_stream SstWriterOpen(char *filename, char *params, MPI_Comm comm);

extern adios2_stream SstReaderOpen(char *filename, char *params, MPI_Comm comm);

extern adios2_full_metadata SstGetMetadata(adios2_stream stream, long timestep);

extern void SstProvideTimestep(adios2_stream s, adios2_metadata local_metadata,
                               adios2_data data, long timestep);

extern void *SstReadRemoteMemory(adios2_stream s, int rank, long timestep,
                                 size_t offset, size_t length, void
                                 *buffer);

extern void SstWaitForCompletion(adios2_stream stream, void *completion);

extern void SstReleaseTimestep(adios2_stream stream, long timestep);

struct _sst_full_metadata {
    int writer_cohort_size;
    adios2_metadata *writer;
};

struct _sst_metadata {
    size_t data_size;
    int var_count;
    struct _sst_var_meta *vars;
};

struct _sst_data {
    size_t data_size;
    char *block;
};

struct _sst_var_meta {
    char *var_name;
    int dimension_count;
    struct _sst_dimen_meta *dimensions;
    int data_offset_in_block;
};

struct _sst_dimen_meta {
    int offset;
    int size;
    int global_size;
};

/* Data plane called by control plane: */
/* DpProvideTimestep(adios2_stream s, adios2_data data, uint64_t timestep); */
/* DpReleaseTimestep(adios2_stream s, uint64_t timestep); */

/* Reader side APIs */
/* Control plane called by ADIOS2 SST engine: */
/* SstInqVar(), SstScheduleRead(), etc.   operate on global metadata */
/* adios2_stream SstReaderOpen(char *filename, char *params); */
/* SstPerformReads(adios2_stream s, uint64_t timestep); */
/* SstReleaseStep(adios2_stream s, uint64_t timestep); */
/* SstAdvanceStep(adios2_stream s, uint64_t timestep); */

/* Data plane called by control plane: */
/* data_completion_handle DpRemoteMemoryRead(adios2_stream s, int rank, uint63_t
 * timestep, */
/*                                                                                                uint64_t
 * offset, uint64_t length, void *buffer); */
/* DpWaitForCompletion(data_completion_handle handle); */
