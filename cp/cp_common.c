#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <atl.h>
#include <evpath.h>
#include <mpi.h>

#include "sst.h"
#include "cp_internal.h"

void CP_parse_params(adios2_stream stream, char *params)
{
    stream->wait_for_first_reader = 1;
}

static FMField cp_reader_init_list[] = {
    {"contact_info", "string", sizeof(char *),
     FMOffset(cp_reader_init_info, contact_info)},
    {"target_stone", "integer", sizeof(int),
     FMOffset(cp_reader_init_info, target_stone)},
    {"reader_ID", "integer", sizeof(void *),
     FMOffset(cp_reader_init_info, reader_ID)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec cp_reader_init_structs[] = {
    {"cp_reader", cp_reader_init_list, sizeof(struct _cp_reader_init_info),
     NULL},
    {NULL, NULL, 0, NULL}};

static FMField cp_writer_init_list[] = {
    {"contact_info", "string", sizeof(char *),
     FMOffset(cp_writer_init_info, contact_info)},
    {"writer_ID", "integer", sizeof(void *),
     FMOffset(cp_writer_init_info, writer_ID)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec cp_writer_init_structs[] = {
    {"cp_writer", cp_writer_init_list, sizeof(struct _cp_writer_init_info),
     NULL},
    {NULL, NULL, 0, NULL}};

static FMField cp_dp_pair_list[] = {
    {"cp_info", "*CP_STRUCT", 0, FMOffset(struct _CP_DP_pair_info *, cp)},
    {"dp_info", "*DP_STRUCT", 0, FMOffset(struct _CP_DP_pair_info *, dp)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec cp_dp_pair_structs[] = {
    {"CP_DP_pair", cp_dp_pair_list, sizeof(struct _CP_DP_pair_info), NULL},
    {NULL, NULL, 0, NULL}};

static FMStructDescRec cp_dp_writer_pair_structs[] = {
    {"CP_DP_writer_pair", cp_dp_pair_list, sizeof(struct _CP_DP_pair_info),
     NULL},
    {NULL, NULL, 0, NULL}};

static FMField cp_dp_array_reader_list[] = {
    {"reader_cohort_size", "integer", sizeof(int),
     FMOffset(struct _combined_reader_info *, reader_cohort_size)},
    {"CP_reader_info", "(*CP_STRUCT)[reader_cohort_size]",
     sizeof(struct _cp_reader_init_info),
     FMOffset(struct _combined_reader_info *, CP_reader_info)},
    {"DP_reader_info", "(*DP_STRUCT)[reader_cohort_size]", 0,
     FMOffset(struct _combined_reader_info *, DP_reader_info)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec cp_dp_reader_array_structs[] = {
    {"combined_reader_info", cp_dp_array_reader_list,
     sizeof(struct _combined_reader_info), NULL},
    {NULL, NULL, 0, NULL}};

static FMField cp_dp_array_writer_list[] = {
    {"writer_cohort_size", "integer", sizeof(int),
     FMOffset(struct _combined_writer_info *, writer_cohort_size)},
    {"CP_writer_info", "(*CP_STRUCT)[writer_cohort_size]",
     sizeof(struct _cp_writer_init_info),
     FMOffset(struct _combined_writer_info *, CP_writer_info)},
    {"DP_writer_info", "(*DP_STRUCT)[writer_cohort_size]", 0,
     FMOffset(struct _combined_writer_info *, DP_writer_info)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec cp_dp_writer_array_structs[] = {
    {"combined_writer_info", cp_dp_array_writer_list,
     sizeof(struct _combined_writer_info), NULL},
    {NULL, NULL, 0, NULL}};

static FMField cp_reader_register_list[] = {
    {"writer_ID", "integer", sizeof(void *),
     FMOffset(struct _reader_register_msg *, writer_file)},
    {"writer_response_condition", "integer", sizeof(int),
     FMOffset(struct _reader_register_msg *, writer_response_condition)},
    {"reader_cohort_size", "integer", sizeof(int),
     FMOffset(struct _reader_register_msg *, reader_cohort_size)},
    {"cp_reader_info", "(*CP_STRUCT)[reader_cohort_size]",
     sizeof(struct _cp_reader_init_info),
     FMOffset(struct _reader_register_msg *, CP_reader_info)},
    {"dp_reader_info", "(*DP_STRUCT)[reader_cohort_size]", 0,
     FMOffset(struct _reader_register_msg *, DP_reader_info)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec cp_reader_register_structs[] = {
    {"reader_register", cp_reader_register_list,
     sizeof(struct _reader_register_msg), NULL},
    {NULL, NULL, 0, NULL}};

static FMField cp_writer_response_list[] = {
    {"writer_response_condition", "integer", sizeof(int),
     FMOffset(struct _writer_response_msg *, writer_response_condition)},
    {"writer_cohort_size", "integer", sizeof(int),
     FMOffset(struct _writer_response_msg *, writer_cohort_size)},
    {"cp_writer_info", "(*CP_STRUCT)[writer_cohort_size]",
     sizeof(struct _cp_writer_init_info),
     FMOffset(struct _writer_response_msg *, CP_writer_info)},
    {"dp_writer_info", "(*DP_STRUCT)[writer_cohort_size]", 0,
     FMOffset(struct _writer_response_msg *, DP_writer_info)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec cp_writer_response_structs[] = {
    {"writer_response", cp_writer_response_list,
     sizeof(struct _writer_response_msg), NULL},
    {NULL, NULL, 0, NULL}};

static FMField sst_metadata_list[] = {
    {"data_size", "integer", sizeof(size_t),
     FMOffset(struct _sst_metadata *, data_size)},
    {"var_count", "integer", sizeof(int),
     FMOffset(struct _sst_metadata *, var_count)},
    {"vars", "var_metadata[var_count]", sizeof(struct _sst_var_meta),
     FMOffset(struct _sst_metadata *, vars)},
    {NULL, NULL, 0, 0}};

static FMField sst_var_meta_list[] = {
    {"var_name", "string", sizeof(char *),
     FMOffset(struct _sst_var_meta *, var_name)},
    {"dimension_count", "integer", sizeof(int),
     FMOffset(struct _sst_var_meta *, dimension_count)},
    {"dimensions", "var_dimension[dimension_count]",
     sizeof(struct _sst_dimen_meta),
     FMOffset(struct _sst_var_meta *, dimensions)},
    {"data_offset_in_block", "integer", sizeof(int),
     FMOffset(struct _sst_var_meta *, data_offset_in_block)},
    {NULL, NULL, 0, 0}};

static FMField sst_dimen_meta_list[] = {
    {"offset", "integer", sizeof(int),
     FMOffset(struct _sst_dimen_meta *, offset)},
    {"size", "integer", sizeof(int), FMOffset(struct _sst_dimen_meta *, size)},
    {"global_size", "integer", sizeof(int),
     FMOffset(struct _sst_dimen_meta *, global_size)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec sst_metadata_structs[] = {
    {"sst_metadata", sst_metadata_list, sizeof(struct _sst_metadata), NULL},
    {"var_metadata", sst_var_meta_list, sizeof(struct _sst_var_meta), NULL},
    {"var_dimension", sst_dimen_meta_list, sizeof(struct _sst_dimen_meta),
     NULL},
    {NULL, NULL, 0, NULL}};

static FMField timestep_metadata_list[] = {
    {"RS_stream", "integer", sizeof(void *),
     FMOffset(struct _timestep_metadata_msg *, RS_stream)},
    {"timestep", "integer", sizeof(int),
     FMOffset(struct _timestep_metadata_msg *, timestep)},
    {"cohort_size", "integer", sizeof(int),
     FMOffset(struct _timestep_metadata_msg *, cohort_size)},
    {"metadata", "(*sst_metadata)[cohort_size]", sizeof(struct _sst_metadata),
     FMOffset(struct _timestep_metadata_msg *, metadata)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec timestep_metadata_structs[] = {
    {"timestep_metadata", timestep_metadata_list,
     sizeof(struct _timestep_metadata_msg), NULL},
    {"sst_metadata", sst_metadata_list, sizeof(struct _sst_metadata), NULL},
    {"var_metadata", sst_var_meta_list, sizeof(struct _sst_var_meta), NULL},
    {"var_dimension", sst_dimen_meta_list, sizeof(struct _sst_dimen_meta),
     NULL},
    {NULL, NULL, 0, NULL}};

static void replaceFormatNameInFieldList(FMStructDescList l, char *orig,
                                         char *repl, int repl_size)
{
    int i = 0;
    while (l[i].format_name) {
        int j = 0;
        while (l[i].field_list[j].field_name) {
            char *loc;
            if ((loc = strstr(l[i].field_list[j].field_type, orig))) {
                char *old = (char *)l[i].field_list[j].field_type;
                char *new =
                    malloc(strlen(old) - strlen(orig) + strlen(repl) + 1);
                strncpy(new, old, loc - old);
                new[loc - old] = 0;
                strcat(new, repl);
                strcat(new, loc + strlen(orig));
                free(old);
                l[i].field_list[j].field_type = new;
                l[i].field_list[j].field_size = repl_size;
            }
            j++;
        }
        i++;
    }
}

/*
 * generated a combined FMStructDescList from separate top-level, cp and dp
 * formats
 * the format names/sizes "CP_STRUCT" and "DP_STRUCT" used in top-level field
 * lists are replaced by
 * the actual names/sizes provided.
 */
static FMStructDescList combineCpDpFormats(FMStructDescList top,
                                           FMStructDescList cp,
                                           FMStructDescList dp)
{
    FMStructDescList CombinedFormats = NULL;
    int i = 0, topCount = 0, cpCount = 0, dpCount = 0;
    CombinedFormats = FMcopy_struct_list(top);

    i = 0;
    while (top[i++].format_name)
        topCount++;

    i = 0;
    while (cp[i++].format_name)
        cpCount++;

    i = 0;
    while (dp[i++].format_name)
        dpCount++;

    CombinedFormats =
        realloc(CombinedFormats, sizeof(CombinedFormats[0]) *
                                     (topCount + cpCount + dpCount + 1));
    for (i = 0; i < cpCount; i++) {
        CombinedFormats[topCount + i].format_name = strdup(cp[i].format_name);
        CombinedFormats[topCount + i].field_list =
            copy_field_list(cp[i].field_list);
        CombinedFormats[topCount + i].struct_size = cp[i].struct_size;
        CombinedFormats[topCount + i].opt_info = NULL;
    }

    for (i = 0; i < dpCount; i++) {
        CombinedFormats[topCount + cpCount + i].format_name =
            strdup(dp[i].format_name);
        CombinedFormats[topCount + cpCount + i].field_list =
            copy_field_list(dp[i].field_list);
        CombinedFormats[topCount + cpCount + i].struct_size = dp[i].struct_size;
        CombinedFormats[topCount + cpCount + i].opt_info = NULL;
    }
    CombinedFormats[topCount + cpCount + dpCount].format_name = NULL;
    CombinedFormats[topCount + cpCount + dpCount].field_list = NULL;
    CombinedFormats[topCount + cpCount + dpCount].struct_size = 0;
    CombinedFormats[topCount + cpCount + dpCount].opt_info = NULL;

    replaceFormatNameInFieldList(CombinedFormats, "CP_STRUCT",
                                 cp[0].format_name, cp[0].struct_size);
    replaceFormatNameInFieldList(CombinedFormats, "DP_STRUCT",
                                 dp[0].format_name, dp[0].struct_size);
    return CombinedFormats;
}

void **consolidateDataToRankZero(adios2_stream stream, void *local_info,
                                 FFSTypeHandle type, void **ret_data_block)
{
    FFSBuffer buf = create_FFSBuffer();
    int data_size;
    int *recvcounts = NULL;
    char *buffer;

    struct _CP_DP_init_info **pointers = NULL;

    buffer = FFSencode(buf, FMFormat_of_original(type), local_info, &data_size);

    if (stream->rank == 0) {
        recvcounts = malloc(stream->cohort_size * sizeof(int));
    }
    MPI_Gather(&data_size, 1, MPI_INT, recvcounts, 1, MPI_INT, 0,
               MPI_COMM_WORLD);

    /*
     * Figure out the total length of block
     * and displacements for each rank
     */

    int *displs = NULL;
    char *recvbuffer = NULL;

    if (stream->rank == 0) {
        int totlen = 0;
        displs = malloc(stream->cohort_size * sizeof(int));

        displs[0] = 0;
        totlen = (recvcounts[0] + 7) & ~7;

        for (int i = 1; i < stream->cohort_size; i++) {
            int round_up = (recvcounts[i] + 7) & ~7;
            displs[i] = totlen;
            totlen += round_up;
        }

        recvbuffer = malloc(totlen * sizeof(char));
    }

    /*
     * Now we have the receive buffer, counts, and displacements, and
     * can gather the data
     */

    MPI_Gatherv(buffer, data_size, MPI_CHAR, recvbuffer, recvcounts, displs,
                MPI_CHAR, 0, MPI_COMM_WORLD);

    if (stream->rank == 0) {
        FFSContext context = stream->CPInfo->ffs_c;
        //        FFSTypeHandle ffs_type = FFSTypeHandle_from_encode(context,
        //        recvbuffer);

        int i;
        pointers = malloc(stream->cohort_size * sizeof(pointers[0]));
        for (i = 0; i < stream->cohort_size; i++) {
            FFSdecode_in_place(context, recvbuffer + displs[i],
                               (void **)&pointers[i]);
            // printf("Decode for rank %d :\n", i);
            // FMdump_data(FMFormat_of_original(ffs_type), pointers[i],
            // 1024000);
        }
        free(displs);
        free(recvcounts);
    }
    *ret_data_block = recvbuffer;
    return (void **)pointers;
}

void *distributeDataFromRankZero(adios2_stream stream, void *root_info,
                                 FFSTypeHandle type, void **ret_data_block)
{
    int data_size;
    char *buffer;
    void *ret_val;

    if (stream->rank == 0) {
        FFSBuffer buf = create_FFSBuffer();
        char *tmp =
            FFSencode(buf, FMFormat_of_original(type), root_info, &data_size);
        MPI_Bcast(&data_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(tmp, data_size, MPI_CHAR, 0, MPI_COMM_WORLD);
        buffer = malloc(data_size);
        memcpy(buffer, tmp, data_size);
        free_FFSBuffer(buf);
    } else {
        MPI_Bcast(&data_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
        buffer = malloc(data_size);
        MPI_Bcast(buffer, data_size, MPI_CHAR, 0, MPI_COMM_WORLD);
    }

    FFSContext context = stream->CPInfo->ffs_c;
    // FFSTypeHandle ffs_type = FFSTypeHandle_from_encode(context, buffer);

    FFSdecode_in_place(context, buffer, &ret_val);
    // printf("Decode for rank %d is : \n", stream->rank);
    // FMdump_data(FMFormat_of_original(ffs_type), ret_val, 1024000);
    *ret_data_block = buffer;
    return ret_val;
}

void **consolidateDataToAll(adios2_stream stream, void *local_info,
                            FFSTypeHandle type, void **ret_data_block)
{
    FFSBuffer buf = create_FFSBuffer();
    int data_size;
    int *recvcounts = NULL;
    char *buffer;

    struct _CP_DP_init_info **pointers = NULL;

    buffer = FFSencode(buf, FMFormat_of_original(type), local_info, &data_size);

    recvcounts = malloc(stream->cohort_size * sizeof(int));

    MPI_Allgather(&data_size, 1, MPI_INT, recvcounts, 1, MPI_INT,
                  MPI_COMM_WORLD);

    /*
     * Figure out the total length of block
     * and displacements for each rank
     */

    int *displs = NULL;
    char *recvbuffer = NULL;

    int totlen = 0;
    displs = malloc(stream->cohort_size * sizeof(int));

    displs[0] = 0;
    totlen = (recvcounts[0] + 7) & ~7;

    for (int i = 1; i < stream->cohort_size; i++) {
        int round_up = (recvcounts[i] + 7) & ~7;
        displs[i] = totlen;
        totlen += round_up;
    }

    recvbuffer = malloc(totlen * sizeof(char));

    /*
     * Now we have the receive buffer, counts, and displacements, and
     * can gather the data
     */

    MPI_Allgatherv(buffer, data_size, MPI_CHAR, recvbuffer, recvcounts, displs,
                   MPI_CHAR, MPI_COMM_WORLD);

    FFSContext context = stream->CPInfo->ffs_c;

    int i;
    pointers = malloc(stream->cohort_size * sizeof(pointers[0]));
    for (i = 0; i < stream->cohort_size; i++) {
        FFSdecode_in_place(context, recvbuffer + displs[i],
                           (void **)&pointers[i]);
        //    FFSTypeHandle ffs_type = FFSTypeHandle_from_encode(context,
        //    recvbuffer);
        //        printf("Decode for rank %d :\n", i);
        //        FMdump_data(FMFormat_of_original(ffs_type), pointers[i],
        //        1024000);
    }
    free(displs);
    free(recvcounts);

    *ret_data_block = recvbuffer;
    return (void **)pointers;
}

atom_t CM_TRANSPORT_ATOM = 0;

static void init_atom_list()
{
    if (CM_TRANSPORT_ATOM)
        return;

    CM_TRANSPORT_ATOM = attr_atom_from_string("CM_TRANSPORT");
}

static void doFormatRegistration(cp_global_info_t CPInfo,
                                 CP_DP_Interface DPInfo)
{
    FMStructDescList per_rank_reader_structs, full_reader_register_structs,
        combined_reader_structs;
    FMStructDescList per_rank_writer_structs, full_writer_response_structs,
        combined_writer_structs;
    FMFormat f;

    per_rank_reader_structs =
        combineCpDpFormats(cp_dp_pair_structs, cp_reader_init_structs,
                           DPInfo->ReaderContactFormats);
    f = FMregister_data_format(CPInfo->fm_c, per_rank_reader_structs);
    CPInfo->per_rank_reader_format =
        FFSTypeHandle_by_index(CPInfo->ffs_c, FMformat_index(f));
    FFSset_fixed_target(CPInfo->ffs_c, per_rank_reader_structs);

    full_reader_register_structs =
        combineCpDpFormats(cp_reader_register_structs, cp_reader_init_structs,
                           DPInfo->ReaderContactFormats);
    CPInfo->reader_register_format =
        CMregister_format(CPInfo->cm, full_reader_register_structs);
    CMregister_handler(CPInfo->reader_register_format,
                       CP_reader_register_handler, NULL);

    combined_reader_structs =
        combineCpDpFormats(cp_dp_reader_array_structs, cp_reader_init_structs,
                           DPInfo->ReaderContactFormats);
    f = FMregister_data_format(CPInfo->fm_c, combined_reader_structs);
    CPInfo->combined_reader_format =
        FFSTypeHandle_by_index(CPInfo->ffs_c, FMformat_index(f));
    FFSset_fixed_target(CPInfo->ffs_c, combined_reader_structs);

    per_rank_writer_structs =
        combineCpDpFormats(cp_dp_writer_pair_structs, cp_writer_init_structs,
                           DPInfo->WriterContactFormats);
    f = FMregister_data_format(CPInfo->fm_c, per_rank_writer_structs);
    CPInfo->per_rank_writer_format =
        FFSTypeHandle_by_index(CPInfo->ffs_c, FMformat_index(f));
    FFSset_fixed_target(CPInfo->ffs_c, per_rank_writer_structs);

    full_writer_response_structs =
        combineCpDpFormats(cp_writer_response_structs, cp_writer_init_structs,
                           DPInfo->WriterContactFormats);
    CPInfo->writer_response_format =
        CMregister_format(CPInfo->cm, full_writer_response_structs);
    CMregister_handler(CPInfo->writer_response_format,
                       CP_writer_response_handler, NULL);

    combined_writer_structs =
        combineCpDpFormats(cp_dp_writer_array_structs, cp_writer_init_structs,
                           DPInfo->WriterContactFormats);
    f = FMregister_data_format(CPInfo->fm_c, combined_writer_structs);
    CPInfo->combined_writer_format =
        FFSTypeHandle_by_index(CPInfo->ffs_c, FMformat_index(f));
    FFSset_fixed_target(CPInfo->ffs_c, combined_writer_structs);

    f = FMregister_data_format(CPInfo->fm_c, sst_metadata_structs);
    CPInfo->metadata_format =
        FFSTypeHandle_by_index(CPInfo->ffs_c, FMformat_index(f));
    FFSset_fixed_target(CPInfo->ffs_c, sst_metadata_structs);

    CPInfo->timestep_metadata_format =
        CMregister_format(CPInfo->cm, timestep_metadata_structs);
    CMregister_handler(CPInfo->timestep_metadata_format,
                       CP_timestep_metadata_handler, NULL);
}

extern cp_global_info_t CP_get_CPInfo(CP_DP_Interface DPInfo)
{
    static cp_global_info_t CPInfo = NULL;

    if (CPInfo)
        return CPInfo;

    init_atom_list();

    CPInfo = malloc(sizeof(*CPInfo));
    memset(CPInfo, 0, sizeof(*CPInfo));

    CPInfo->cm = CManager_create();
    CMfork_comm_thread(CPInfo->cm);

    attr_list listen_list = create_attr_list();
    set_string_attr(listen_list, CM_TRANSPORT_ATOM, strdup("enet"));
    CMlisten_specific(CPInfo->cm, listen_list);
    free_attr_list(listen_list);

    CPInfo->fm_c = CMget_FMcontext(CPInfo->cm);
    CPInfo->ffs_c = create_FFSContext_FM(CPInfo->fm_c);

    doFormatRegistration(CPInfo, DPInfo);

    return CPInfo;
}

adios2_stream CP_new_stream()
{
    adios2_stream stream = malloc(sizeof(*stream));
    memset(stream, 0, sizeof(*stream));
    pthread_mutex_init(&stream->data_lock, NULL);
    pthread_cond_init(&stream->data_condition, NULL);
    stream->verbose = 1;
    return stream;
}
