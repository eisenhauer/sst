#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <mpi.h>
#include <atl.h>
#include <evpath.h>
#include <pthread.h>

#include "sst.h"
#include "cp_internal.h"

/*
 * this is a global value.  Should only be touched by the main thread, so not
 * protected from simultaneous access.  Only read after first write.
 */

static void write_contact_info(char *name, adios2_stream stream)
{
    char *contact = attr_list_to_string(CMget_contact_list(stream->CPInfo->cm));
    char *tmp_name = malloc(strlen(name) + strlen(".tmp") + 1);
    char *file_name = malloc(strlen(name) + strlen(".bpflx") + 1);
    FILE *writer_info;
    sprintf(tmp_name, "%s.tmp", name);
    sprintf(file_name, "%s.bpflx", name);
    writer_info = fopen(tmp_name, "w");
    fprintf(writer_info, "%p:%s", (void *)stream, contact);
    fclose(writer_info);
    rename(tmp_name, file_name);
}

static char *read_contact_info(char *name, adios2_stream stream)
{
    char *file_name = malloc(strlen(name) + strlen(".bpflx") + 1);
    FILE *writer_info;
    sprintf(file_name, "%s.bpflx", name);
    printf("Looking for writer contact in file %s\n", file_name);
redo:
    writer_info = fopen(file_name, "r");
    while (!writer_info) {
        CMusleep(stream->CPInfo->cm, 500);
        writer_info = fopen(file_name, "r");
    }
    struct stat buf;
    fstat(fileno(writer_info), &buf);
    int size = buf.st_size;
    if (size == 0) {
        printf("Size of writer contact file is zero, but it shouldn't be! "
               "Retrying!\n");
        goto redo;
    }

    char *buffer = calloc(1, size + 1);
    (void) fread(buffer, size, 1, writer_info);
    fclose(writer_info);
    return buffer;
}

static
int * setupPeerArray(int my_size, int my_rank, int peer_size)
{
    int portion_size = peer_size / my_size;
    int leftovers = peer_size - portion_size * my_size;
    int start_offset = leftovers;
    int start;
    if (my_rank < leftovers) {
        portion_size++;
        start_offset = 0;
    }
    start = portion_size * my_rank + start_offset;
    int *my_peers = malloc((portion_size + 1) * sizeof(int));
    for (int i=0; i < portion_size; i++) {
        my_peers[i] = start + i;
    }
    my_peers[portion_size] = -1;

    return my_peers;
}
    
static
void initWSReader(WS_reader_info reader, int reader_size, cp_reader_init_info *reader_info)
{
    int writer_size = reader->parent_stream->cohort_size;
    int writer_rank = reader->parent_stream->rank;
    int i;
    reader->reader_cohort_size = reader_size;
    reader->connections = malloc(sizeof(reader->connections[0]) * reader_size);
    for (i = 0; i < reader_size; i++) {
        reader->connections[i].contact_list = attr_list_from_string(reader_info[i]->contact_info);
        reader->connections[i].remote_stream_ID = reader_info[i]->reader_ID;
        reader->connections[i].CMconn = NULL;
    }
    reader->peers = setupPeerArray(writer_size, writer_rank, reader_size);
    i = 0;
    while (reader->peers[i] != -1) {
        int peer = reader->peers[i];
        reader->connections[peer].CMconn = CMget_conn(reader->parent_stream->CPInfo->cm, reader->connections[peer].contact_list);
        i++;
    }
}

