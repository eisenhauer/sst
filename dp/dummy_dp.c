#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atl.h>
#include <evpath.h>

#include "dp_interface.h"

int DP_get_contact_info() { return 0; }

typedef struct private_reader_info {
    CManager cm;
    void *CP_stream;
    CMFormat readRequestFormat;
    int rank;

    /* writer info */
    int writer_cohort_size;
    SST_peerCohort peerCohort;
    struct _dp_writer_contact_info *writer_contact_info;
} dummy_dp_reader_private;

typedef struct private_per_reader_writer_info {
    void *remote_reader_ID;
    struct private_writer_info *WS_stream;
    SST_peerCohort peerCohort;
} dummy_dp_per_reader_writer_private;

typedef struct _timestep_entry {
    long timestep;
    void *data;
    struct _timestep_entry *next;
} *timestepList;

typedef struct private_writer_info {
    CManager cm;
    void *CP_stream;

    timestepList timesteps;
    CMFormat readReplyFormat;

    int reader_count;
    dummy_dp_per_reader_writer_private **readers;
} dummy_dp_writer_private;

typedef struct _dp_reader_contact_info {
    char *dp_contact_info;
    int random_integer;
    void *stream_ID;
} * dp_reader_contact_info;

typedef struct _dp_writer_contact_info {
    char *dp_contact_info;
    void *stream_ID;
} * dp_writer_contact_info;

typedef struct _dummyReadRequestMsg {
    long timestep;
    size_t offset;
    size_t length;
    void *WS_stream;
    void *RS_stream;
    int requestingRank;
    int notifyCondition;
} *dummyReadRequestMsg;

