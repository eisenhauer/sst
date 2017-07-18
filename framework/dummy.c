#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sst.h"
#include "dummy.h"

adios2_metadata CreateDummyMetadata(long timestep, int rank, int size)
{
    adios2_metadata meta = malloc(sizeof(struct _sst_metadata));
    struct _sst_var_meta *var;
    meta->data_size = 10240;
    meta->var_count = 1;
    var = (struct _sst_var_meta *)malloc(sizeof(struct _sst_var_meta));
    var->var_name = strdup("Array");
    var->dimension_count = 1;
    var->dimensions = malloc(sizeof(struct _sst_dimen_meta));
    var->dimensions->offset = rank * 10;
    var->dimensions->size = 10;
    var->dimensions->global_size = 10 * size;
    meta->vars = var;
    return meta;
}

adios2_data CreateDummyData(long timestep, int rank, int size)
{
    adios2_data data = malloc(sizeof(struct _sst_data));
    data->data_size = 10 * sizeof(double);
    data->block = malloc(data->data_size);
    double *tmp = (double *)data->block;
    for (int i = 0; i < 10; i++) {
        tmp[0] = timestep * 1000.0 + rank * 10.0 + i;
    }
    return data;
}