void writer_participate_in_reader_open(adios2_stream stream)
{
    request_queue req;
    reader_data_t return_data;
    void *free_block = NULL;
    int writer_response_condition = -1;
    CMConnection conn;
    if (stream->rank == 0) {
        pthread_mutex_lock(&stream->data_lock);
        assert((stream->read_request_queue));
        req = stream->read_request_queue;
        stream->read_request_queue = req->next;
        req->next = NULL;
        pthread_mutex_unlock(&stream->data_lock);
        struct _combined_reader_info reader_data;
        reader_data.reader_cohort_size = req->msg->reader_cohort_size;
        reader_data.CP_reader_info = req->msg->CP_reader_info;
        reader_data.DP_reader_info = req->msg->DP_reader_info;
        return_data = distributeDataFromRankZero(
            stream, &reader_data, stream->CPInfo->combined_reader_format,
            &free_block);
        writer_response_condition = req->msg->writer_response_condition;
        conn = req->conn;
        CMreturn_buffer(stream->CPInfo->cm, req->msg);
        free(req);
    } else {
        return_data = distributeDataFromRankZero(
            stream, NULL, stream->CPInfo->combined_reader_format, &free_block);
    }
    printf("I am writer rank %d, my info on readers is:\n", stream->rank);
    FMdump_data(FMFormat_of_original(stream->CPInfo->combined_reader_format),
                return_data, 1024000);
    printf("\n");

    stream->readers = realloc(stream->readers, sizeof(stream->readers[0]) *
                                                   (stream->reader_count + 1));
    DP_WSR_stream per_reader_stream;
    void *DP_writer_info;
    void *ret_data_block;

    per_reader_stream = stream->DP_Interface->WriterPerReaderInit(
        stream->DPstream, return_data->reader_cohort_size,
        return_data->DP_reader_info, &DP_writer_info);

    stream->readers[stream->reader_count].DP_WSR_Stream = per_reader_stream;
    stream->readers[stream->reader_count].parent_stream = stream;
    initWSReader(&stream->readers[stream->reader_count], return_data->reader_cohort_size,
                   return_data->CP_reader_info);

    stream->reader_count++;

    struct _CP_DP_pair_info combined_init;
    struct _cp_reader_init_info cpInfo;

    struct _CP_DP_pair_info **pointers = NULL;

    cpInfo.contact_info =
        attr_list_to_string(CMget_contact_list(stream->CPInfo->cm));
    cpInfo.target_stone = 42;
    cpInfo.reader_ID = &stream;

    combined_init.cp = (void **)&cpInfo;
    combined_init.dp = DP_writer_info;

    pointers = (struct _CP_DP_pair_info **)consolidateDataToRankZero(
        stream, &combined_init, stream->CPInfo->per_rank_writer_format,
        &ret_data_block);

    if (stream->rank == 0) {
        struct _writer_response_msg response;
        response.writer_response_condition = writer_response_condition;
        response.writer_cohort_size = stream->cohort_size;
        response.CP_writer_info =
            malloc(response.writer_cohort_size * sizeof(void *));
        response.DP_writer_info =
            malloc(response.writer_cohort_size * sizeof(void *));
        for (int i = 0; i < response.writer_cohort_size; i++) {
            response.CP_writer_info[i] =
                (struct _cp_writer_init_info *)pointers[i]->cp;
            response.DP_writer_info[i] = pointers[i]->dp;
        }
        CMwrite(conn, stream->CPInfo->writer_response_format, &response);
    }
}

