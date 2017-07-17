/**
 * writer.c
 */

#include "mpi.h"
#include "sst.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dummy.h"

int main(int argc, char **argv)
{
    int rank = 0, size = 0;
    MPI_Comm comm = MPI_COMM_WORLD;

    int retval;
    int err_count = 0;
    adios2_stream output;
    adios2_metadata meta;
    adios2_data data;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    output = SstWriterOpen("test", "", MPI_COMM_WORLD);

    /* test framework calls for creating dummy data */
    meta = CreateDummyMetadata(/* timestep */ 0, rank, size);
    data = CreateDummyData(/* timestep */ 0, rank, size);

    /* provide metadata and data for timestep 0 */
    SstProvideTimestep(output, meta, data, 0);

    /* (cleanly) shutdown this stream */
    //    SstWriterClose(output);
    sleep(20);

    MPI_Finalize();
}
