#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

#include "sst.h"
#include "dummy.h"

#define DATA_SIZE 10240

SstMetadata CreateDummyMetadata(long timestep, int rank, int size)
{
    SstMetadata meta = malloc(sizeof(struct _SstMetadata));
    struct _SstVarMeta *var;
    meta->DataSize = DATA_SIZE;
    meta->VarCount = 1;
    var = (struct _SstVarMeta *)malloc(sizeof(struct _SstVarMeta));
    var->VarName = strdup("Array");
    var->DimensionCount = 1;
    var->Dimensions = malloc(sizeof(struct _SstDimenMeta));
    var->Dimensions->Offset = rank * 10240;
    var->Dimensions->Size = 10240;
    var->Dimensions->GlobalSize = 10240 * size;
    meta->Vars = var;
    return meta;
}

SstData CreateDummyData(long timestep, int rank, int size)
{
    SstData data = malloc(sizeof(struct _SstData));
    data->DataSize = DATA_SIZE;
    data->block = malloc(DATA_SIZE);
    double *tmp = (double *)data->block;
    for (int i = 0; i < DATA_SIZE / 8; i++) {
        tmp[i] = timestep * 1000.0 + rank * 10.0 + i;
    }
    return data;
}

extern int ValidateDummyData(long timestep, int rank, int size, int offset,
                             void *buffer)
{
    int start = (offset + 7) / 8;
    int remainder = offset % 8;
    double first_double = timestep * 1000.0 + rank * 10.0 + (start - 1);
    double *tmp = (double *)((char *)buffer + 8 - remainder);
    int result = 0;

    if (remainder != 0) {
        result =
            memcmp(buffer, ((char *)&first_double) + remainder, 8 - remainder);
    } else {
        tmp = buffer;
    }
    for (int i = start; i < DATA_SIZE / 8; i++) {
        result |= !(*tmp == timestep * 1000.0 + rank * 10.0 + i);
        tmp++;
    }
    return result;
}
