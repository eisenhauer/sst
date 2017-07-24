#include "dp_interface.h"
#include <pthread.h>

typedef struct _cp_global_info {
    /* exchange info */
    CManager cm;
    FFSContext ffs_c;
    FMContext fm_c;
    FFSTypeHandle PerRankReaderInfoFormat;
    FFSTypeHandle CombinedReaderInfoFormat;
    CMFormat ReaderRegisterFormat;
    FFSTypeHandle PerRankWriterInfoFormat;
    FFSTypeHandle CombinedWriterInfoFormat;
    CMFormat WriterResponseFormat;
    FFSTypeHandle PerRankMetadataFormat;
    CMFormat DeliverTimestepMetadataFormat;
    CMFormat ReleaseTimestepFormat;
} * cp_global_info_t;

struct _reader_register_msg;

typedef struct _request_queue {
    struct _reader_register_msg *msg;
    CMConnection conn;
    struct _request_queue *next;
} * request_queue;

typedef struct _CP_peerConnection {
    attr_list contact_list;
    void *remote_stream_ID;
    CMConnection CMconn;
} CP_peerConnection;

typedef struct _WS_reader_info {
    SstStream parent_stream;
    void *DP_WSR_Stream;
    void *RS_Stream_ID;
    int reader_cohort_size;
    int *peers;
    CP_peerConnection *connections;
} * WS_reader_info;

struct _timestep_metadata_list {
    struct _timestep_metadata_msg *metadata;
    struct _timestep_metadata_list *next;
};

enum StreamRole {ReaderRole, WriterRole};

struct _SstStream {
    cp_global_info_t CPInfo;

    MPI_Comm mpiComm;
    enum StreamRole Role;

    /* params */
    int wait_for_first_reader;

    /* state */
    int verbose;

    /* MPI info */
    int rank;
    int cohort_size;

    CP_DP_Interface DP_Interface;
    void *DPstream;

    pthread_mutex_t data_lock;
    pthread_cond_t data_condition;

    /* WRITER-SIDE FIELDS */
    int writer_timestep;

    /* rendezvous condition */
    int first_reader_condition;
    request_queue read_request_queue;

    int reader_count;
    WS_reader_info *readers;

    /* READER-SIDE FIELDS */
    struct _timestep_metadata_list *timesteps;
    int writer_cohort_size;
    int *peers;
    CP_peerConnection *connections_to_writer;
};

/*
 * This is the baseline contact information for each reader-side rank.
 * It will be gathered and provided to writer ranks
 */
typedef struct _cp_reader_init_info {
    char *contact_info;
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
    SstMetadata *metadata;
};

/*
 * The ReleaseTimestep message informs the writers that this reader is done with
 * a particular timestep.
 * One is sent to each writer rank.
 */
struct _ReleaseTimestepMsg {
    void *WSR_Stream;
    int Timestep;
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

void CP_parse_params(SstStream stream, char *params);
extern cp_global_info_t CP_get_CPInfo(CP_DP_Interface DPInfo);
void **consolidateDataToRankZero(SstStream stream, void *local_info,
                                 FFSTypeHandle type, void **ret_data_block);
void **consolidateDataToAll(SstStream stream, void *local_info,
                            FFSTypeHandle type, void **ret_data_block);
void *distributeDataFromRankZero(SstStream stream, void *root_info,
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
extern void CP_ReleaseTimestepHandler(CManager cm, CMConnection conn,
                                      void *msg_v, void *client_data,
                                      attr_list attrs);
extern SstStream CP_new_stream();
