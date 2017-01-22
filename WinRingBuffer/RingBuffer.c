#include "stdafx.h"
#include "RingBuffer.h"

LPCTSTR LOG_FILE_NAME = L"log.txt";
RingBuffer *MainRingBuffer;

bool initRingBuffer(int size)
{
	MainRingBuffer = (RingBuffer *)allocateMemory(sizeof(RingBuffer));
	MainRingBuffer->size = size;
	MainRingBuffer->start = allocateMemory(size);
	MainRingBuffer->pointerToFirstNoFlushedByte = MainRingBuffer->start;
	MainRingBuffer->pointerToNextToWriteByte = MainRingBuffer->start;
	MainRingBuffer->filehandler = (RingBuffer *)allocateMemory(sizeof(RingBuffer));
	initFileHandler(MainRingBuffer->filehandler);
	MainRingBuffer->locker = (Locker *)allocateMemory(sizeof(Locker));
	initLocker(MainRingBuffer->locker);
	MainRingBuffer->flusherThread = (FlushThread *)allocateMemory(sizeof(FlushThread));
	initFlushThread(MainRingBuffer->flusherThread);
	return true;
}

void log(char * string) {
	lockForLog(MainRingBuffer);
	int stringLength = getSizeOfString(string);
	ShouldWrite sizes;
	void *startAddress = tryToReservPointers(MainRingBuffer, stringLength, &sizes);
	while (startAddress == NULL) {
		startAddress = tryToReservPointers(MainRingBuffer, stringLength, &sizes);
	}
	saveToBuffer(MainRingBuffer, startAddress, string, &sizes);
	unlockForLog(MainRingBuffer);
	if (shouldFlush(MainRingBuffer)) {
		notifyForFlush(MainRingBuffer);
	}
}

void flush(RingBuffer *ringBuffer) {
	ShouldWrite shouldWrite;
	calculatePossibleFlushesSizes(ringBuffer, &shouldWrite);
	flushToDisk(ringBuffer, &shouldWrite);
}

void flushToDisk(RingBuffer *ringBuffer, ShouldWrite *shouldWrite) {
	if (shouldWrite->toEndBytes != 0) {
		writeToFile(ringBuffer->filehandler, ringBuffer->pointerToFirstNoFlushedByte, 
			shouldWrite->toEndBytes);
		ringBuffer->pointerToFirstNoFlushedByte = ringBuffer->pointerToFirstNoFlushedByte + shouldWrite->toEndBytes;
	}
	if (shouldWrite->fromBegginningBytes != 0) {
		writeToFile(ringBuffer->filehandler, ringBuffer->start, shouldWrite->fromBegginningBytes);
		ringBuffer->pointerToFirstNoFlushedByte = ringBuffer->start + shouldWrite->fromBegginningBytes;
	}
	//todo should be atomic
	ringBuffer->countOfNonFlushedBytes = 0;
}


void writeToFile(FileHandler *handler, int* start, int length) {
	BOOL bErrorFlag = FALSE;
	DWORD dwBytesWritten = 0;
	bErrorFlag = WriteFile(
		handler->hFile,           // open file handle
		start,      // start of data to write
		length,  // number of bytes to write
		&dwBytesWritten, // number of bytes that were written
		NULL);            // no overlapped structure

	if (FALSE == bErrorFlag)
	{
		printf("Terminal failure: Unable to write to file.\n");
		abort();
	}
	else
	{
		if (length != dwBytesWritten)
		{
			// This is an error because a synchronous write that results in
			// success (WriteFile returns TRUE) should write all data as
			// requested. This would not necessarily be the case for
			// asynchronous writes.
			printf("Error: dwBytesWritten != dwBytesToWrite\n");
			abort();
		}
	};
}

