#pragma once
#include "part.h"
#include <cstring>
#include <windows.h>


#define signal(x) ReleaseSemaphore(x,1,NULL)
#define wait(x) WaitForSingleObject(x,INFINITE)

const unsigned int CACHE_SIZE = 4096;

typedef unsigned long BytesCnt;

class Cache {
	
	static unsigned long mask;

	HANDLE mutex[CACHE_SIZE];
	struct TagMemory {
		ClusterNo cluster;
	};	
	TagMemory tag[CACHE_SIZE];
	char * dataMemory[CACHE_SIZE];
	bool dirty[CACHE_SIZE];
	bool valid[CACHE_SIZE];
	Partition *partition;
 
public:
	Cache(Partition*);
	~Cache();
	char* readCluster(ClusterNo);
	void writeCluster(ClusterNo, char*);
	
	void readBlock(ClusterNo, char*, BytesCnt, BytesCnt);

	void writeBlock(ClusterNo, char*, BytesCnt, BytesCnt);
	void flushCache();
	void dumpCache();
};