adios2_stream SstWriterOpen(char *name, char *params, MPI_Comm comm)
{
    adios2_stream stream;

    stream = CP_new_stream();
    CP_parse_params(stream, params);

    stream->DP_Interface = LoadDP("dummy");

    stream->CPInfo = CP_get_CPInfo(stream->DP_Interface);

    stream->mpiComm = comm;
    if (stream->wait_for_first_reader) {
        stream->first_reader_condition =
            CMCondition_get(stream->CPInfo->cm, NULL);
    } else {
        stream->first_reader_condition = -1;
    }

    MPI_Comm_rank(stream->mpiComm, &stream->rank);
    MPI_Comm_size(stream->mpiComm, &stream->cohort_size);

    stream->DPstream = stream->DP_Interface->InitWriter(stream);

    if (stream->rank == 0)
        write_contact_info(name, stream);

    if (stream->wait_for_first_reader) {
        if (stream->rank == 0) {
            pthread_mutex_lock(&stream->data_lock);
            if (stream->read_request_queue == NULL) {
                pthread_cond_wait(&stream->data_condition, &stream->data_lock);
            }
            assert(stream->read_request_queue);
            pthread_mutex_unlock(&stream->data_lock);
        }
        MPI_Barrier(stream->mpiComm);
        printf("Rank %d, participate in reader open\n", stream->rank);
        writer_participate_in_reader_open(stream);

        if (stream->rank == 0) {
            pthread_mutex_lock(&stream->data_lock);
            if (stream->read_request_queue == NULL) {
                pthread_cond_wait(&stream->data_condition, &stream->data_lock);
            }
            assert(stream->read_request_queue);
            pthread_mutex_unlock(&stream->data_lock);
        }
        printf("Rank %d, Waiting for activate message\n", stream->rank);
        MPI_Barrier(stream->mpiComm);
    }
    return stream;
}

void 
sendOneToEachReaderRank(adios2_stream s, CMFormat f, void *msg, void**RS_stream_ptr)
{
    for (int i=0; i < s->reader_count; i++) {
        int j = 0;
        while (s->readers[i].peers[j] != -1) {
            int peer = s->readers[i].peers[j];
            CMConnection conn = s->readers[i].connections[peer].CMconn;
            /* add the reader-rank-specific stream identifier to each outgoing message */
            *RS_stream_ptr = s->readers[i].connections[peer].remote_stream_ID;
            fprintf(stderr, "Rank %d, sending a message to reader rank %d in send one to each reader\n", s->rank, peer);
            CMwrite(conn, f, msg);
            j++;
        }
    }
}

void SstProvideTimestep(adios2_stream s, adios2_metadata local_metadata,
                        adios2_data data, long timestep)
{
    void *data_block;
    adios2_metadata *global_metadata;
    struct _timestep_metadata_msg msg;
    global_metadata = (adios2_metadata *)consolidateDataToAll(
        s, local_metadata, s->CPInfo->metadata_format, &data_block);
    msg.cohort_size = s->cohort_size;
    msg.metadata = global_metadata;
    msg.timestep = s->writer_timestep++;
    sendOneToEachReaderRank(s, s->CPInfo->timestep_metadata_format, &msg, &msg.RS_stream);
    free(data_block);
}

static void **participate_in_reader_init_data_exchange(adios2_stream stream,
                                                       void *dpInfo,
                                                       void **ret_data_block)
{

    struct _CP_DP_pair_info combined_init;
    struct _cp_reader_init_info cpInfo;

    struct _CP_DP_pair_info **pointers = NULL;

    cpInfo.contact_info =
        attr_list_to_string(CMget_contact_list(stream->CPInfo->cm));
    cpInfo.target_stone = 42;
    cpInfo.reader_ID = stream;

    combined_init.cp = (void **)&cpInfo;
    combined_init.dp = dpInfo;

    pointers = (struct _CP_DP_pair_info **)consolidateDataToRankZero(
        stream, &combined_init, stream->CPInfo->per_rank_reader_format,
        ret_data_block);
    return (void **)pointers;
}