//todo shoudle be correct type for address
 void calculatePossibleFlushesSizes(RingBuffer *ringBuffer, ShouldWrite* shouldWrite) {
	 int diff = ringBuffer->pointerToNextToWriteByte - ringBuffer->pointerToFirstNoFlushedByte;
	 if (diff >= 0) {
		 shouldWrite->toEndBytes = diff;
		 shouldWrite->fromBegginningBytes = 0;
	 } else {
		 shouldWrite->toEndBytes = ringBuffer->start + ringBuffer->size - ringBuffer->pointerToFirstNoFlushedByte;
		 shouldWrite->fromBegginningBytes = ringBuffer->size + diff;
	 }
}

 void initLocker(Locker *locker) {
	 locker->mutex = CreateMutex(
		 NULL,              // default security attributes
		 FALSE,             // initially not owned
		 NULL);
	 if (locker->mutex == NULL)
	 {
		 printf("CreateMutex error: %d\n", GetLastError());
		 abort();
	 }
 }

 DWORD WINAPI FlushToDisk(LPVOID lpParam)
 {
	 
	 while (1) {
		 Sleep(1000);
		 flush(MainRingBuffer);
	}
 }

 void initFlushThread(FlushThread *workerThread) {
	 workerThread->workerThread = CreateThread(
		 NULL,       // default security attributes
		 0,          // default stack size
		 (LPTHREAD_START_ROUTINE)FlushToDisk,
		 NULL,       // no thread function arguments
		 0,          // default creation flags
		 &workerThread->ThreadID); // receive thread identifier

	 if (workerThread->workerThread == NULL)
	 {
		 printf("CreateThread error: %d\n", GetLastError());
		 abort();
	 }
 }

 void initFileHandler(FileHandler *locker) {
	 locker->hFile = CreateFile(LOG_FILE_NAME,                // name of the write
		 GENERIC_WRITE,          // open for writing
		 0,                      // do not share
		 NULL,                   // default security
		 CREATE_NEW,             // create new file only
		 FILE_ATTRIBUTE_NORMAL,  // normal file
		 NULL);                  // no attr. template

	 if (locker->hFile == INVALID_HANDLE_VALUE)
	 {
		 printf("Unable to open file %d\n", GetLastError());
		 abort();
	 }
 }

 void lockForLog(RingBuffer *ringBuffer) {
	 DWORD dwWaitResult = WaitForSingleObject(
		 ringBuffer->locker->mutex,    // handle to mutex
		 INFINITE);  // no time-out interval
	 if (dwWaitResult == WAIT_ABANDONED) {
		 printf("Unable to WaitForSingleObject %d\n", GetLastError());
		 abort();
	 }
 }

 

 void unlockForLog(RingBuffer *ringBuffer) {
	 if (!ReleaseMutex(ringBuffer->locker->mutex)) {
		 printf("Unable to oReleaseMutex %d\n", GetLastError());
		 abort();
	 }
 }

 void notifyForFlush(RingBuffer *ringBuffer) {
	 return;
 }

 bool shouldFlush(RingBuffer *ringBuffer) {
	 return false;
 }

 void saveToBuffer(RingBuffer *ringBuffer, void * startAddress, char *string, ShouldWrite *sizes) {
	 if (sizes->toEndBytes != 0) {
		 memcpy(startAddress, string, sizes->toEndBytes);
	 }
	 if (sizes->fromBegginningBytes != 0) {
		 memcpy(ringBuffer->start, string + sizes->toEndBytes, sizes->fromBegginningBytes);
	 }
 }

 void * tryToReservPointers(RingBuffer * ringBuffer, int size, ShouldWrite *sizes) {
	 int currentAvaliableSize = calculateCurrentFreeSpace(ringBuffer);
	 if (currentAvaliableSize < size) {
		 return NULL;
	 }
	 int diff = (ringBuffer->start + ringBuffer->size) - (ringBuffer->pointerToNextToWriteByte + size);
	 void * oldStartToWrite = ringBuffer->pointerToNextToWriteByte;
	 if (diff >= 0) {
		 ringBuffer->pointerToNextToWriteByte += size;
		 sizes->toEndBytes = size;
		 sizes->fromBegginningBytes = 0;
	 } else {
		 ringBuffer->pointerToNextToWriteByte = ringBuffer->start - diff;
		 sizes->toEndBytes = ringBuffer->start + ringBuffer->size - ringBuffer->pointerToNextToWriteByte;
		 sizes->fromBegginningBytes = -diff;
	 }
	 return oldStartToWrite;
 }

 int calculateCurrentFreeSpace(RingBuffer *ringBuffer) {
	 int diff = ringBuffer->pointerToNextToWriteByte - ringBuffer->pointerToFirstNoFlushedByte;
	 if (diff >= 0) {
		 return ringBuffer->size - diff;
	 } else {
		 return -diff;
	 }
 }

 int  getSizeOfString(char *string) {
	 return strlen(string);
 }

 void* allocateMemory(int size) {
	 return malloc(size);
 }

 void flushAll() {
	 Sleep(5000);
 }