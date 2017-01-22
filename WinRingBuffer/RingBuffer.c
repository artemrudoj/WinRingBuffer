#include "stdafx.h"
#include "RingBuffer.h"

LPCTSTR LOG_FILE_NAME = L"log.txt";
RingBuffer *MainRingBuffer;

bool initRingBuffer(int size) {
	MainRingBuffer = (RingBuffer *)allocateMemory(sizeof(RingBuffer));
	if (MainRingBuffer == NULL) {
		PRINT("MainRingBuffer = NULL");
		return false;
	}
	MainRingBuffer->size = size;
	MainRingBuffer->start = allocateMemory(size);
	if (MainRingBuffer->start == NULL) {
		PRINT("MainRingBuffer->start = NULL");
		goto free_buf;
	}
	MainRingBuffer->pointerToFirstNoFlushedByte = MainRingBuffer->start;
	MainRingBuffer->pointerToNextToWriteByte = MainRingBuffer->start;

	if (!initFileHandler(&MainRingBuffer->filehandler)) {
		PRINT("initFileHandler incorrect");
		goto free_buf;
	}
	if (!initLocker(&MainRingBuffer->locker)) {
		PRINT("initLocker incorrect");
		goto free_buf;
	}
	if (!initFlushThread(&MainRingBuffer->flusherThread)) {
		PRINT("initFlushThread incorrect");
		goto free_buf;
	}
	return true;
free_buf:
	freeMemory(MainRingBuffer->start);
free_main_buf:
	freeMemory(MainRingBuffer);
	return false;
}

void log(char * string) {
	lockForLog(MainRingBuffer);
	int stringLength = getSizeOfString(string);
	ShouldWrite sizes;
	char *startAddress = tryToReservPointers(MainRingBuffer, stringLength, &sizes);
	while (startAddress == NULL) {
		startAddress = tryToReservPointers(MainRingBuffer, stringLength, &sizes);
	}
	LogEntry* logEntry = (LogEntry* )startAddress;
	
	logEntry->isReady = false;
	logEntry->size = stringLength;
	
	unlockForLog(MainRingBuffer);
	saveToBuffer(MainRingBuffer, startAddress, string, &sizes);
	logEntry->isReady = true;
	
	if (shouldFlush(MainRingBuffer)) {
		notifyForFlush(MainRingBuffer);
	}
}

void flush(RingBuffer *ringBuffer) {
	char buf[200];
	int length;
	void * lastNonFlushedPointer = calculatePossibleFlushesSizes(ringBuffer, buf, &length);
	if (lastNonFlushedPointer != NULL) {
		writeToFile(&ringBuffer->filehandler, buf, length);
		ringBuffer->pointerToFirstNoFlushedByte = lastNonFlushedPointer;
	}
}



bool writeToFile(FileHandler *handler, int* start, int length) {
	BOOL bErrorFlag = FALSE;
	DWORD dwBytesWritten = 0;
	bErrorFlag = WriteFile(
		handler->hFile,           // open file handle
		start,      // start of data to write
		length,  // number of bytes to write
		&dwBytesWritten, // number of bytes that were written
		NULL);            // no overlapped structure

	if (FALSE == bErrorFlag) {
		PRINT("Terminal failure: Unable to write to file.\n");
		return false;
	} else {
		if (length != dwBytesWritten)
		{
			// This is an error because a synchronous write that results in
			// success (WriteFile returns TRUE) should write all data as
			// requested. This would not necessarily be the case for
			// asynchronous writes.
			PRINT("Error: dwBytesWritten != dwBytesToWrite\n");
			return false;
		}
	};
	return true;
}