adios2_stream SstReaderOpen(char *name, char *params, MPI_Comm comm)
{
    adios2_stream stream;
    void *dpInfo;
    struct _CP_DP_pair_info **pointers;
    void *data_block;
    void *free_block;
    writer_data_t return_data;

    stream = CP_new_stream();

    CP_parse_params(stream, params);

    stream->DP_Interface = LoadDP("dummy");

    stream->CPInfo = CP_get_CPInfo(stream->DP_Interface);

    stream->mpiComm = comm;

    MPI_Comm_rank(stream->mpiComm, &stream->rank);
    MPI_Comm_size(stream->mpiComm, &stream->cohort_size);

    stream->DPstream = stream->DP_Interface->InitReader(stream, &dpInfo);

    pointers =
        (struct _CP_DP_pair_info **)participate_in_reader_init_data_exchange(
            stream, dpInfo, &data_block);

    if (stream->rank == 0) {
        char *writer_0_contact = read_contact_info(name, stream);
        void *writer_file_ID;
        char *cm_contact_string =
            malloc(strlen(writer_0_contact)); /* at least long enough */
        sscanf(writer_0_contact, "%p:%s", &writer_file_ID, cm_contact_string);
        printf("Writer contact info is fileID %p, contact info %s\n",
               writer_file_ID, cm_contact_string);

        attr_list writer_rank0_contact =
            attr_list_from_string(cm_contact_string);
        CMConnection conn =
            CMget_conn(stream->CPInfo->cm, writer_rank0_contact);
        struct _reader_register_msg reader_register;

        reader_register.writer_file = writer_file_ID;
        reader_register.writer_response_condition =
            CMCondition_get(stream->CPInfo->cm, conn);
        reader_register.reader_cohort_size = stream->cohort_size;
        reader_register.CP_reader_info =
            malloc(reader_register.reader_cohort_size * sizeof(void *));
        reader_register.DP_reader_info =
            malloc(reader_register.reader_cohort_size * sizeof(void *));
        for (int i = 0; i < reader_register.reader_cohort_size; i++) {
            reader_register.CP_reader_info[i] =
                (cp_reader_init_info)pointers[i]->cp;
            reader_register.DP_reader_info[i] = pointers[i]->dp;
        }
        /* the response value is set in the handler */
        struct _writer_response_msg *response = NULL;
        CMCondition_set_client_data(stream->CPInfo->cm,
                                    reader_register.writer_response_condition,
                                    &response);

        CMwrite(conn, stream->CPInfo->reader_register_format, &reader_register);
        /* wait for "go" from writer */
        printf("waiting for writer response message in read_open, WAITING, "
               "condition %d\n",
               reader_register.writer_response_condition);
        CMCondition_wait(stream->CPInfo->cm,
                         reader_register.writer_response_condition);
        printf("finished wait writer response message in read_open\n");

        assert(response);
        struct _combined_writer_info writer_data;
        writer_data.writer_cohort_size = response->writer_cohort_size;
        writer_data.CP_writer_info = response->CP_writer_info;
        writer_data.DP_writer_info = response->DP_writer_info;
        return_data = distributeDataFromRankZero(
            stream, &writer_data, stream->CPInfo->combined_writer_format,
            &free_block);
    } else {
        return_data = distributeDataFromRankZero(
            stream, NULL, stream->CPInfo->combined_writer_format, &free_block);
    }
    printf("I am reader rank %d, my info on readers is:\n", stream->rank);
    FMdump_data(FMFormat_of_original(stream->CPInfo->combined_writer_format),
                return_data, 1024000);
    printf("\n");

    return stream;
}

void queue_reader_register_msg_and_notify(adios2_stream stream,
                                          struct _reader_register_msg *req,
                                          CMConnection conn)
{
    pthread_mutex_lock(&stream->data_lock);
    request_queue new = malloc(sizeof(struct _request_queue));
    new->msg = req;
    new->conn = conn;
    if (stream->read_request_queue) {
        request_queue last = stream->read_request_queue;
        while (last->next) {
            last = last->next;
        }
        last->next = new;
    } else {
        stream->read_request_queue = new;
    }
    pthread_cond_signal(&stream->data_condition);
    pthread_mutex_unlock(&stream->data_lock);
}

void queue_timestep_metadata_msg_and_notify(adios2_stream stream,
                                            struct _timestep_metadata_msg *tsm,
                                            CMConnection conn)
{
    pthread_mutex_lock(&stream->data_lock);
    struct _timestep_metadata_list *new = malloc(sizeof(struct _request_queue));
    new->metadata = tsm;
    new->next = NULL;
    if (stream->timesteps) {
        struct _timestep_metadata_list *last = stream->timesteps;
        while (last->next) {
            last = last->next;
        }
        last->next = new;
    } else {
        stream->timesteps = new;
    }
    pthread_cond_signal(&stream->data_condition);
    pthread_mutex_unlock(&stream->data_lock);
}

