#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <atl.h>
#include <evpath.h>
#include <mpi.h>
#include <pthread.h>

#include "sst.h"
#include "cp_internal.h"

extern void CP_verbose(SstStream stream, char *format, ...);
static void DP_verbose(SstStream stream, char *format, ...);
static CManager CP_getCManager(SstStream stream);
static void CP_sendToPeer(SstStream stream, CP_PeerCohort cohort, int rank,
                          CMFormat format, void *data);
static int CP_myRank(SstStream stream);

struct _CP_Services Svcs = {
    (CP_VerboseFunc)DP_verbose, (CP_GetCManagerFunc)CP_getCManager,
    (CP_SendToPeerFunc)CP_sendToPeer, (CP_MyRankFunc)CP_myRank};

static void writeContactInfo(char *name, SstStream stream)
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

static char *readContactInfo(char *name, SstStream stream)
{
    char *file_name = malloc(strlen(name) + strlen(".bpflx") + 1);
    FILE *writer_info;
    sprintf(file_name, "%s.bpflx", name);
//    printf("Looking for writer contact in file %s\n", file_name);
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
        //        printf("Size of writer contact file is zero, but it shouldn't
        //        be! "
        //               "Retrying!\n");
        goto redo;
    }

    char *buffer = calloc(1, size + 1);
    (void)fread(buffer, size, 1, writer_info);
    fclose(writer_info);
    return buffer;
}

static int *setupPeerArray(int my_size, int my_rank, int peer_size)
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
    for (int i = 0; i < portion_size; i++) {
        my_peers[i] = start + i;
    }
    my_peers[portion_size] = -1;

    return my_peers;
}

static void initWSReader(WS_reader_info reader, int reader_size,
                         cp_reader_init_info *reader_info)
{
    int writer_size = reader->parent_stream->cohort_size;
    int writer_rank = reader->parent_stream->rank;
    int i;
    reader->reader_cohort_size = reader_size;
    reader->connections = calloc(sizeof(reader->connections[0]), reader_size);
    for (i = 0; i < reader_size; i++) {
        reader->connections[i].contact_list =
            attr_list_from_string(reader_info[i]->contact_info);
        reader->connections[i].remote_stream_ID = reader_info[i]->reader_ID;
        reader->connections[i].CMconn = NULL;
    }
    reader->peers = setupPeerArray(writer_size, writer_rank, reader_size);
    i = 0;
    while (reader->peers[i] != -1) {
        int peer = reader->peers[i];
        reader->connections[peer].CMconn =
            CMget_conn(reader->parent_stream->CPInfo->cm,
                       reader->connections[peer].contact_list);
        i++;
    }
}

