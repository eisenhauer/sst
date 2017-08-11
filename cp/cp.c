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
static MPI_Comm CP_getMPIComm(SstStream stream);

struct _CP_Services Svcs = {
    (CP_VerboseFunc)DP_verbose, (CP_GetCManagerFunc)CP_getCManager,
    (CP_SendToPeerFunc)CP_sendToPeer, (CP_GetMPICommFunc)CP_getMPIComm};

static void sendOneToEachWriterRank(SstStream s, CMFormat f, void *msg,
                                    void **WS_stream_ptr);
static void writeContactInfo(char *name, SstStream stream)
{
    char *contact = attr_list_to_string(CMget_contact_list(stream->CPInfo->cm));
    char *tmp_name = malloc(strlen(name) + strlen(".tmp") + 1);
    char *file_name = malloc(strlen(name) + strlen(".bpflx") + 1);
    FILE *WriterInfo;

    sprintf(tmp_name, "%s.tmp", name);
    sprintf(file_name, "%s.bpflx", name);
    WriterInfo = fopen(tmp_name, "w");
    fprintf(WriterInfo, "%p:%s", (void *)stream, contact);
    fclose(WriterInfo);
    rename(tmp_name, file_name);
}

static char *readContactInfo(char *name, SstStream stream)
{
    char *file_name = malloc(strlen(name) + strlen(".bpflx") + 1);
    FILE *WriterInfo;
    sprintf(file_name, "%s.bpflx", name);
//    printf("Looking for writer contact in file %s\n", file_name);
redo:
    WriterInfo = fopen(file_name, "r");
    while (!WriterInfo) {
        CMusleep(stream->CPInfo->cm, 500);
        WriterInfo = fopen(file_name, "r");
    }
    struct stat buf;
    fstat(fileno(WriterInfo), &buf);
    int size = buf.st_size;
    if (size == 0) {
        //        printf("Size of writer contact file is zero, but it shouldn't
        //        be! "
        //               "Retrying!\n");
        goto redo;
    }

    char *buffer = calloc(1, size + 1);
    (void)fread(buffer, size, 1, WriterInfo);
    fclose(WriterInfo);
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

static void initWSReader(WS_ReaderInfo reader, int ReaderSize,
                         CP_ReaderInitInfo *reader_info)
{
    int writer_size = reader->ParentStream->CohortSize;
    int writer_rank = reader->ParentStream->Rank;
    int i;
    reader->ReaderCohortSize = ReaderSize;
    reader->Connections = calloc(sizeof(reader->Connections[0]), ReaderSize);
    for (i = 0; i < ReaderSize; i++) {
        reader->Connections[i].ContactList =
            attr_list_from_string(reader_info[i]->ContactInfo);
        reader->Connections[i].RemoteStreamID = reader_info[i]->ReaderID;
        reader->Connections[i].CMconn = NULL;
    }
    reader->Peers = setupPeerArray(writer_size, writer_rank, ReaderSize);
    i = 0;
    while (reader->Peers[i] != -1) {
        int peer = reader->Peers[i];
        reader->Connections[peer].CMconn =
            CMget_conn(reader->ParentStream->CPInfo->cm,
                       reader->Connections[peer].ContactList);
        i++;
    }
}

WS_ReaderInfo writer_participate_in_reader_open(SstStream stream)
{
    RequestQueue req;
    reader_data_t return_data;
    void *free_block = NULL;
    int WriterResponseCondition = -1;
    CMConnection conn;
    CP_verbose(stream, "Beginning writer-side reader open protocol\n");
    if (stream->Rank == 0) {
        pthread_mutex_lock(&stream->DataLock);
        assert((stream->ReadRequestQueue));
        req = stream->ReadRequestQueue;
        stream->ReadRequestQueue = req->Next;
        req->Next = NULL;
        pthread_mutex_unlock(&stream->DataLock);
        struct _CombinedReaderInfo reader_data;
        reader_data.ReaderCohortSize = req->Msg->ReaderCohortSize;
        reader_data.CP_ReaderInfo = req->Msg->CP_ReaderInfo;
        reader_data.DP_ReaderInfo = req->Msg->DP_ReaderInfo;
        return_data = CP_distributeDataFromRankZero(
            stream, &reader_data, stream->CPInfo->CombinedReaderInfoFormat,
            &free_block);
        WriterResponseCondition = req->Msg->WriterResponseCondition;
        conn = req->Conn;
        CMreturn_buffer(stream->CPInfo->cm, req->Msg);
        free(req);
    } else {
        return_data = CP_distributeDataFromRankZero(
            stream, NULL, stream->CPInfo->CombinedReaderInfoFormat,
            &free_block);
    }
    //    printf("I am writer rank %d, my info on readers is:\n", stream->Rank);
    //    FMdump_data(FMFormat_of_original(stream->CPInfo->combined_reader_format),
    //                return_data, 1024000);
    //    printf("\n");

    stream->Readers = realloc(stream->Readers, sizeof(stream->Readers[0]) *
                                                   (stream->ReaderCount + 1));
    DP_WSR_Stream per_reader_stream;
    void *DP_WriterInfo;
    void *ret_data_block;
    CP_PeerConnection *connections_to_reader;
    connections_to_reader =
        calloc(sizeof(CP_PeerConnection), return_data->ReaderCohortSize);
    for (int i = 0; i < return_data->ReaderCohortSize; i++) {
        attr_list attrs =
            attr_list_from_string(return_data->CP_ReaderInfo[i]->ContactInfo);
        connections_to_reader[i].ContactList = attrs;
        connections_to_reader[i].RemoteStreamID =
            return_data->CP_ReaderInfo[i]->ReaderID;
    }

    per_reader_stream = stream->DP_Interface->initWriterPerReader(
        &Svcs, stream->DP_Stream, return_data->ReaderCohortSize,
        connections_to_reader, return_data->DP_ReaderInfo, &DP_WriterInfo);

    WS_ReaderInfo CP_WSR_Stream = malloc(sizeof(*CP_WSR_Stream));
    stream->Readers[stream->ReaderCount] = CP_WSR_Stream;
    CP_WSR_Stream->DP_WSR_Stream = per_reader_stream;
    CP_WSR_Stream->ParentStream = stream;
    CP_WSR_Stream->Connections = connections_to_reader;
    initWSReader(CP_WSR_Stream, return_data->ReaderCohortSize,
                 return_data->CP_ReaderInfo);

    stream->ReaderCount++;

    struct _CP_DP_PairInfo combined_init;
    struct _CP_WriterInitInfo cpInfo;

    struct _CP_DP_PairInfo **pointers = NULL;

    cpInfo.ContactInfo =
        attr_list_to_string(CMget_contact_list(stream->CPInfo->cm));
    cpInfo.WriterID = CP_WSR_Stream;

    combined_init.CP_Info = (void **)&cpInfo;
    combined_init.DP_Info = DP_WriterInfo;

    pointers = (struct _CP_DP_PairInfo **)CP_consolidateDataToRankZero(
        stream, &combined_init, stream->CPInfo->PerRankWriterInfoFormat,
        &ret_data_block);

    if (stream->Rank == 0) {
        struct _WriterResponseMsg response;
        memset(&response, 0, sizeof(response));
        response.WriterResponseCondition = WriterResponseCondition;
        response.WriterCohortSize = stream->CohortSize;
        response.CP_WriterInfo =
            malloc(response.WriterCohortSize * sizeof(void *));
        response.DP_WriterInfo =
            malloc(response.WriterCohortSize * sizeof(void *));
        for (int i = 0; i < response.WriterCohortSize; i++) {
            response.CP_WriterInfo[i] =
                (struct _CP_WriterInitInfo *)pointers[i]->CP_Info;
            response.DP_WriterInfo[i] = pointers[i]->DP_Info;
        }
        CMwrite(conn, stream->CPInfo->WriterResponseFormat, &response);
    }
    CP_verbose(stream, "Finish writer-side reader open protocol for reader %p, "
                       "reader ready response pending\n",
               CP_WSR_Stream);
    return CP_WSR_Stream;
}

static void waitForReaderResponse(WS_ReaderInfo Reader)
{
    SstStream Stream = Reader->ParentStream;
    pthread_mutex_lock(&Stream->DataLock);
    while (Reader->ReaderStatus != Established) {
        CP_verbose(Stream, "Waiting for Reader ready on WSR %p.\n", Reader);
        pthread_cond_wait(&Stream->DataCondition, &Stream->DataLock);
    }
    assert(Reader->ReaderStatus == Established);
    pthread_mutex_unlock(&Stream->DataLock);
    CP_verbose(Stream, "Reader ready on WSR %p, stream established.\n", Reader);
}

SstStream SstWriterOpen(char *name, char *params, MPI_Comm comm)
{
    SstStream stream;

    stream = CP_newStream();
    stream->Role = WriterRole;
    CP_parseParams(stream, params);

    stream->DP_Interface = LoadDP("dummy");

    stream->CPInfo = CP_getCPInfo(stream->DP_Interface);

    stream->mpiComm = comm;
    if (stream->WaitForFirstReader) {
        stream->FirstReaderCondition =
            CMCondition_get(stream->CPInfo->cm, NULL);
    } else {
        stream->FirstReaderCondition = -1;
    }

    MPI_Comm_rank(stream->mpiComm, &stream->Rank);
    MPI_Comm_size(stream->mpiComm, &stream->CohortSize);

    stream->DP_Stream = stream->DP_Interface->initWriter(&Svcs, stream);

    if (stream->Rank == 0) {
        writeContactInfo(name, stream);
    }

    CP_verbose(stream, "Opening stream \"%s\"\n", name);

    if (stream->WaitForFirstReader) {
        WS_ReaderInfo reader;
        CP_verbose(
            stream,
            "Stream parameter requires rendezvous, waiting for first reader\n");
        if (stream->Rank == 0) {
            pthread_mutex_lock(&stream->DataLock);
            if (stream->ReadRequestQueue == NULL) {
                pthread_cond_wait(&stream->DataCondition, &stream->DataLock);
            }
            assert(stream->ReadRequestQueue);
            pthread_mutex_unlock(&stream->DataLock);
        }
        MPI_Barrier(stream->mpiComm);

        reader = writer_participate_in_reader_open(stream);
        waitForReaderResponse(reader);
        MPI_Barrier(stream->mpiComm);
    }
    CP_verbose(stream, "Finish opening stream \"%s\"\n", name);
    return stream;
}

void sendOneToEachReaderRank(SstStream s, CMFormat f, void *msg,
                             void **RS_stream_ptr)
{
    for (int i = 0; i < s->ReaderCount; i++) {
        int j = 0;
        WS_ReaderInfo CP_WSR_Stream = s->Readers[i];
        while (CP_WSR_Stream->Peers[j] != -1) {
            int peer = CP_WSR_Stream->Peers[j];
            CMConnection conn = CP_WSR_Stream->Connections[peer].CMconn;
            /* add the reader-rank-specific stream identifier to each outgoing
             * message */
            *RS_stream_ptr = CP_WSR_Stream->Connections[peer].RemoteStreamID;
            CMwrite(conn, f, msg);
            j++;
        }
    }
}

void SstWriterClose(SstStream Stream)
{
    struct _WriterCloseMsg Msg;
    Msg.FinalTimestep = Stream->LastProvidedTimestep;
    sendOneToEachReaderRank(Stream, Stream->CPInfo->WriterCloseFormat, &Msg,
                            &Msg.RS_Stream);

    /* wait until all queued data is sent */
    CP_verbose(Stream, "Checking for queued timesteps in WriterClose\n");
    pthread_mutex_lock(&Stream->DataLock);
    while (Stream->QueuedTimesteps) {
        CP_verbose(Stream,
                   "Waiting for timesteps to be released in WriterClose\n");
        pthread_cond_wait(&Stream->DataCondition, &Stream->DataLock);
    }
    pthread_mutex_unlock(&Stream->DataLock);
    CP_verbose(Stream, "All timesteps are released in WriterClose\n");
}

void SstProvideTimestep(SstStream s, SstMetadata LocalMetadata, SstData Data,
                        long Timestep)
{
    void *data_block;
    MetadataPlusDPInfo *pointers;
    struct _TimestepMetadataMsg Msg;
    void *DP_TimestepInfo = NULL;
    struct _MetadataPlusDPInfo Md;
    CPTimestepList Entry = malloc(sizeof(struct _CPTimestepEntry));

    s->DP_Interface->provideTimestep(&Svcs, s->DP_Stream, Data, LocalMetadata,
                                     Timestep, &DP_TimestepInfo);

    Md.Metadata = LocalMetadata;
    Md.DP_TimestepInfo = DP_TimestepInfo;

    pointers = (MetadataPlusDPInfo *)CP_consolidateDataToAll(
        s, &Md, s->CPInfo->PerRankMetadataFormat, &data_block);

    Msg.CohortSize = s->CohortSize;
    Msg.Timestep = s->WriterTimestep++;

    /* separate metadata and DP_info to separate arrays */
    Msg.Metadata = malloc(s->CohortSize * sizeof(void *));
    Msg.DP_TimestepInfo = malloc(s->CohortSize * sizeof(void *));
    int NullCount = 0;
    for (int i = 0; i < s->CohortSize; i++) {
        Msg.Metadata[i] = pointers[i]->Metadata;
        Msg.DP_TimestepInfo[i] = pointers[i]->DP_TimestepInfo;
        if (pointers[i]->DP_TimestepInfo == NULL)
            NullCount++;
    }
    if (NullCount == s->CohortSize) {
        free(Msg.DP_TimestepInfo);
        Msg.DP_TimestepInfo = NULL;
    }

    CP_verbose(s,
               "Sending TimestepMetadata for timestep %d, one to each reader\n",
               Timestep);

    /*
     * lock this stream's data and queue the timestep
     */
    pthread_mutex_lock(&s->DataLock);
    s->LastProvidedTimestep = Timestep;
    Entry->Data = Data;
    Entry->Timestep = Timestep;
    Entry->MetadataArray = Msg.Metadata;
    Entry->DP_TimestepInfo = Msg.DP_TimestepInfo;
    Entry->Next = s->QueuedTimesteps;
    s->QueuedTimesteps = Entry;
    s->QueuedTimestepCount++;
    /* no one waits on timesteps being added, so no condition signal to note
     * change */
    pthread_mutex_unlock(&s->DataLock);

    sendOneToEachReaderRank(s, s->CPInfo->DeliverTimestepMetadataFormat, &Msg,
                            &Msg.RS_stream);
}

static void **participate_in_reader_init_data_exchange(SstStream stream,
                                                       void *dpInfo,
                                                       void **ret_data_block)
{

    struct _CP_DP_PairInfo combined_init;
    struct _CP_ReaderInitInfo cpInfo;

    struct _CP_DP_PairInfo **pointers = NULL;

    cpInfo.ContactInfo =
        attr_list_to_string(CMget_contact_list(stream->CPInfo->cm));
    cpInfo.ReaderID = stream;

    combined_init.CP_Info = (void **)&cpInfo;
    combined_init.DP_Info = dpInfo;

    pointers = (struct _CP_DP_PairInfo **)CP_consolidateDataToRankZero(
        stream, &combined_init, stream->CPInfo->PerRankReaderInfoFormat,
        ret_data_block);
    return (void **)pointers;
}

SstStream SstReaderOpen(char *name, char *params, MPI_Comm comm)
{
    SstStream Stream;
    void *dpInfo;
    struct _CP_DP_PairInfo **pointers;
    void *data_block;
    void *free_block;
    writer_data_t return_data;
    struct _ReaderActivateMsg Msg;

    Stream = CP_newStream();
    Stream->Role = ReaderRole;

    CP_parseParams(Stream, params);

    Stream->DP_Interface = LoadDP("dummy");

    Stream->CPInfo = CP_getCPInfo(Stream->DP_Interface);

    Stream->mpiComm = comm;

    MPI_Comm_rank(Stream->mpiComm, &Stream->Rank);
    MPI_Comm_size(Stream->mpiComm, &Stream->CohortSize);

    Stream->DP_Stream =
        Stream->DP_Interface->initReader(&Svcs, Stream, &dpInfo);

    pointers =
        (struct _CP_DP_PairInfo **)participate_in_reader_init_data_exchange(
            Stream, dpInfo, &data_block);

    if (Stream->Rank == 0) {
        char *writer_0_contact = readContactInfo(name, Stream);
        void *writer_file_ID;
        char *cm_contact_string =
            malloc(strlen(writer_0_contact)); /* at least long enough */
        sscanf(writer_0_contact, "%p:%s", &writer_file_ID, cm_contact_string);
        //        printf("Writer contact info is fileID %p, contact info %s\n",
        //               writer_file_ID, cm_contact_string);

        attr_list writer_rank0_contact =
            attr_list_from_string(cm_contact_string);
        CMConnection conn =
            CMget_conn(Stream->CPInfo->cm, writer_rank0_contact);
        struct _ReaderRegisterMsg reader_register;

        reader_register.WriterFile = writer_file_ID;
        reader_register.WriterResponseCondition =
            CMCondition_get(Stream->CPInfo->cm, conn);
        reader_register.ReaderCohortSize = Stream->CohortSize;
        reader_register.CP_ReaderInfo =
            malloc(reader_register.ReaderCohortSize * sizeof(void *));
        reader_register.DP_ReaderInfo =
            malloc(reader_register.ReaderCohortSize * sizeof(void *));
        for (int i = 0; i < reader_register.ReaderCohortSize; i++) {
            reader_register.CP_ReaderInfo[i] =
                (CP_ReaderInitInfo)pointers[i]->CP_Info;
            reader_register.DP_ReaderInfo[i] = pointers[i]->DP_Info;
        }
        /* the response value is set in the handler */
        struct _WriterResponseMsg *response = NULL;
        CMCondition_set_client_data(Stream->CPInfo->cm,
                                    reader_register.WriterResponseCondition,
                                    &response);

        CMwrite(conn, Stream->CPInfo->ReaderRegisterFormat, &reader_register);
        /* wait for "go" from writer */
        CP_verbose(
            Stream,
            "Waiting for writer response message in SstReadOpen(\"%s\")\n",
            name, reader_register.WriterResponseCondition);
        CMCondition_wait(Stream->CPInfo->cm,
                         reader_register.WriterResponseCondition);
        CP_verbose(Stream,
                   "finished wait writer response message in read_open\n");

        assert(response);
        struct _CombinedWriterInfo writer_data;
        writer_data.WriterCohortSize = response->WriterCohortSize;
        writer_data.CP_WriterInfo = response->CP_WriterInfo;
        writer_data.DP_WriterInfo = response->DP_WriterInfo;
        return_data = CP_distributeDataFromRankZero(
            Stream, &writer_data, Stream->CPInfo->CombinedWriterInfoFormat,
            &free_block);
    } else {
        return_data = CP_distributeDataFromRankZero(
            Stream, NULL, Stream->CPInfo->CombinedWriterInfoFormat,
            &free_block);
    }
    //    printf("I am reader rank %d, my info on writers is:\n", Stream->Rank);
    //    FMdump_data(FMFormat_of_original(Stream->CPInfo->combined_writer_format),
    //                return_data, 1024000);
    //    printf("\n");

    Stream->ConnectionsToWriter =
        calloc(sizeof(CP_PeerConnection), return_data->WriterCohortSize);
    for (int i = 0; i < return_data->WriterCohortSize; i++) {
        attr_list attrs =
            attr_list_from_string(return_data->CP_WriterInfo[i]->ContactInfo);
        Stream->ConnectionsToWriter[i].ContactList = attrs;
        Stream->ConnectionsToWriter[i].RemoteStreamID =
            return_data->CP_WriterInfo[i]->WriterID;
    }

    Stream->Peers = setupPeerArray(Stream->CohortSize, Stream->Rank,
                                   return_data->WriterCohortSize);
    int i = 0;
    while (Stream->Peers[i] != -1) {
        int peer = Stream->Peers[i];
        Stream->ConnectionsToWriter[peer].CMconn = CMget_conn(
            Stream->CPInfo->cm, Stream->ConnectionsToWriter[peer].ContactList);
        i++;
    }

    Stream->DP_Interface->provideWriterDataToReader(
        &Svcs, Stream->DP_Stream, return_data->WriterCohortSize,
        Stream->ConnectionsToWriter, return_data->DP_WriterInfo);
    pthread_mutex_lock(&Stream->DataLock);
    Stream->Status = Established;
    pthread_mutex_unlock(&Stream->DataLock);
    CP_verbose(Stream, "Sending Reader Activate messages to writer\n");
    sendOneToEachWriterRank(Stream, Stream->CPInfo->ReaderActivateFormat, &Msg,
                            &Msg.WSR_Stream);
    CP_verbose(Stream, "Finish opening stream \"%s\"\n", name);
    return Stream;
}

void queueReaderRegisterMsgAndNotify(SstStream stream,
                                     struct _ReaderRegisterMsg *req,
                                     CMConnection conn)
{
    pthread_mutex_lock(&stream->DataLock);
    RequestQueue new = malloc(sizeof(struct _RequestQueue));
    new->Msg = req;
    new->Conn = conn;
    new->Next = NULL;
    if (stream->ReadRequestQueue) {
        RequestQueue last = stream->ReadRequestQueue;
        while (last->Next) {
            last = last->Next;
        }
        last->Next = new;
    } else {
        stream->ReadRequestQueue = new;
    }
    pthread_cond_signal(&stream->DataCondition);
    pthread_mutex_unlock(&stream->DataLock);
}

void queueTimestepMetadataMsgAndNotify(SstStream stream,
                                       struct _TimestepMetadataMsg *tsm,
                                       CMConnection conn)
{
    pthread_mutex_lock(&stream->DataLock);
    struct _TimestepMetadataList *new = malloc(sizeof(struct _RequestQueue));
    new->MetadataMsg = tsm;
    new->Next = NULL;
    if (stream->Timesteps) {
        struct _TimestepMetadataList *last = stream->Timesteps;
        while (last->Next) {
            last = last->Next;
        }
        last->Next = new;
    } else {
        stream->Timesteps = new;
    }
    pthread_cond_signal(&stream->DataCondition);
    pthread_mutex_unlock(&stream->DataLock);
}

void CP_ReaderRegisterHandler(CManager cm, CMConnection conn, void *msg_v,
                              void *client_data, attr_list attrs)
{
    SstStream stream;
    struct _ReaderRegisterMsg *msg = (struct _ReaderRegisterMsg *)msg_v;
    //    fprintf(stderr,
    //            "Received a reader registration message directed at writer
    //            %p\n",
    //            msg->writer_file);
    //    fprintf(stderr, "A reader cohort of size %d is requesting to be
    //    added\n",
    //            msg->ReaderCohortSize);
    //    for (int i = 0; i < msg->ReaderCohortSize; i++) {
    //        fprintf(stderr, " rank %d CP contact info: %s, %d, %p\n", i,
    //                msg->CP_ReaderInfo[i]->ContactInfo,
    //                msg->CP_ReaderInfo[i]->target_stone,
    //                msg->CP_ReaderInfo[i]->ReaderID);
    //    }
    stream = msg->WriterFile;

    /* arrange for this message data to stay around */
    CMtake_buffer(cm, msg);

    queueReaderRegisterMsgAndNotify(stream, msg, conn);
}

void CP_ReaderActivateHandler(CManager cm, CMConnection conn, void *msg_v,
                              void *client_data, attr_list attrs)
{
    SstStream stream;
    struct _ReaderActivateMsg *Msg = (struct _ReaderActivateMsg *)msg_v;

    WS_ReaderInfo CP_WSR_Stream = Msg->WSR_Stream;
    CP_verbose(CP_WSR_Stream->ParentStream, "Reader Activate message received "
                                            "for stream %p.  Setting state to "
                                            "Established.\n",
               CP_WSR_Stream);
    pthread_mutex_lock(&CP_WSR_Stream->ParentStream->DataLock);
    CP_WSR_Stream->ReaderStatus = Established;
    /*
     * the main thread might be waiting for this
     */
    pthread_cond_signal(&CP_WSR_Stream->ParentStream->DataCondition);
    pthread_mutex_unlock(&CP_WSR_Stream->ParentStream->DataLock);
}

void CP_TimestepMetadataHandler(CManager cm, CMConnection conn, void *msg_v,
                                void *client_data, attr_list attrs)
{
    SstStream stream;
    struct _TimestepMetadataMsg *msg = (struct _TimestepMetadataMsg *)msg_v;
    stream = (SstStream)msg->RS_stream;
    CP_verbose(stream,
               "Received an incoming metadata message for timestep %d\n",
               stream->Rank, msg->Timestep);

    /* arrange for this message data to stay around */
    CMtake_buffer(cm, msg);

    queueTimestepMetadataMsgAndNotify(stream, msg, conn);
}

void CP_WriterResponseHandler(CManager cm, CMConnection conn, void *msg_v,
                              void *client_data, attr_list attrs)
{
    struct _WriterResponseMsg *msg = (struct _WriterResponseMsg *)msg_v;
    struct _WriterResponseMsg **response_ptr;
    //    fprintf(stderr, "Received a writer_response message for condition
    //    %d\n",
    //            msg->WriterResponseCondition);
    //    fprintf(stderr, "The responding writer has cohort of size %d :\n",
    //            msg->writer_CohortSize);
    //    for (int i = 0; i < msg->writer_CohortSize; i++) {
    //        fprintf(stderr, " rank %d CP contact info: %s, %p\n", i,
    //                msg->CP_WriterInfo[i]->ContactInfo,
    //                msg->CP_WriterInfo[i]->WriterID);
    //    }

    /* arrange for this message data to stay around */
    CMtake_buffer(cm, msg);

    /* attach the message to the CMCondition so it an be retrieved by the main
     * thread */
    response_ptr =
        CMCondition_get_client_data(cm, msg->WriterResponseCondition);
    *response_ptr = msg;

    /* wake the main thread */
    CMCondition_signal(cm, msg->WriterResponseCondition);
}

extern CPTimestepList dequeueTimestep(SstStream Stream, long Timestep)
{
    CPTimestepList Ret = NULL;
    CPTimestepList List = NULL;
    pthread_mutex_lock(&Stream->DataLock);
    List = Stream->QueuedTimesteps;
    if (Stream->QueuedTimesteps->Timestep == Timestep) {
        Stream->QueuedTimesteps = List->Next;
        Ret = List;
    } else {
        CPTimestepList Last = List;
        List = List->Next;
        while (List != NULL) {
            if (List->Timestep == Timestep) {
                Last->Next = List->Next;
                Ret = List;
            }
            Last = List;
            List = List->Next;
        }
        if (Ret == NULL) {
            /*
             * Shouldn't ever get here because we should never dequeue a
             * timestep that we don't have.
             */
            fprintf(stderr, "Failed to dequeue Timestep %ld, not found\n",
                    Timestep);
            assert(0);
        }
    }
    Stream->QueuedTimestepCount--;
    /* main thread might be waiting on timesteps going away */
    pthread_cond_signal(&Stream->DataCondition);
    pthread_mutex_unlock(&Stream->DataLock);
    return NULL;
}

extern void CP_ReleaseTimestepHandler(CManager cm, CMConnection conn,
                                      void *msg_v, void *client_data,
                                      attr_list attrs)
{
    struct _ReleaseTimestepMsg *msg = (struct _ReleaseTimestepMsg *)msg_v;
    WS_ReaderInfo reader = (WS_ReaderInfo)msg->WSR_Stream;
    SstStream stream = reader->ParentStream;
    CPTimestepList Entry = NULL;

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
        &Svcs, reader->ParentStream->DP_Stream, msg->Timestep);

    Entry = dequeueTimestep(reader->ParentStream, msg->Timestep);
    free(Entry);
}

