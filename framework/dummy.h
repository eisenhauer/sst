SstMetadata CreateDummyMetadata(long timestep, int rank, int size);

extern SstData CreateDummyData(long timestep, int rank, int size);

extern int ValidateDummyData(long timestep, int rank, int size, int offset,
                             void *buffer);
