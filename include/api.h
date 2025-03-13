extern void* Setup(const char *dev, int queueId, int needWakeup, int sharedUmem, int forceSkb, int zeroCopy);
extern int Cleanup(void* xskPtr);
extern int SendPacket(void* xskPtr, void *pkt, int length, int batchSize);