void writer_participate_in_reader_open(SstStream stream)
{
    request_queue req;
    reader_data_t return_data;
    void *free_block = NULL;
    int writer_response_condition = -1;
    CMConnection conn;
    CP_verbose(stream, "Beginning writer-side reader open protocol\n");
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
            stream, &reader_data, stream->CPInfo->CombinedReaderInfoFormat,
            &free_block);
        writer_response_condition = req->msg->writer_response_condition;
        conn = req->conn;
        CMreturn_buffer(stream->CPInfo->cm, req->msg);
        free(req);
    } else {
        return_data = distributeDataFromRankZero(
            stream, NULL, stream->CPInfo->CombinedReaderInfoFormat,
            &free_block);
    }
    //    printf("I am writer rank %d, my info on readers is:\n", stream->rank);
    //    FMdump_data(FMFormat_of_original(stream->CPInfo->combined_reader_format),
    //                return_data, 1024000);
    //    printf("\n");

    stream->readers = realloc(stream->readers, sizeof(stream->readers[0]) *
                                                   (stream->reader_count + 1));
    DP_WSR_Stream per_reader_stream;
    void *DP_writer_info;
    void *ret_data_block;
    CP_peerConnection *connections_to_reader;
    connections_to_reader =
        calloc(sizeof(CP_peerConnection), return_data->reader_cohort_size);
    for (int i = 0; i < return_data->reader_cohort_size; i++) {
        attr_list attrs =
            attr_list_from_string(return_data->CP_reader_info[i]->contact_info);
        connections_to_reader[i].contact_list = attrs;
        connections_to_reader[i].remote_stream_ID =
            return_data->CP_reader_info[i]->reader_ID;
    }

    per_reader_stream = stream->DP_Interface->initWriterPerReader(
        &Svcs, stream->DPstream, return_data->reader_cohort_size,
        connections_to_reader, return_data->DP_reader_info, &DP_writer_info);

    WS_reader_info CP_WSR_Stream = malloc(sizeof(*CP_WSR_Stream));
    stream->readers[stream->reader_count] = CP_WSR_Stream;
    CP_WSR_Stream->DP_WSR_Stream = per_reader_stream;
    CP_WSR_Stream->parent_stream = stream;
    CP_WSR_Stream->connections = connections_to_reader;
    initWSReader(CP_WSR_Stream, return_data->reader_cohort_size,
                 return_data->CP_reader_info);

    stream->reader_count++;

    struct _CP_DP_pair_info combined_init;
    struct _cp_writer_init_info cpInfo;

    struct _CP_DP_pair_info **pointers = NULL;

    cpInfo.contact_info =
        attr_list_to_string(CMget_contact_list(stream->CPInfo->cm));
    cpInfo.writer_ID = CP_WSR_Stream;

    combined_init.cp = (void **)&cpInfo;
    combined_init.dp = DP_writer_info;

    pointers = (struct _CP_DP_pair_info **)consolidateDataToRankZero(
        stream, &combined_init, stream->CPInfo->PerRankWriterInfoFormat,
        &ret_data_block);

    if (stream->rank == 0) {
        struct _writer_response_msg response;
        memset(&response, 0, sizeof(response));
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
        CMwrite(conn, stream->CPInfo->WriterResponseFormat, &response);
    }
    CP_verbose(stream, "Finish writer-side reader open protocol\n");
}

SstStream SstWriterOpen(char *name, char *params, MPI_Comm comm)
{
    SstStream stream;

    stream = CP_new_stream();
    stream->Role = WriterRole;
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

    stream->DPstream = stream->DP_Interface->initWriter(&Svcs, stream);

    if (stream->rank == 0) {
        writeContactInfo(name, stream);
    }

    CP_verbose(stream, "Opening stream \"%s\"\n", name);

    if (stream->wait_for_first_reader) {
        CP_verbose(
            stream,
            "Stream parameter requires rendezvous, waiting for first reader\n");
        if (stream->rank == 0) {
            pthread_mutex_lock(&stream->data_lock);
            if (stream->read_request_queue == NULL) {
                pthread_cond_wait(&stream->data_condition, &stream->data_lock);
            }
            assert(stream->read_request_queue);
            pthread_mutex_unlock(&stream->data_lock);
        }
        MPI_Barrier(stream->mpiComm);

        writer_participate_in_reader_open(stream);

        MPI_Barrier(stream->mpiComm);
    }
    CP_verbose(stream, "Finish opening stream \"%s\"\n", name);
    return stream;
}

void sendOneToEachReaderRank(SstStream s, CMFormat f, void *msg,
                             void **RS_stream_ptr)
{
    for (int i = 0; i < s->reader_count; i++) {
        int j = 0;
        WS_reader_info CP_WSR_Stream = s->readers[i];
        while (CP_WSR_Stream->peers[j] != -1) {
            int peer = CP_WSR_Stream->peers[j];
            CMConnection conn = CP_WSR_Stream->connections[peer].CMconn;
            /* add the reader-rank-specific stream identifier to each outgoing
             * message */
            *RS_stream_ptr = CP_WSR_Stream->connections[peer].remote_stream_ID;
            CMwrite(conn, f, msg);
            j++;
        }
    }
}