extern void CP_WriterCloseHandler(CManager cm, CMConnection conn, void *msg_v,
                                  void *client_data, attr_list attrs)
{
    WriterCloseMsg msg = (WriterCloseMsg)msg_v;
    SstStream Stream = (SstStream)msg->RS_Stream;
    CPTimestepList Entry = NULL;

    CP_verbose(Stream, "Received a writer close message. "
                       "Timestep %d was the final timestep.\n",
               msg->FinalTimestep);

    pthread_mutex_lock(&Stream->DataLock);
    Stream->Status = PeerClosed;
    /* wake anyone that might be waiting */
    pthread_cond_signal(&Stream->DataCondition);
    pthread_mutex_unlock(&Stream->DataLock);
}

static TSMetadataList waitForMetadata(SstStream stream, long timestep)
{
    struct _TimestepMetadataList *next;
    SstFullMetadata ret;
    pthread_mutex_lock(&stream->DataLock);
    next = stream->Timesteps;
    while (1) {
        next = stream->Timesteps;
        while (next) {
            if (next->MetadataMsg->Timestep == timestep) {
                pthread_mutex_unlock(&stream->DataLock);
                return next;
            }
            next = next->Next;
        }
        /* didn't find requested timestep, check stream status */
        if (stream->Status != Established) {
            /* closed or failed, return NULL */
            return NULL;
        }
        /* wait until we get the timestep metadata or something else changes */
        pthread_cond_wait(&stream->DataCondition, &stream->DataLock);
    }
    /* NOTREACHED */
    pthread_mutex_unlock(&stream->DataLock);
}

