
struct _SstFullMetadata {
    int WriterCohortSize;
    struct _SstMetadata **WriterMetadata;
    void **DP_TimestepInfo;
};

struct _SstMetadata {
    size_t DataSize;
    int VarCount;
    struct _SstVarMeta *Vars;
};

struct _SstData {
    size_t DataSize;
    char *block;
};

struct _SstVarMeta {
    char *VarName;
    int DimensionCount;
    struct _SstDimenMeta *Dimensions;
    int DataOffsetInBlock;
};

struct _SstDimenMeta {
    int Offset;
    int Size;
    int GlobalSize;
};
