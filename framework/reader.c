#include "mpi.h"
#include "sst.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int rank, size;
    adios2_full_metadata meta;
    void **completions;
    adios2_stream input;
    char ** buffers;

    MPI_Comm comm = MPI_COMM_WORLD;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    input = SstReaderOpen("test", "", MPI_COMM_WORLD);

    meta = SstGetMetadata(input, /* timestep */ 0); 

    printf("Reader rank %d got metadata %p\n", rank, meta);

    completions = malloc(sizeof(completions[0]) * meta->writer_cohort_size);
    memset(completions, 0, sizeof(completions[0]) * meta->writer_cohort_size);
    buffers = malloc(sizeof(buffers[0]) * meta->writer_cohort_size);
    memset(buffers, 0, sizeof(buffers[0]) * meta->writer_cohort_size);

    for (int i= rank%2; i < meta->writer_cohort_size; i+=2) {
        /* only filling in every other one */
        buffers[i] = malloc(meta->writer[i]->data_size);
        completions[i] = SstReadRemoteMemory(input, i /* rank */, 0,
                                             0 /* offset */, meta->writer[i]->data_size, buffers[i]);
    }

    for (int i=0; i < meta->writer_cohort_size; i++) {
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
    /* SstReleaseStep(input, 0); */
    /* SstReaderClose(input); */
}
