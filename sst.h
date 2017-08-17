/*
 *  The SST external interfaces.
 *
 *  This is more a rough sketch than a final version.  The details will
 *  change when the integration with ADIOS2 layers happen.  In the meantime,
 *  this interface (hopefully) captures enough of the functionality for
 *  control plane and data plane implementations to proceed while the
 *  integration details are hashed out.
 */
#ifndef SST_H_
#define SST_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * SstStream is the basic type of a stream connecting an ADIOS2 reader
 * and an ADIOS2 writer.  Externally the same data type is used for both.
 */
typedef struct _SstStream *SstStream;

/*
 *  metadata and typedefs are tentative and may come from ADIOS2 constructors.
*/
typedef struct _SstMetadata *SstMetadata;
typedef struct _SstFullMetadata *SstFullMetadata;
typedef struct _SstData *SstData;

typedef enum { SstSuccess, SstEndOfStream, SstFatalError } SstStatusValue;

/*
 *  Writer-side operations
 */
extern SstStream SstWriterOpen(char *filename, char *params, MPI_Comm comm);
extern void SstProvideTimestep(SstStream s, SstMetadata local_metadata,
                               SstData data, long timestep);
extern void SstWriterClose(SstStream stream);

/*
 *  Reader-side operations
 */
extern SstStream SstReaderOpen(char *filename, char *params, MPI_Comm comm);
extern SstFullMetadata SstGetMetadata(SstStream stream, long timestep);
extern void *SstReadRemoteMemory(SstStream s, int rank, long timestep,
                                 size_t offset, size_t length, void *buffer,
                                 void *DP_TimestepInfo);
extern SstStatusValue SstWaitForCompletion(SstStream stream, void *completion);
extern void SstReleaseStep(SstStream stream, long timestep);
extern SstStatusValue SstAdvanceStep(SstStream stream, long timestep);
extern void SstReaderClose(SstStream stream);

#include "sst_data.h"

#ifdef __cplusplus
}
#endif

#endif /* SST_H_*/
