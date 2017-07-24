#include "mpi.h"

#include "sst.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        buffers[i] = malloc(meta->writer[i]->DataSize);
        completions[i] =
            SstReadRemoteMemory(input, i /* rank */, 0, 0 /* offset */,
                                meta->writer[i]->DataSize, buffers[i]);
    }

    for (int i = 0; i < meta->WriterCohortSize; i++) {
        if (completions[i]) {
            SstWaitForCompletion(input, completions[i]);
        }
    }

    /* for (i=0; i < meta->writer_size; i++) { */
    /*     if (completions[i]) { */
    /*         DpWaitForCompletion(completions[i]); */
    /*         result |= ValidateDummyData(0, i, buffer[i]); */
    /*     } */
    /* } */

    SstReleaseStep(input, 0);
    SstAdvanceStep(input, 0);
    SstReaderClose(input);

    /*
     * here temporarily until we have a clean shutdown protocol implemented
     */
    sleep(20);

    MPI_Finalize();
    return 0;
}