bool isEnoughtForHeader(RingBuffer *buffer, char * pointer) {
	if (buffer->start + buffer->size - pointer > sizeOfLogEntryHeader()) {
		return true;
	} else {
		return false;
	}
}
//todo shoudle be correct type for address
 void* calculatePossibleFlushesSizes(RingBuffer *ringBuffer, char *buffer, int *sumLength) {
	 // nothing to flush
	 if (ringBuffer->pointerToFirstNoFlushedByte == ringBuffer->pointerToNextToWriteByte) {
		 return NULL;
	 }
	 LogEntry *iterator;
	 char *data;
	 int headerSize = sizeOfLogEntryHeader();
	 if (isEnoughtForHeader(ringBuffer, ringBuffer->pointerToFirstNoFlushedByte)) {
		 iterator = ringBuffer->pointerToFirstNoFlushedByte;
	 } else {
		 iterator = ringBuffer->start;
	 }
	 *sumLength = 0;
	 char *tmpBuffer = buffer;
	 void *firstToNonFlushed = iterator;
	 while (iterator->isReady == 1) {
		 (*sumLength) += iterator->size;
		 iterator->isReady = false;
		 data = (char *)iterator + headerSize;
		 int sizeToEnd = (ringBuffer->start + ringBuffer->size) - data;
		 int diff = sizeToEnd - iterator->size;
		 if (diff >= 0) {
			 memcpy(tmpBuffer, data, iterator->size);
			 tmpBuffer += iterator->size;
			 iterator = (char *)iterator + headerSize + iterator->size;
		 } else {
			 if (sizeToEnd != 0) {
				 memcpy(tmpBuffer, data, sizeToEnd);
			 }
			 memcpy(tmpBuffer + sizeToEnd, ringBuffer->start,  -diff);
			 tmpBuffer += iterator->size;
			 iterator = (char *)ringBuffer->start - diff;
		 }
		 firstToNonFlushed = iterator;
		 if (!isEnoughtForHeader(ringBuffer, iterator)) {
			 iterator = ringBuffer->start;
		 }
	 }
	 return firstToNonFlushed;
}

 bool initLocker(Locker *locker) {
	 locker->mutex = CreateMutex(
		 NULL,              // default security attributes
		 FALSE,             // initially not owned
		 NULL);
	 if (locker->mutex == NULL) {
		 PRINT("CreateMutex error: %d\n", GetLastError());
		 return false;
	 }
	 return true;
 }

 DWORD WINAPI FlushToDisk(LPVOID lpParam) {
	 while (1) {
		 Sleep(1000);
		 flush(MainRingBuffer);
	 }
 }

 bool initFlushThread(FlushThread *workerThread) {
	 workerThread->workerThread = CreateThread(
		 NULL,       // default security attributes
		 0,          // default stack size
		 (LPTHREAD_START_ROUTINE)FlushToDisk,
		 NULL,       // no thread function arguments
		 0,          // default creation flags
		 &workerThread->ThreadID); // receive thread identifier

	 if (workerThread->workerThread == NULL) {
		 PRINT("CreateThread error: %d\n", GetLastError());
		 return false;
	 }
	 return true;
 }

 bool initFileHandler(FileHandler *locker) {
	 locker->hFile = CreateFile(LOG_FILE_NAME,                // name of the write
		 GENERIC_WRITE,          // open for writing
		 0,                      // do not share
		 NULL,                   // default security
		 CREATE_NEW,             // create new file only
		 FILE_ATTRIBUTE_NORMAL,  // normal file
		 NULL);                  // no attr. template

	 if (locker->hFile == INVALID_HANDLE_VALUE) {
		 PRINT("Unable to open file %d\n", GetLastError());
		 return false;
	 }
	 return true;
 }

 bool lockForLog(RingBuffer *ringBuffer) {
	 DWORD dwWaitResult = WaitForSingleObject(
		 ringBuffer->locker.mutex,    // handle to mutex
		 INFINITE);  // no time-out interval
	 if (dwWaitResult == WAIT_ABANDONED) {
		 PRINT("Unable to WaitForSingleObject %d\n", GetLastError());
		 return false;
	 }
	 return true;
 }

 

 bool unlockForLog(RingBuffer *ringBuffer) {
	 if (!ReleaseMutex(ringBuffer->locker.mutex)) {
		 PRINT("Unable to oReleaseMutex %d\n", GetLastError());
		 return false;
	 }
	 return true;
 }

 void notifyForFlush(RingBuffer *ringBuffer) {
	 return;
 }

 bool shouldFlush(RingBuffer *ringBuffer) {
	 return false;
 }

 //should consider header
 void saveToBuffer(RingBuffer *ringBuffer, char * startAddress, char *string, ShouldWrite *sizes) {
	 int headerSize = sizeOfLogEntryHeader();
	 // all on begin of buffer
	 if (sizes->toEndBytes == 0) {
		 memcpy(ringBuffer->start + headerSize, string, sizes->fromBegginningBytes - headerSize);
	 } else if (sizes->toEndBytes == headerSize){
		 //only header on the end of buffer
		 memcpy(ringBuffer->start, string, sizes->fromBegginningBytes);
	 } else {
		 //only header and some of raw data on the end
		 sizes->toEndBytes -= headerSize;
		 char *tmpPointer = (char *)startAddress;
		 tmpPointer += headerSize;
		 memcpy(tmpPointer, string, sizes->toEndBytes);
		 if (sizes->fromBegginningBytes != 0) {
			 memcpy(ringBuffer->start, string + sizes->toEndBytes, sizes->fromBegginningBytes);
		 }
	 }
 }

 void * tryToReservPointers(RingBuffer * ringBuffer, int size, ShouldWrite *sizes) {
	 int currentAvaliableSize = calculateCurrentFreeSpace(ringBuffer);
	 int logEntryHeaderLength = sizeOfLogEntryHeader();

	 int restToEnd = (ringBuffer->start + ringBuffer->size) - ringBuffer->pointerToNextToWriteByte;
	 int additionalLengthForFragmentation;
     // header always full on end of the line otherwise to begin
	 if (restToEnd < logEntryHeaderLength) {
		 additionalLengthForFragmentation = restToEnd;
	 } else {
		 additionalLengthForFragmentation = 0;
	 }
	 if (currentAvaliableSize < size + logEntryHeaderLength + additionalLengthForFragmentation) {
		 return NULL;
	 }
	 if (restToEnd < logEntryHeaderLength) {
		 ringBuffer->pointerToNextToWriteByte = ringBuffer->start;
	 }
	 int fullSizeWithHeader = size + logEntryHeaderLength;
	 int diff = (ringBuffer->start + ringBuffer->size) - (ringBuffer->pointerToNextToWriteByte + fullSizeWithHeader);
	 void * oldStartToWrite = ringBuffer->pointerToNextToWriteByte;
	 if (diff >= 0) {
		 ringBuffer->pointerToNextToWriteByte += fullSizeWithHeader;
		 sizes->toEndBytes = fullSizeWithHeader;
		 sizes->fromBegginningBytes = 0;
	 } else {
		 sizes->toEndBytes = restToEnd;
		 sizes->fromBegginningBytes = fullSizeWithHeader - restToEnd;
		 ringBuffer->pointerToNextToWriteByte = ringBuffer->start + sizes->fromBegginningBytes;
	 }
	 //todo should sync
	 int restToEndForFlush = (ringBuffer->start + ringBuffer->size) - ringBuffer->pointerToFirstNoFlushedByte;
	 if (restToEnd < logEntryHeaderLength) {
		 ringBuffer->pointerToFirstNoFlushedByte = ringBuffer->start;
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
	 Sleep(1000);
 }

 void freeMemory(void *pointer) {
	 free(pointer);
 }

 int sizeOfLogEntryHeader() {
	 return sizeof(bool) + sizeof(int);
 }