void CP_reader_register_handler(CManager cm, CMConnection conn, void *msg_v,
                                void *client_data, attr_list attrs)
{
    adios2_stream stream;
    int i;
    struct _reader_register_msg *msg = (struct _reader_register_msg *)msg_v;
    fprintf(stderr,
            "Received a reader registration message directed at writer %p\n",
            msg->writer_file);
    fprintf(stderr, "A reader cohort of size %d is requesting to be added\n",
            msg->reader_cohort_size);
    for (i = 0; i < msg->reader_cohort_size; i++) {
        fprintf(stderr, " rank %d CP contact info: %s, %d, %p\n", i,
                msg->CP_reader_info[i]->contact_info,
                msg->CP_reader_info[i]->target_stone,
                msg->CP_reader_info[i]->reader_ID);
    }
    stream = msg->writer_file;

    /* arrange for this message data to stay around */
    CMtake_buffer(cm, msg);

    queue_reader_register_msg_and_notify(stream, msg, conn);
}

void CP_timestep_metadata_handler(CManager cm, CMConnection conn, void *msg_v,
                                  void *client_data, attr_list attrs)
{
    adios2_stream stream;
    struct _timestep_metadata_msg *msg = (struct _timestep_metadata_msg *)msg_v;
    stream = (adios2_stream) msg->RS_stream;
    fprintf(stderr,
            "Reader %d received an incoming metadata message for timestep %d\n",
            stream->rank, msg->timestep);

    /* arrange for this message data to stay around */
    CMtake_buffer(cm, msg);

    queue_timestep_metadata_msg_and_notify(stream, msg, conn);
}

void CP_writer_response_handler(CManager cm, CMConnection conn, void *msg_v,
                                void *client_data, attr_list attrs)
{
    int i;
    struct _writer_response_msg *msg = (struct _writer_response_msg *)msg_v;
    struct _writer_response_msg **response_ptr;
    fprintf(stderr, "Received a writer_response message for condition %d\n",
            msg->writer_response_condition);
    fprintf(stderr, "The responding writer has cohort of size %d :\n",
            msg->writer_cohort_size);
    for (i = 0; i < msg->writer_cohort_size; i++) {
        fprintf(stderr, " rank %d CP contact info: %s, %p\n", i,
                msg->CP_writer_info[i]->contact_info,
                msg->CP_writer_info[i]->writer_ID);
    }

    /* arrange for this message data to stay around */
    CMtake_buffer(cm, msg);

    /* attach the message to the CMCondition so it an be retrieved by the main
     * thread */
    response_ptr =
        CMCondition_get_client_data(cm, msg->writer_response_condition);
    *response_ptr = msg;

    /* wake the main thread */
    CMCondition_signal(cm, msg->writer_response_condition);
}

extern adios2_full_metadata
SstGetMetadata(adios2_stream stream, long timestep)
{
    struct _timestep_metadata_list *next;
    adios2_full_metadata ret;
    pthread_mutex_lock(&stream->data_lock);
    next = stream->timesteps;
    while (1) {
        while (next) {
            if (next->metadata->timestep == timestep) {
                ret = malloc(sizeof(struct _sst_full_metadata));
                ret->writer_cohort_size = next->metadata->cohort_size;
                ret->writer = next->metadata->metadata;
                pthread_mutex_unlock(&stream->data_lock);
                return ret;
            }
            next = next->next;
        }
        pthread_cond_wait(&stream->data_condition, &stream->data_lock);
    }
    /* NOTREACHED */
    pthread_mutex_unlock(&stream->data_lock);
}
