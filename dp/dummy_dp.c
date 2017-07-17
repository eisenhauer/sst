#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atl.h>
#include <evpath.h>

#include "dp_interface.h"

int DP_get_contact_info() { return 0; }

static CManager static_cm = NULL;

static atom_t CM_TRANSPORT = 0;

typedef struct private_reader_info {
    CManager cm;
    void *CP_stream;
} dummy_dp_reader_private;

typedef struct private_per_reader_writer_info {
    void *remote_reader_ID;
    struct private_writer_info *WS_stream;
} dummy_dp_per_reader_writer_private;

typedef struct private_writer_info {
    CManager cm;
    void *CP_stream;
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

static DP_RS_stream DummyInitReader(void *CP_stream, void **init_exchange_info)
{
    dummy_dp_reader_private *stream =
        malloc(sizeof(struct private_reader_info));
    static char *proc_contact_info = NULL;
    dp_reader_contact_info init_info =
        malloc(sizeof(struct _dp_reader_contact_info));

    /* if (!static_cm) { */
    /*     init_cm(0); */
    /* } */
    /* stream->cm = static_cm; */
    stream->CP_stream = CP_stream;

    init_info->dp_contact_info = "WTF?";
    init_info->random_integer = 42;
    init_info->stream_ID = stream;

    *init_exchange_info = init_info;

    return stream;
}

static DP_WS_stream DummyInitWriter(void *CP_stream)
{
    dummy_dp_writer_private *stream =
        malloc(sizeof(struct private_writer_info));
    static char *proc_contact_info = NULL;
    memset(stream, 0, sizeof(struct private_writer_info));
    stream->CP_stream = CP_stream;
    return (void *)stream;
}

static DP_WS_stream DummyWriterPerReaderInit(DP_WS_stream WS_stream_v,
                                             int readerCohortSize,
                                             void **providedReaderInfo_v,
                                             void **initWriterInfo)
{
    dummy_dp_writer_private *WS_stream = (dummy_dp_writer_private *)WS_stream_v;

    dummy_dp_per_reader_writer_private *this_reader =
        malloc(sizeof(*this_reader));
    dp_writer_contact_info this_writer_contact =
        malloc(sizeof(struct _dp_writer_contact_info));

    this_reader->WS_stream = WS_stream; /* pointer to writer struct */
    WS_stream->readers =
        realloc(WS_stream->readers,
                sizeof(*this_reader) * (WS_stream->reader_count + 1));
    WS_stream->readers[WS_stream->reader_count] = this_reader;
    WS_stream->reader_count++;

    memset(this_writer_contact, 0, sizeof(struct _dp_writer_contact_info));
    this_writer_contact->dp_contact_info = "testing writer contact";
    this_writer_contact->stream_ID = this_reader;
    *initWriterInfo = this_writer_contact;
    return this_reader;
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
    return &dummyDPInterface;
}
