#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

#include "sst.h"
#include "dummy.h"

SstMetadata CreateDummyMetadata(long timestep, int rank, int size)
{
    SstMetadata meta = malloc(sizeof(struct _SstMetadata));
    struct _SstVarMeta *var;
    meta->DataSize = 10240;
    meta->VarCount = 1;
    var = (struct _SstVarMeta *)malloc(sizeof(struct _SstVarMeta));
    var->VarName = strdup("Array");
    var->DimensionCount = 1;
    var->Dimensions = malloc(sizeof(struct _SstDimenMeta));
    var->Dimensions->Offset = rank * 10;
    var->Dimensions->Size = 10;
    var->Dimensions->GlobalSize = 10 * size;
    meta->Vars = var;
    return meta;
}

SstData CreateDummyData(long timestep, int rank, int size)
{
    SstData data = malloc(sizeof(struct _SstData));
    data->DataSize = 10 * sizeof(double);
    data->block = malloc(data->DataSize);
    double *tmp = (double *)data->block;
    for (int i = 0; i < 10; i++) {
        tmp[0] = timestep * 1000.0 + rank * 10.0 + i;
    }
    return data;
}
