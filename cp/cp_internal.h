#include "dp_interface.h"
#include <pthread.h>

typedef struct _cp_global_info {
    /* exchange info */
    CManager cm;
    FFSContext ffs_c;
    FMContext fm_c;
    FFSTypeHandle per_rank_reader_format;
    FFSTypeHandle combined_reader_format;
    CMFormat reader_register_format;
    FFSTypeHandle per_rank_writer_format;
    FFSTypeHandle combined_writer_format;
    CMFormat writer_response_format;
    FFSTypeHandle metadata_format;
    CMFormat timestep_metadata_format;
} * cp_global_info_t;

struct _reader_register_msg;

typedef struct _request_queue {
    struct _reader_register_msg *msg;
    CMConnection conn;
    struct _request_queue *next;
} * request_queue;

typedef struct _WS_reader_connection {
    attr_list contact_list;
    void *remote_stream_ID;
    CMConnection CMconn;
} WS_reader_connection;

typedef struct _WS_reader_info {
    adios2_stream parent_stream;
    DP_WSR_stream DP_WSR_Stream;
    void *RS_Stream_ID;
    int reader_cohort_size;
    int *peers;
    WS_reader_connection *connections;
} * WS_reader_info;

struct _timestep_metadata_list {
    struct _timestep_metadata_msg *metadata;
    struct _timestep_metadata_list *next;
};

struct _sst_stream {
    cp_global_info_t CPInfo;

    MPI_Comm mpiComm;

    /* params */
    int wait_for_first_reader;

    /* state */

    /* MPI info */
    int rank;
    int cohort_size;


    SST_DP_Interface DP_Interface;
    void *DPstream;

    pthread_mutex_t data_lock;
    pthread_cond_t data_condition;

    /* WRITER-SIDE FIELDS */
    int writer_timestep;

    /* rendezvous condition */
    int first_reader_condition;
    request_queue read_request_queue;

    int reader_count;
    WS_reader_info readers;

    /* READER-SIDE FIELDS */
    struct _timestep_metadata_list *timesteps;
};

/*
 * This is the baseline contact information for each reader-side rank.
 * It will be gathered and provided to writer ranks
 */
typedef struct _cp_reader_init_info {
    char *contact_info;
    int target_stone;
    void *reader_ID;
} * cp_reader_init_info;

/*
 * This is the structure that holds reader_side CP and DP contact info for a
 * single rank.
 * This is gathered on reader side.
 */
struct _CP_DP_pair_info {
    void **cp;
    void **dp;
};

/*
 * Reader register messages are sent from reader rank 0 to writer rank 0
 * They contain basic info, plus contact information for each reader rank
 */
struct _reader_register_msg {
    void *writer_file;
    int writer_response_condition;
    int reader_cohort_size;
    cp_reader_init_info *CP_reader_info;
    void **DP_reader_info;
};

/*
 * This is the consolidated reader contact info structure that is used to
 * diseminate full reader contact information to all writer ranks
 */
typedef struct _combined_reader_info {
    int reader_cohort_size;
    cp_reader_init_info *CP_reader_info;
    void **DP_reader_info;
} * reader_data_t;

/*
 * This is the baseline contact information for each writer-side rank.
 * It will be gathered and provided to reader ranks
 */
typedef struct _cp_writer_init_info {
    char *contact_info;
    void *writer_ID;
} * cp_writer_init_info;

/*
 * Writer response messages from writer rank 0 to reader rank 0 after the
 * initial contact request.
 * They contain basic info, plus contact information for each reader rank
 */
struct _writer_response_msg {
    int writer_response_condition;
    int writer_cohort_size;
    cp_writer_init_info *CP_writer_info;
    void **DP_writer_info;
};

/*
 * The timestep_metadata message carries the metadata from all writer ranks.
 * One is sent to each reader.
 */
struct _timestep_metadata_msg {
    void *RS_stream;
    int timestep;
    int cohort_size;
    adios2_metadata *metadata;
};

/*
 * This is the consolidated writer contact info structure that is used to
 * diseminate full writer contact information to all reader ranks
 */
typedef struct _combined_writer_info {
    int writer_cohort_size;
    cp_writer_init_info *CP_writer_info;
    void **DP_writer_info;
} * writer_data_t;

extern atom_t CM_TRANSPORT_ATOM;

void CP_parse_params(adios2_stream stream, char *params);
extern cp_global_info_t CP_get_CPInfo(SST_DP_Interface DPInfo);
void **consolidateDataToRankZero(adios2_stream stream, void *local_info,
                                 FFSTypeHandle type, void **ret_data_block);
void **consolidateDataToAll(adios2_stream stream, void *local_info,
                            FFSTypeHandle type, void **ret_data_block);
void *distributeDataFromRankZero(adios2_stream stream, void *root_info,
                                 FFSTypeHandle type, void **ret_data_block);
extern void CP_reader_register_handler(CManager cm, CMConnection conn,
                                       void *msg_v, void *client_data,
                                       attr_list attrs);
extern void CP_writer_response_handler(CManager cm, CMConnection conn,
                                       void *msg_v, void *client_data,
                                       attr_list attrs);
extern void CP_timestep_metadata_handler(CManager cm, CMConnection conn,
                                         void *msg_v, void *client_data,
                                         attr_list attrs);
extern adios2_stream CP_new_stream();