void SstProvideTimestep(SstStream s, SstMetadata LocalMetadata, SstData Data,
                        long Timestep)
{
    void *data_block;
    MetadataPlusDPInfo *pointers;
    struct _timestep_metadata_msg msg;
    void *DP_TimestepInfo = NULL;
    struct _MetadataPlusDPInfo Md;

    s->DP_Interface->provideTimestep(&Svcs, s->DPstream, Data, LocalMetadata,
                                     Timestep, &DP_TimestepInfo);

    Md.Metadata = LocalMetadata;
    Md.DP_TimestepInfo = DP_TimestepInfo;

    pointers = (MetadataPlusDPInfo *)consolidateDataToAll(
        s, &Md, s->CPInfo->PerRankMetadataFormat, &data_block);

    msg.cohort_size = s->cohort_size;
    msg.timestep = s->writer_timestep++;

    /* separate metadata and DP_info to separate arrays */
    msg.metadata = malloc(s->cohort_size * sizeof(void *));
    msg.DP_TimestepInfo = malloc(s->cohort_size * sizeof(void *));
    int NullCount = 0;
    for (int i = 0; i < s->cohort_size; i++) {
        msg.metadata[i] = pointers[i]->metadata;
        msg.DP_TimestepInfo[i] = pointers[i]->DP_TimestepInfo;
        if (pointers[i]->DP_TimestepInfo == NULL)
            NullCount++;
    }
    if (NullCount == s->cohort_size) {
        free(msg.DP_TimestepInfo);
        msg.DP_TimestepInfo = NULL;
    }

    CP_verbose(s,
               "Sending TimestepMetadata for timestep %d, one to each reader\n",
               Timestep);

    sendOneToEachReaderRank(s, s->CPInfo->DeliverTimestepMetadataFormat, &msg,
                            &msg.RS_stream);
    free(data_block);
}

static void **participate_in_reader_init_data_exchange(SstStream stream,
                                                       void *dpInfo,
                                                       void **ret_data_block)
{

    struct _CP_DP_pair_info combined_init;
    struct _cp_reader_init_info cpInfo;

    struct _CP_DP_pair_info **pointers = NULL;

    cpInfo.contact_info =
        attr_list_to_string(CMget_contact_list(stream->CPInfo->cm));
    cpInfo.reader_ID = stream;

    combined_init.cp = (void **)&cpInfo;
    combined_init.dp = dpInfo;

    pointers = (struct _CP_DP_pair_info **)consolidateDataToRankZero(
        stream, &combined_init, stream->CPInfo->PerRankReaderInfoFormat,
        ret_data_block);
    return (void **)pointers;
}

