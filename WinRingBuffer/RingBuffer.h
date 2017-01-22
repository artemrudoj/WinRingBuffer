#pragma once
#include <windows.h>
//#define KERNEL
//#ifdef KERNEL
//#include <Wdm.h>
//#else
// TODO: USERSPACE include files
//#endif

#ifdef KERNEL
#define PRINT(msg, ...) DbgPrint(msg, __VA_ARGS__)
#else
#define PRINT(msg, ...) printf(msg, __VA_ARGS__)
#endif
typedef struct _Locker {
	HANDLE mutex;
} Locker;

typedef struct _FlushThread {
	HANDLE workerThread;
	DWORD ThreadID;
} FlushThread;


typedef struct _FileHandler {
	HANDLE hFile;
} FileHandler;

typedef struct _RingBuffer {
	int* start;
	int  size;
	char* pointerToFirstNoFlushedByte;
	char* pointerToNextToWriteByte;
	char  countOfNonFlushedBytes;
	Locker locker;
	FlushThread flusherThread;
	FileHandler  filehandler;
} RingBuffer;

typedef struct _ShouldWrite {
	int toEndBytes;
	int fromBegginningBytes;
} ShouldWrite;






bool initFileHandler(FileHandler *locker);
bool initLocker(Locker *locker);
bool initFlushThread(FlushThread *workerThread);
bool initRingBuffer(int size);
void log(char* string);
void flush(RingBuffer *ringBuffer);
int  getSizeOfString(char *string);
bool lockForLog(RingBuffer *ringBuffer);
bool unlockForLog(RingBuffer *ringBuffer);
void* tryToReservPointers(RingBuffer * ringBuffer, int size, ShouldWrite *sizes);
void saveToBuffer(RingBuffer *ringBuffer, void* strat, char *string, ShouldWrite *sizes);
void calculatePossibleFlushesSizes(RingBuffer *ringBuffer, ShouldWrite* shouldWrite);
void flushToDisk(RingBuffer *ringBuffer, ShouldWrite *shouldWrite);
bool shouldFlush(RingBuffer *ringBuffer);
void notifyForFlush(RingBuffer *ringBuffer);
//todo shoudle be correct type for address
bool writeToFile(FileHandler *handler, int* start, int length);
int calculateCurrentFreeSpace(RingBuffer *buffer);
void* allocateMemory(int size);
void flushAll();
void freeMemory(void *pointer);