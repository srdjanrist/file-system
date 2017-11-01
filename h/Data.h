#pragma once
#include <vector>
#include <iostream>
#include <exception>
#include <deque>
#include <list>
#include <Windows.h>
#include "KernelFile.h"
#include "KernelFS.h"
#include "part.h"
#include "cache.h"
#include <map>
#include <string>
#include <unordered_set>
#include <set>

using namespace std;


//const unsigned MAP_SIZE = 32;

const unsigned FILES_PER_CLUSTER = ClusterSize / sizeof(Entry);

class ENonExistentFile : public exception {
public:
	virtual const char* what() const throw() {
		return "File doesn't exist!\n";
	}
};


struct FCB {
	SRWLOCK srwlock;
	int cnt;
	BytesCnt size;
	ClusterNo index;
	FCB() {
		cnt = 0;
		InitializeSRWLock(&srwlock);
	}
};


class PartitionData {
	friend class KernelFS;
	friend class KernelFile;

	Partition *myPartition;
	int numOpenedFiles;	

	HANDLE mutex;

	ClusterNo clusterNumber;
	vector<bool> bitVector;


	Cache *cache;

	bool dirtyBitVector;	
	deque<ClusterNo> freeClusters_;
	/*unordered_*/set<ClusterNo> freeClusters;
	HANDLE setSem = CreateSemaphore(NULL, 1, 1, NULL);

	map<string, FCB*> openedFiles;
	HANDLE unmountSem, formatSem;
	bool formatting;
		

	PartitionData(Partition*);
	~PartitionData();
	char format();

	int numFiles;
	void loadBitVector(Partition*);
	void saveBitVector();

	char doesExist(char*);
	ClusterNo getClusterWithEntry(unsigned );
	char doesExist(char*, unsigned&);
	int readRootDir(EntryNum, Directory&);

	KernelFile* open(char*, char);

	void deleteIndex(ClusterNo);

	unsigned findFreeEntry();

	long createFile(char * name);

	void extractNameExt(char * str, char * name, char * ext);

	ClusterNo getFreeCluster();

	char deleteFile(char*);
	void deleteFile(unsigned);

	void closeFile(KernelFile*);

	void getInfoForEntry(unsigned, BytesCnt &, ClusterNo &);

	void truncateFile(unsigned);
	void truncateFile(ClusterNo, ClusterNo);

	ClusterNo allocateIndex();

	string createString(char * str);

	void freeCluster(ClusterNo);

	ClusterNo getNewIndex();
	
};