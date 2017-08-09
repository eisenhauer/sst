#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

#include "sst.h"
#include "dummy.h"

int main(int argc, char **argv)
{
    int rank, size;
    SstFullMetadata meta;
    void **completions;
    SstStream input;
    char **buffers;

    MPI_Comm comm = MPI_COMM_WORLD;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    input = SstReaderOpen("test", "", MPI_COMM_WORLD);

    meta = SstGetMetadata(input, /* timestep */ 0);

    completions = malloc(sizeof(completions[0]) * meta->WriterCohortSize);
    memset(completions, 0, sizeof(completions[0]) * meta->WriterCohortSize);
    buffers = malloc(sizeof(buffers[0]) * meta->WriterCohortSize);
    memset(buffers, 0, sizeof(buffers[0]) * meta->WriterCohortSize);

    for (int i = rank % 2; i < meta->WriterCohortSize; i += 2) {
        /* only filling in every other one */
        buffers[i] = malloc(meta->WriterMetadata[i]->DataSize);
        completions[i] = SstReadRemoteMemory(
            input, i /* rank */, 0, 3 /* offset */,
            meta->WriterMetadata[i]->DataSize - 3, buffers[i],
            meta->DP_TimestepInfo ? meta->DP_TimestepInfo[i] : NULL);
    }

    for (int i = 0; i < meta->WriterCohortSize; i++) {
        if (completions[i]) {
            SstWaitForCompletion(input, completions[i]);
            if (ValidateDummyData(0, i, meta->WriterCohortSize, 3,
                                  buffers[i]) != 0) {
                printf("Bad data from rank %d\n", i);
            }
        }
    }

    SstReleaseStep(input, 0);
    SstAdvanceStep(input, 0);
    SstReaderClose(input);

    MPI_Finalize();
    return 0;
}
