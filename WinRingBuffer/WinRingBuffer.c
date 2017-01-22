// WinRingBuffer.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#define MAX_THREADS 5

DWORD WINAPI MyThreadFunction(LPVOID lpParam);

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
	char c = *((char *)lpParam);
	int i = 0;
	char buf[10];
	for (i = 0; i < 7; i++) {
		buf[i] = c;
	}
	buf[7] = '\r';
	buf[8] = '\n';
	buf[9] = '\0';
	log(buf);
	return 0;
}


void generateArrayOfSymbols(char *buf) {
	srand((unsigned)time(NULL));
	
	int i = 0;
	for (i = 0; i < MAX_THREADS; i++) {
		int u = (double)rand() / (RAND_MAX + 1) * (122 - 97) + 97;
		buf[i] = u;
	}
}

int main() {
	initRingBuffer(15);
	
	DWORD   dwThreadIdArray[MAX_THREADS];
	HANDLE  hThreadArray[MAX_THREADS];

	// Create MAX_THREADS worker threads.
	char buf[MAX_THREADS];
	generateArrayOfSymbols(buf);
	for (int i = 0; i<MAX_THREADS; i++)
	{

		hThreadArray[i] = CreateThread(
			NULL,                   // default security attributes
			0,                      // use default stack size  
			MyThreadFunction,       // thread function name
			&buf[i],          // argument to thread function 
			0,                      // use default creation flags 
			&dwThreadIdArray[i]);   // returns the thread identifier 


									// Check the return value for success.
									// If CreateThread fails, terminate execution. 
									// This will automatically clean up threads and memory. 

		if (hThreadArray[i] == NULL)
		{
			ExitProcess(3);
		}
	} // End of main thread creation loop.

	  // Wait until all threads have terminated.

	WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);

	// Close all thread handles and free memory allocations.

	flushAll();
    return 0;
}

