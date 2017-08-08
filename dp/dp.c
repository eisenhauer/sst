#include <stdio.h>
#include <string.h>

#include <evpath.h>
#include <fm.h>
#include <mpi.h>

#include "sst_data.h"
#include "dp_interface.h"

extern CP_DP_Interface LoadDummyDP();

CP_DP_Interface LoadDP(char *dp_name)
{
    if (strcmp(dp_name, "dummy") == 0) {
        return LoadDummyDP();
    } else {
        fprintf(stderr, "Unknown DP interface %s, load failed\n", dp_name);
        return NULL;
    }
}