SstStream SstReaderOpen(char *name, char *params, MPI_Comm comm)
{
    SstStream stream;
    void *dpInfo;
    struct _CP_DP_pair_info **pointers;
    void *data_block;
    void *free_block;
    writer_data_t return_data;

    stream = CP_new_stream();
    stream->Role = ReaderRole;

    CP_parse_params(stream, params);

    stream->DP_Interface = LoadDP("dummy");

    stream->CPInfo = CP_get_CPInfo(stream->DP_Interface);

    stream->mpiComm = comm;

    MPI_Comm_rank(stream->mpiComm, &stream->rank);
    MPI_Comm_size(stream->mpiComm, &stream->cohort_size);

    stream->DPstream = stream->DP_Interface->initReader(&Svcs, stream, &dpInfo);

    pointers =
        (struct _CP_DP_pair_info **)participate_in_reader_init_data_exchange(
            stream, dpInfo, &data_block);

    if (stream->rank == 0) {
        char *writer_0_contact = readContactInfo(name, stream);
        void *writer_file_ID;
        char *cm_contact_string =
            malloc(strlen(writer_0_contact)); /* at least long enough */
        sscanf(writer_0_contact, "%p:%s", &writer_file_ID, cm_contact_string);
        //        printf("Writer contact info is fileID %p, contact info %s\n",
        //               writer_file_ID, cm_contact_string);

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

        CMwrite(conn, stream->CPInfo->ReaderRegisterFormat, &reader_register);
        /* wait for "go" from writer */
        CP_verbose(
            stream,
            "Waiting for writer response message in SstReadOpen(\"%s\")\n",
            name, reader_register.writer_response_condition);
        CMCondition_wait(stream->CPInfo->cm,
                         reader_register.writer_response_condition);
        CP_verbose(stream,
                   "finished wait writer response message in read_open\n");

        assert(response);
        struct _combined_writer_info writer_data;
        writer_data.writer_cohort_size = response->writer_cohort_size;
        writer_data.CP_writer_info = response->CP_writer_info;
        writer_data.DP_writer_info = response->DP_writer_info;
        return_data = distributeDataFromRankZero(
            stream, &writer_data, stream->CPInfo->CombinedWriterInfoFormat,
            &free_block);
    } else {
        return_data = distributeDataFromRankZero(
            stream, NULL, stream->CPInfo->CombinedWriterInfoFormat,
            &free_block);
    }
    //    printf("I am reader rank %d, my info on writers is:\n", stream->rank);
    //    FMdump_data(FMFormat_of_original(stream->CPInfo->combined_writer_format),
    //                return_data, 1024000);
    //    printf("\n");

    stream->connections_to_writer =
        calloc(sizeof(CP_peerConnection), return_data->writer_cohort_size);
    for (int i = 0; i < return_data->writer_cohort_size; i++) {
        attr_list attrs =
            attr_list_from_string(return_data->CP_writer_info[i]->contact_info);
        stream->connections_to_writer[i].contact_list = attrs;
        stream->connections_to_writer[i].remote_stream_ID =
            return_data->CP_writer_info[i]->writer_ID;
    }

    stream->peers = setupPeerArray(stream->cohort_size, stream->rank,
                                   return_data->writer_cohort_size);
    int i = 0;
    while (stream->peers[i] != -1) {
        int peer = stream->peers[i];
        stream->connections_to_writer[peer].CMconn =
            CMget_conn(stream->CPInfo->cm,
                       stream->connections_to_writer[peer].contact_list);
        i++;
    }

    stream->DP_Interface->provideWriterDataToReader(
        &Svcs, stream->DPstream, return_data->writer_cohort_size,
        stream->connections_to_writer, return_data->DP_writer_info);
    CP_verbose(stream, "Finish opening stream \"%s\"\n", name);
    return stream;
}

