
/* necessary EVPath include so that data format descriptions can be present in
 * interfaces */
#include <evpath.h>
#include <mpi.h>

#include "cp/cp.h"

#include "dp_interface.h"

SST_DP_Interface LoadDP(char *dp_name);

/* struct _sst_stream; */
/* typedef struct _sst_stream *adios2_stream; */

/* struct _sst_metadata; */
/* typedef struct _sst_metadata *adios2_metadata; */

/* struct _sst_data; */
/* typedef struct _sst_data *adios2_data; */

/* extern */
/* adios2_stream SstWriterOpen(char *filename, char *params, MPI_Comm comm); */

/* extern void */
/* SstProvideTimestep(adios2_stream s, adios2_metadata local_metadata,
 * adios2_data data, long timestep); */
/* /\* Data plane called by control plane: *\/ */
/* /\* DpProvideTimestep(adios2_stream s, adios2_data data, uint64_t timestep);
 * *\/ */
/* /\* DpReleaseTimestep(adios2_stream s, uint64_t timestep); *\/ */

/* /\* Reader side APIs *\/ */
/* /\* Control plane called by ADIOS2 SST engine: *\/ */
/* /\* SstInqVar(), SstScheduleRead(), etc.   operate on global metadata *\/ */
/* /\* adios2_stream SstReaderOpen(char *filename, char *params); *\/ */
/* /\* SstPerformReads(adios2_stream s, uint64_t timestep); *\/ */
/* /\* SstReleaseStep(adios2_stream s, uint64_t timestep); *\/ */
/* /\* SstAdvanceStep(adios2_stream s, uint64_t timestep); *\/ */

/* /\* Data plane called by control plane: *\/ */
/* /\* data_completion_handle DpRemoteMemoryRead(adios2_stream s, int rank,
 * uint63_t timestep, *\/ */
/* /\* uint64_t offset, uint64_t length, void *buffer); *\/ */
/* /\* DpWaitForCompletion(data_completion_handle handle); *\/ */
/* /\* Writer side APIs *\/ */
/* /\* Control plane called by ADIOS2 SST engine: *\/ */
/* /\* adios2_stream SstWriterOpen(char *filename, char *params); *\/ */
/* /\* SstProvideTimestep(adios2_stream s, adios2_metadata local_metadata,
 * adios2_data data, uint64_t timestep); *\/ */
/* /\* Data plane called by control plane: *\/ */
/* /\* DpProvideTimestep(adios2_stream s, adios2_data data, uint64_t timestep);
 * *\/ */
/* /\* DpReleaseTimestep(adios2_stream s, uint64_t timestep); *\/ */

/* /\* Reader side APIs *\/ */
/* /\* Control plane called by ADIOS2 SST engine: *\/ */
/* /\* SstInqVar(), SstScheduleRead(), etc.   operate on global metadata *\/ */
/* /\* adios2_stream SstReaderOpen(char *filename, char *params); *\/ */
/* /\* SstPerformReads(adios2_stream s, uint64_t timestep); *\/ */
/* /\* SstReleaseStep(adios2_stream s, uint64_t timestep); *\/ */
/* /\* SstAdvanceStep(adios2_stream s, uint64_t timestep); *\/ */

/* /\* Data plane called by control plane: *\/ */
/* /\* data_completion_handle DpRemoteMemoryRead(adios2_stream s, int rank,
 * uint63_t timestep, *\/ */
/* /\* uint64_t offset, uint64_t length, void *buffer); *\/ */
/* /\* DpWaitForCompletion(data_completion_handle handle); *\/ */