extern SstFullMetadata SstGetMetadata(SstStream stream, long timestep)
{
    TSMetadataList Entry;
    SstFullMetadata ret;
    Entry = waitForMetadata(stream, timestep);
    if (Entry) {
        ret = malloc(sizeof(struct _SstFullMetadata));
        ret->WriterCohortSize = Entry->MetadataMsg->CohortSize;
        ret->WriterMetadata = Entry->MetadataMsg->Metadata;
        if (stream->DP_Interface->TimestepInfoFormats == NULL) {
            // DP didn't provide struct info, no valid data
            ret->DP_TimestepInfo = NULL;
        } else {
            ret->DP_TimestepInfo = Entry->MetadataMsg->DP_TimestepInfo;
        }
        stream->CurrentWorkingTimestep = timestep;
        return ret;
    }
    assert(stream->Status != Established);
    return NULL;
}

extern void *SstReadRemoteMemory(SstStream stream, int rank, long timestep,
                                 size_t offset, size_t length, void *buffer,
                                 void *DP_TimestepInfo)
{
    return stream->DP_Interface->readRemoteMemory(
        &Svcs, stream->DP_Stream, rank, timestep, offset, length, buffer,
        DP_TimestepInfo);
}

void sendOneToEachWriterRank(SstStream s, CMFormat f, void *msg,
                             void **WS_stream_ptr)
{
    int i = 0;
    while (s->Peers[i] != -1) {
        int peer = s->Peers[i];
        CMConnection conn = s->ConnectionsToWriter[peer].CMconn;
        /* add the writer stream identifier to each outgoing
         * message */
        *WS_stream_ptr = s->ConnectionsToWriter[peer].RemoteStreamID;
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
    pthread_mutex_lock(&stream->DataLock);
    struct _TimestepMetadataList *list = stream->Timesteps;

    if (stream->Timesteps->MetadataMsg->Timestep == Timestep) {
        stream->Timesteps = list->Next;
        free(list);
    } else {
        struct _TimestepMetadataList *last = list;
        list = list->Next;
        while (list != NULL) {
            if (list->MetadataMsg->Timestep == Timestep) {
                last->Next = list->Next;
                free(list);
            }
            last = list;
            list = list->Next;
        }
    }
    pthread_mutex_unlock(&stream->DataLock);

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

/*
 * wait for metadata for Timestep indicated to arrive, or fail with EndOfStream
 * or Error
 */
extern SstStatusValue SstAdvanceStep(SstStream stream, long Timestep)
{

    stream->CurrentWorkingTimestep = Timestep;

    TSMetadataList Entry;
    Entry = waitForMetadata(stream, Timestep);
    if (Entry) {
        stream->CurrentWorkingTimestep = Timestep;
        return SstSuccess;
    }
    if (stream->Status == PeerClosed) {
        return SstEndOfStream;
    } else {
        return SstFatalError;
    }
}

extern void SstReaderClose(SstStream Stream)
{
    /* need to have a reader-side shutdown protocol, but for now, just sleep for
     * a little while to makes sure our release message for the last timestep
     * got received */
    CMsleep(Stream->CPInfo->cm, 1);
}

extern SstStatusValue SstWaitForCompletion(SstStream stream, void *handle)
{
    //   We need a way to return an error from DP */
    stream->DP_Interface->waitForCompletion(&Svcs, handle);
    return SstSuccess;
}

static void DP_verbose(SstStream s, char *format, ...)
{
    if (s->Verbose) {
        va_list args;
        va_start(args, format);
        if (s->Role == ReaderRole) {
            fprintf(stderr, "DP Reader %d (%p): ", s->Rank, s);
        } else {
            fprintf(stderr, "DP Writer %d (%p): ", s->Rank, s);
        }
        vfprintf(stderr, format, args);
        va_end(args);
    }
}
extern void CP_verbose(SstStream s, char *format, ...)
{
    if (s->Verbose) {
        va_list args;
        va_start(args, format);
        if (s->Role == ReaderRole) {
            fprintf(stderr, "Reader %d (%p): ", s->Rank, s);
        } else {
            fprintf(stderr, "Writer %d (%p): ", s->Rank, s);
        }
        vfprintf(stderr, format, args);
        va_end(args);
    }
}

static CManager CP_getCManager(SstStream stream) { return stream->CPInfo->cm; }

static MPI_Comm CP_getMPIComm(SstStream stream) { return stream->mpiComm; }

static void CP_sendToPeer(SstStream s, CP_PeerCohort cohort, int rank,
                          CMFormat format, void *data)
{
    CP_PeerConnection *peers = (CP_PeerConnection *)cohort;
    if (peers[rank].CMconn == NULL) {
        peers[rank].CMconn = CMget_conn(s->CPInfo->cm, peers[rank].ContactList);
    }
    CMwrite(peers[rank].CMconn, format, data);
}
