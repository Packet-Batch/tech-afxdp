extern int Setup(const char *dev, int queueId, int needWakeup, int sharedUmem, int forceSkb, int zeroCopy, int threads);
extern int Cleanup(int threads);
extern int SendPacket(void *pkt, int length, int threadIdx, int batchSize);