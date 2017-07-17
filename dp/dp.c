#include <stdio.h>
#include <string.h>

#include <fm.h>

#include "dp_interface.h"

extern SST_DP_Interface LoadDummyDP();

SST_DP_Interface LoadDP(char *dp_name)
{
    if (strcmp(dp_name, "dummy") == 0) {
        return LoadDummyDP();
    } else {
        fprintf(stderr, "Unknown DP interface %s, load failed\n", dp_name);
        return NULL;
    }
}