static FMField dummyReadRequestList[] = {
    {"timestep", "integer", sizeof(long),
     FMOffset(dummyReadRequestMsg, timestep)},
    {"offset", "integer", sizeof(size_t),
     FMOffset(dummyReadRequestMsg, offset)},
    {"length", "integer", sizeof(size_t),
     FMOffset(dummyReadRequestMsg, length)},
    {"WS_stream", "integer", sizeof(void*),
     FMOffset(dummyReadRequestMsg, WS_stream)},
    {"RS_stream", "integer", sizeof(void*),
     FMOffset(dummyReadRequestMsg, RS_stream)},
    {"requestingRank", "integer", sizeof(int),
     FMOffset(dummyReadRequestMsg, requestingRank)},
    {"notifyCondition", "integer", sizeof(int),
     FMOffset(dummyReadRequestMsg, notifyCondition)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec dummyReadRequestStructs[] = {
    {"dummy_read_request", dummyReadRequestList, sizeof(struct _dummyReadRequestMsg),
     NULL},
    {NULL, NULL, 0, NULL}};


typedef struct _dummyReadReplyMsg {
    long timestep;
    size_t data_length;
    void *RS_stream;
    char *data;
    int notifyCondition;
} *dummyReadReplyMsg;

static FMField dummyReadReplyList[] = {
    {"timestep", "integer", sizeof(long),
     FMOffset(dummyReadReplyMsg, timestep)},
    {"RS_stream", "integer", sizeof(void*),
     FMOffset(dummyReadReplyMsg, RS_stream)},
    {"data_length", "integer", sizeof(size_t),
     FMOffset(dummyReadReplyMsg, data_length)},
    {"data", "char[data_length]", sizeof(char),
     FMOffset(dummyReadReplyMsg, data)},
    {"notifyCondition", "integer", sizeof(int),
     FMOffset(dummyReadReplyMsg, notifyCondition)},
    {NULL, NULL, 0, 0}};

static FMStructDescRec dummyReadReplyStructs[] = {
    {"dummy_read_reply", dummyReadReplyList, sizeof(struct _dummyReadReplyMsg),
     NULL},
    {NULL, NULL, 0, NULL}};

static void dummyReadReplyHandler(CManager cm, CMConnection conn,
				  void *msg_v, void *client_data,
				  attr_list attrs);

static DP_RS_stream DummyInitReader(SST_services svcs, void *CP_stream, void **init_exchange_info)
{
    dummy_dp_reader_private *stream =
        malloc(sizeof(struct private_reader_info));
    dp_reader_contact_info init_info =
        malloc(sizeof(struct _dp_reader_contact_info));
    CManager cm = svcs->getCManager(CP_stream);

    memset(stream, 0, sizeof(*stream));
    memset(init_info, 0, sizeof(*init_info));

    /* if (!static_cm) { */
    /*     init_cm(0); */
    /* } */
    /* stream->cm = static_cm; */
    stream->CP_stream = CP_stream;
    stream->rank = svcs->myRank(CP_stream);
    stream->readRequestFormat = CMregister_format(cm, dummyReadRequestStructs);
    CMFormat f = CMregister_format(cm, dummyReadReplyStructs);
    CMregister_handler(f, dummyReadReplyHandler, svcs);

    init_info->dp_contact_info = "WTF?";
    init_info->random_integer = 42;
    init_info->stream_ID = stream;

    *init_exchange_info = init_info;

    return stream;
}

static void dummyReadRequestHandler(CManager cm, CMConnection conn,
				    void *msg_v, void *client_data,
				    attr_list attrs)
{
    dummyReadRequestMsg readRequestMsg = (dummyReadRequestMsg)msg_v;
    dummy_dp_per_reader_writer_private *WSR_stream = readRequestMsg->WS_stream;

    dummy_dp_writer_private *WS_stream = WSR_stream->WS_stream;
    timestepList tmp = WS_stream->timesteps;
    SST_services svcs = (SST_services) client_data;

    svcs->verbose(WS_stream->CP_stream, "Writer got a request to read remote memory\n");
    while (tmp != NULL) {
	if (tmp->timestep == readRequestMsg->timestep) {
	    struct _dummyReadReplyMsg readReplyMsg;
	    readReplyMsg.timestep = readRequestMsg->timestep;
	    readReplyMsg.data_length = readRequestMsg->length;
	    readReplyMsg.data = tmp->data + readRequestMsg->offset;
	    readReplyMsg.RS_stream = readRequestMsg->RS_stream;
	    readReplyMsg.notifyCondition = readRequestMsg->notifyCondition;
	    svcs->verbose(WS_stream->CP_stream, "Writer sending a reply to remote memory read\n");
	    svcs->sendToPeer(WS_stream->CP_stream, WSR_stream->peerCohort, readRequestMsg->requestingRank, 
			     WS_stream->readReplyFormat, &readReplyMsg);
	    return;
	}
	tmp = tmp->next;
    }
    /* should never get here because we shouldn't get requests for timesteps we don't have */
}

struct _completion_handle {
    int CMcondition;
    CManager cm;
    void *CPstream;
    void *buffer;
    int rank;
};

static void dummyReadReplyHandler(CManager cm, CMConnection conn,
				  void *msg_v, void *client_data,
				  attr_list attrs)
{
    dummyReadReplyMsg readReplyMsg = (dummyReadReplyMsg)msg_v;
    dummy_dp_reader_private *RS_stream = readReplyMsg->RS_stream;
    SST_services svcs = (SST_services) client_data;
    struct _completion_handle *handle =
        CMCondition_get_client_data(cm, readReplyMsg->notifyCondition);

    svcs->verbose(RS_stream->CP_stream, "Reader got a reply to remote memory read, condition is %d\n", readReplyMsg->notifyCondition);
    memcpy(handle->buffer, readReplyMsg->data, readReplyMsg->data_length);
    CMCondition_signal(cm, readReplyMsg->notifyCondition);
}


static DP_WS_stream DummyInitWriter(SST_services svcs, void *CP_stream)
{
    dummy_dp_writer_private *stream =
        malloc(sizeof(struct private_writer_info));
    CManager cm = svcs->getCManager(CP_stream);

    memset(stream, 0, sizeof(struct private_writer_info));
    stream->CP_stream = CP_stream;
    stream->timesteps = NULL;
    CMFormat f = CMregister_format(cm, dummyReadRequestStructs);
    CMregister_handler(f, dummyReadRequestHandler, svcs);
    stream->readReplyFormat = CMregister_format(cm, dummyReadReplyStructs);

    return (void *)stream;
}

static DP_WSR_stream DummyWriterPerReaderInit(SST_services svcs, DP_WS_stream WS_stream_v,
                                             int readerCohortSize, SST_peerCohort peerCohort,
                                             void **providedReaderInfo_v,
                                             void **initWriterInfo)
{
    dummy_dp_writer_private *WS_stream = (dummy_dp_writer_private *)WS_stream_v;

    dummy_dp_per_reader_writer_private *this_reader =
        malloc(sizeof(*this_reader));
    dp_writer_contact_info this_writer_contact =
        malloc(sizeof(struct _dp_writer_contact_info));
    int rank = svcs->myRank(WS_stream->CP_stream);
    char *dummy_contact_string = malloc(32);
    sprintf(dummy_contact_string, "Rank %d, test contact", rank);

    printf("Assigning WS_stream = %p to WSR_stream %p\n", WS_stream, this_reader);
    this_reader->WS_stream = WS_stream; /* pointer to writer struct */
    this_reader->peerCohort = peerCohort;
    WS_stream->readers =
        realloc(WS_stream->readers,
                sizeof(*this_reader) * (WS_stream->reader_count + 1));
    WS_stream->readers[WS_stream->reader_count] = this_reader;
    WS_stream->reader_count++;

    memset(this_writer_contact, 0, sizeof(struct _dp_writer_contact_info));
    this_writer_contact->dp_contact_info = dummy_contact_string;
    svcs->verbose(WS_stream->CP_stream, "WSR rank %d has stream ID %p\n", 
		  rank, this_reader);
    this_writer_contact->stream_ID = this_reader;
    *initWriterInfo = this_writer_contact;
    return this_reader;
}

static void DummyReaderProvideWriterData(SST_services svcs, DP_RS_stream RS_stream_v,
						 int writerCohortSize, SST_peerCohort peerCohort,
						 void **providedWriterInfo_v)
{
    dummy_dp_reader_private *RS_stream = (dummy_dp_reader_private *)RS_stream_v;
    dp_writer_contact_info *providedWriterInfo = (dp_writer_contact_info*)providedWriterInfo_v;

    RS_stream->peerCohort = peerCohort;
    RS_stream->writer_cohort_size = writerCohortSize;

    /* make a copy of writer contact information (original will not be preserved) */
    RS_stream->writer_contact_info = malloc(sizeof(struct _dp_writer_contact_info)*writerCohortSize);
    for (int i = 0; i < writerCohortSize; i++) {
	RS_stream->writer_contact_info[i].dp_contact_info = strdup(providedWriterInfo[i]->dp_contact_info);
	RS_stream->writer_contact_info[i].stream_ID = providedWriterInfo[i]->stream_ID;
	svcs->verbose(RS_stream->CP_stream, "Reader received contact info %s for WSR rank %d\n", RS_stream->writer_contact_info[i].dp_contact_info, i);
	svcs->verbose(RS_stream->CP_stream, "Reader received stream ID %p for WSR rank %d\n", RS_stream->writer_contact_info[i].stream_ID, i);
    }
}

static void *
DummyReadRemoteMemory(SST_services svcs, DP_RS_stream stream_v, int rank, long timestep, size_t offset, size_t length, void *buffer)
{
    dummy_dp_reader_private *stream = (dummy_dp_reader_private *) stream_v;   /* DP_RS_stream is the return from InitReader */
    CManager cm = svcs->getCManager(stream->CP_stream);
    struct _completion_handle *ret = malloc(sizeof(struct _completion_handle));
    struct _dummyReadRequestMsg readRequestMsg;

    ret->CMcondition = CMCondition_get(cm, NULL);
    ret->CPstream = stream->CP_stream;
    ret->cm = cm;
    ret->buffer = buffer;
    ret->rank = rank;
    /* 
     * set the completion handle as client data on the condition so that
     * handler has access to it.
     */
    CMCondition_set_client_data(cm, ret->CMcondition, ret);

    svcs->verbose(stream->CP_stream, "Got a request to read remote memory destionation is rank %d, WSR_stream = %p\n", rank, stream->writer_contact_info[rank].stream_ID);
    /* send request to appropriate writer */

    readRequestMsg.timestep = timestep;
    readRequestMsg.offset = offset;
    readRequestMsg.length = length;
    readRequestMsg.WS_stream = stream->writer_contact_info[rank].stream_ID;
    readRequestMsg.RS_stream = stream;
    readRequestMsg.requestingRank = stream->rank;
    readRequestMsg.notifyCondition = ret->CMcondition;
    svcs->sendToPeer(stream->CP_stream, stream->peerCohort, rank, stream->readRequestFormat, 
		     &readRequestMsg);

    return ret;
}

static void
DummyWaitForCompletion(SST_services svcs, void *handle_v)
{
    struct _completion_handle *handle = (struct _completion_handle *)handle_v;
    svcs->verbose(handle->CPstream, "DP waiting for read completion, condition %d\n", handle->CMcondition);
    CMCondition_wait(handle->cm, handle->CMcondition);
    free(handle);
}

static void
DummyProvideTimestep(SST_services svcs, DP_WS_stream stream_v, void *data, long timestep)
{
    dummy_dp_writer_private *stream = (dummy_dp_writer_private *)stream_v;
    timestepList entry = malloc(sizeof(struct _timestep_entry));

    entry->data = data;
    entry->timestep = timestep;
    entry->next = stream->timesteps;
    stream->timesteps = entry;
}

static void
DummyReleaseTimestep(SST_services svcs, DP_WS_stream stream_v, long timestep)
{
    dummy_dp_writer_private *stream = (dummy_dp_writer_private *)stream_v;
    timestepList curr = stream->timesteps;

    if (stream->timesteps->timestep == timestep) {
	stream->timesteps = curr->next;
	free(curr);
    } else {
	timestepList last = curr;
	curr = curr->next;
	while (curr != NULL) {
	    if (curr->timestep == timestep) {
		last->next = curr->next;
		free(curr);
	    }
	    last = curr;
	    curr = curr->next;
	}
	/* shouldn't ever get here because we should never release a timestep that we don't have */
	fprintf(stderr, "Failed to release timestep %ld, not found\n", timestep);
    }
}

FMField dp_reader_contact_list[] = {
    {"dp_contact_info", "string", sizeof(char *),
     FMOffset(dp_reader_contact_info, dp_contact_info)},
    {"random_integer", "integer", sizeof(int),
     FMOffset(dp_reader_contact_info, random_integer)},
    {"reader_ID", "integer", sizeof(void *),
     FMOffset(dp_reader_contact_info, stream_ID)},
    {NULL, NULL, 0, 0}};

FMStructDescRec dp_reader_contact_formats[] = {
    {"dp_reader_contact_info", dp_reader_contact_list,
     sizeof(struct _dp_reader_contact_info), NULL},
    {NULL, NULL, 0, NULL}};

FMField dp_writer_contact_list[] = {
    {"dp_contact_info", "string", sizeof(char *),
     FMOffset(dp_writer_contact_info, dp_contact_info)},
    {"writer_ID", "integer", sizeof(void *),
     FMOffset(dp_writer_contact_info, stream_ID)},
    {NULL, NULL, 0, 0}};

FMStructDescRec dp_writer_contact_formats[] = {
    {"dp_writer_contact_info", dp_writer_contact_list,
     sizeof(struct _dp_writer_contact_info), NULL},
    {NULL, NULL, 0, NULL}};

struct _SST_DP_Interface dummyDPInterface;

extern SST_DP_Interface LoadDummyDP()
{
    memset(&dummyDPInterface, 0, sizeof(dummyDPInterface));
    dummyDPInterface.readerContactFormats = dp_reader_contact_formats;
    dummyDPInterface.writerContactFormats = dp_writer_contact_formats;
    dummyDPInterface.InitReader = DummyInitReader;
    dummyDPInterface.InitWriter = DummyInitWriter;
    dummyDPInterface.WriterPerReaderInit = DummyWriterPerReaderInit;
    dummyDPInterface.ReaderProvideWriterData = DummyReaderProvideWriterData;
    dummyDPInterface.ReadRemoteMemory = DummyReadRemoteMemory;
    dummyDPInterface.WaitForCompletion = DummyWaitForCompletion;
    dummyDPInterface.ProvideTimestep = DummyProvideTimestep;
    dummyDPInterface.ReleaseTimestep = DummyReleaseTimestep;
    return &dummyDPInterface;
}