void queue_reader_register_msg_and_notify(SstStream stream,
                                          struct _reader_register_msg *req,
                                          CMConnection conn)
{
    pthread_mutex_lock(&stream->data_lock);
    request_queue new = malloc(sizeof(struct _request_queue));
    new->msg = req;
    new->conn = conn;
    new->next = NULL;
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

void queue_timestep_metadata_msg_and_notify(SstStream stream,
                                            struct _timestep_metadata_msg *tsm,
                                            CMConnection conn)
{
    pthread_mutex_lock(&stream->data_lock);
    struct _timestep_metadata_list *new = malloc(sizeof(struct _request_queue));
    new->MetadataMsg = tsm;
    new->Next = NULL;
    if (stream->timesteps) {
        struct _timestep_metadata_list *last = stream->timesteps;
        while (last->Next) {
            last = last->Next;
        }
        last->Next = new;
    } else {
        stream->timesteps = new;
    }
    pthread_cond_signal(&stream->data_condition);
    pthread_mutex_unlock(&stream->data_lock);
}

void CP_reader_register_handler(CManager cm, CMConnection conn, void *msg_v,
                                void *client_data, attr_list attrs)
{
    SstStream stream;
    struct _reader_register_msg *msg = (struct _reader_register_msg *)msg_v;
    //    fprintf(stderr,
    //            "Received a reader registration message directed at writer
    //            %p\n",
    //            msg->writer_file);
    //    fprintf(stderr, "A reader cohort of size %d is requesting to be
    //    added\n",
    //            msg->reader_cohort_size);
    //    for (int i = 0; i < msg->reader_cohort_size; i++) {
    //        fprintf(stderr, " rank %d CP contact info: %s, %d, %p\n", i,
    //                msg->CP_reader_info[i]->contact_info,
    //                msg->CP_reader_info[i]->target_stone,
    //                msg->CP_reader_info[i]->reader_ID);
    //    }
    stream = msg->writer_file;

    /* arrange for this message data to stay around */
    CMtake_buffer(cm, msg);

    queue_reader_register_msg_and_notify(stream, msg, conn);
}

void CP_timestep_metadata_handler(CManager cm, CMConnection conn, void *msg_v,
                                  void *client_data, attr_list attrs)
{
    SstStream stream;
    struct _timestep_metadata_msg *msg = (struct _timestep_metadata_msg *)msg_v;
    stream = (SstStream)msg->RS_stream;
    CP_verbose(stream,
               "Received an incoming metadata message for timestep %d\n",
               stream->rank, msg->timestep);

    /* arrange for this message data to stay around */
    CMtake_buffer(cm, msg);

    queue_timestep_metadata_msg_and_notify(stream, msg, conn);
}

void CP_writer_response_handler(CManager cm, CMConnection conn, void *msg_v,
                                void *client_data, attr_list attrs)
{
    struct _writer_response_msg *msg = (struct _writer_response_msg *)msg_v;
    struct _writer_response_msg **response_ptr;
    //    fprintf(stderr, "Received a writer_response message for condition
    //    %d\n",
    //            msg->writer_response_condition);
    //    fprintf(stderr, "The responding writer has cohort of size %d :\n",
    //            msg->writer_cohort_size);
    //    for (int i = 0; i < msg->writer_cohort_size; i++) {
    //        fprintf(stderr, " rank %d CP contact info: %s, %p\n", i,
    //                msg->CP_writer_info[i]->contact_info,
    //                msg->CP_writer_info[i]->writer_ID);
    //    }

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

extern void CP_ReleaseTimestepHandler(CManager cm, CMConnection conn,
                                      void *msg_v, void *client_data,
                                      attr_list attrs)
{
    struct _ReleaseTimestepMsg *msg = (struct _ReleaseTimestepMsg *)msg_v;
    WS_reader_info reader = (WS_reader_info)msg->WSR_Stream;
    SstStream stream = reader->parent_stream;
    CP_verbose(stream, "Received a release timestep message "
                       "for timestep %d\n",
               msg->Timestep);

    /*
     * This needs reconsideration for multiple readers.  Currently we do
     * provideTimestep once for the "parent" stream.  We call data plane
     * releaseTimestep whenever we get any release from any reader.  This is
     * fine while it's one-to-one.  But if not, someone needs to be keeping
     * track.  Perhaps with reference counts, but still handling the failure
     * situation where knowing how to adjust the reference count is hard.
     * Left for later at the moment.
     */
    stream->DP_Interface->releaseTimestep(
        &Svcs, reader->parent_stream->DPstream, msg->Timestep);
}

extern SstFullMetadata SstGetMetadata(SstStream stream, long timestep)
{
    struct _timestep_metadata_list *next;
    SstFullMetadata ret;
    pthread_mutex_lock(&stream->data_lock);
    next = stream->timesteps;
    while (1) {
        next = stream->timesteps;
        while (next) {
            if (next->MetadataMsg->timestep == timestep) {
                ret = malloc(sizeof(struct _SstFullMetadata));
                ret->WriterCohortSize = next->MetadataMsg->cohort_size;
                ret->WriterMetadata = next->MetadataMsg->metadata;
                if (stream->DP_Interface->TimestepInfoFormats == NULL) {
                    // DP didn't provide struct info, no valid data
                    ret->DP_TimestepInfo = NULL;
                } else {
                    ret->DP_TimestepInfo = next->MetadataMsg->DP_TimestepInfo;
                }
                pthread_mutex_unlock(&stream->data_lock);
                return ret;
            }
            next = next->Next;
        }
        pthread_cond_wait(&stream->data_condition, &stream->data_lock);
    }
    /* NOTREACHED */
    pthread_mutex_unlock(&stream->data_lock);
}

extern void *SstReadRemoteMemory(SstStream stream, int rank, long timestep,
                                 size_t offset, size_t length, void *buffer,
                                 void *DP_TimestepInfo)
{
    return stream->DP_Interface->readRemoteMemory(&Svcs, stream->DPstream, rank,
                                                  timestep, offset, length,
                                                  buffer, DP_TimestepInfo);
}

void sendOneToEachWriterRank(SstStream s, CMFormat f, void *msg,
                             void **WS_stream_ptr)
{
    int i = 0;
    while (s->peers[i] != -1) {
        int peer = s->peers[i];
        CMConnection conn = s->connections_to_writer[peer].CMconn;
        /* add the writer stream identifier to each outgoing
         * message */
        *WS_stream_ptr = s->connections_to_writer[peer].remote_stream_ID;
        CMwrite(conn, f, msg);
        i++;
    }
}

extern void SstReleaseStep(SstStream stream, long Timestep)
{
    long MaxTimestep;
    struct _ReleaseTimestepMsg Msg;

    /*
     * remove local metadata for that timestep
     */
    pthread_mutex_lock(&stream->data_lock);
    struct _timestep_metadata_list *list = stream->timesteps;

    if (stream->timesteps->MetadataMsg->timestep == Timestep) {
        stream->timesteps = list->Next;
        free(list);
    } else {
        struct _timestep_metadata_list *last = list;
        list = list->Next;
        while (list != NULL) {
            if (list->MetadataMsg->timestep == Timestep) {
                last->Next = list->Next;
                free(list);
            }
            last = list;
            list = list->Next;
        }
    }
    pthread_mutex_unlock(&stream->data_lock);

    /*
     * this can be just a barrier (to ensure that everyone has called
     * SstReleaseStep), but doing a reduce and comparing the returned max to
     * our value will detect if someone is calling with a different timestep
     * value (which would be bad).  This is a relatively cheap upcost from
     * simple barrier in return for robustness checking.
     */
    MPI_Allreduce(&Timestep, &MaxTimestep, 1, MPI_LONG, MPI_MAX,
                  stream->mpiComm);
    assert((Timestep == MaxTimestep) && "All ranks must be in sync.  Someone "
                                        "called SstReleaseTimestep with a "
                                        "different timestep value");

    Msg.Timestep = Timestep;

    /*
     * send each writer rank a release for this timestep (actually goes to WSR
     * streams)
     */
    CP_verbose(
        stream,
        "Sending ReleaseTimestep message for timestep %d, one to each writer\n",
        Timestep);
    sendOneToEachWriterRank(stream, stream->CPInfo->ReleaseTimestepFormat, &Msg,
                            &Msg.WSR_Stream);
}

extern void SstAdvanceStep(SstStream stream, long Timestep) {}

extern void SstReaderClose(SstStream stream) {}

extern void SstWaitForCompletion(SstStream stream, void *handle)
{
    return stream->DP_Interface->waitForCompletion(&Svcs, handle);
}

static void DP_verbose(SstStream s, char *format, ...)
{
    if (s->verbose) {
        va_list args;
        va_start(args, format);
        if (s->Role == ReaderRole) {
            fprintf(stderr, "DP Reader %d (%p): ", s->rank, s);
        } else {
            fprintf(stderr, "DP Writer %d (%p): ", s->rank, s);
        }
        vfprintf(stderr, format, args);
        va_end(args);
    }
}
extern void CP_verbose(SstStream s, char *format, ...)
{
    if (s->verbose) {
        va_list args;
        va_start(args, format);
        if (s->Role == ReaderRole) {
            fprintf(stderr, "Reader %d (%p): ", s->rank, s);
        } else {
            fprintf(stderr, "Writer %d (%p): ", s->rank, s);
        }
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static CManager CP_getCManager(SstStream stream) { return stream->CPInfo->cm; }

static int CP_myRank(SstStream stream) { return stream->rank; }

static void CP_sendToPeer(SstStream s, CP_PeerCohort cohort, int rank,
                          CMFormat format, void *data)
{
    CP_peerConnection *peers = (CP_peerConnection *)cohort;
    if (peers[rank].CMconn == NULL) {
        peers[rank].CMconn =
            CMget_conn(s->CPInfo->cm, peers[rank].contact_list);
    }
    CMwrite(peers[rank].CMconn, format, data);
}
