#pragma once
#include <windows.h>

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
	Locker *locker;
	FlushThread* flusherThread;
	FileHandler * filehandler;
} RingBuffer;

typedef struct _ShouldWrite {
	int toEndBytes;
	int fromBegginningBytes;
} ShouldWrite;






void initFileHandler(FileHandler *locker);
void initLocker(Locker *locker);
void initFlushThread(FlushThread *workerThread);
bool initRingBuffer(int size);
void log(char* string);
void flush(RingBuffer *ringBuffer);
int  getSizeOfString(char *string);
void lockForLog(RingBuffer *ringBuffer);
void unlockForLog(RingBuffer *ringBuffer);
void* tryToReservPointers(RingBuffer * ringBuffer, int size, ShouldWrite *sizes);
void saveToBuffer(RingBuffer *ringBuffer, void* strat, char *string, ShouldWrite *sizes);
void calculatePossibleFlushesSizes(RingBuffer *ringBuffer, ShouldWrite* shouldWrite);
void flushToDisk(RingBuffer *ringBuffer, ShouldWrite *shouldWrite);
bool shouldFlush(RingBuffer *ringBuffer);
void notifyForFlush(RingBuffer *ringBuffer);
//todo shoudle be correct type for address
void writeToFile(FileHandler *handler, int* start, int length);
int calculateCurrentFreeSpace(RingBuffer *buffer);
void* allocateMemory(int size);
void flushAll();