#pragma once

#include "fs.h"
#include "part.h"
#include "KernelFS.h"
#include "Cache.h"
#include "Data.h"
#include "string.h"
using namespace std;

//#include "definitions.h"


const unsigned long ENTRIES_PER_CLUSTER = ClusterSize / sizeof(long);
const unsigned long MAX_FILE_SIZE = ClusterSize * ENTRIES_PER_CLUSTER / 2 *( 1 + ENTRIES_PER_CLUSTER );

class KernelFile {
public:
	
	KernelFile(char, PartitionData*, unsigned, BytesCnt, ClusterNo, string);

	~KernelFile();
	char readFromCluster(ClusterNo, BytesCnt);
	char * getCluster(ClusterNo);

	char write(BytesCnt, char*);
	BytesCnt read(BytesCnt, char* buffer);
	char seek(BytesCnt);
	BytesCnt filePos();
	char eof();
	BytesCnt getFileSize();
	char truncate();
	void closeFile();

	struct fileRegister {
		bool valid;
		bool dirty;
		ClusterNo cluster;
		ClusterNo oldEntry;
		char buffer[ClusterSize];
		
	};
	fileRegister reg;
	inline void loadLocalCluster();
	inline void saveLocalCluster(); 
	inline void readBlockLocally(char*, BytesCnt, BytesCnt);
	inline void writeBlockLocally(char*, BytesCnt, BytesCnt);

//private:
	friend class KernelFS;
	friend class PartitionData;

	BytesCnt cursor, size;
	ClusterNo index;
	PartitionData *partitionData;
	long entry;
	string name;
	char mode;

	ClusterNo readEntryInIndex(ClusterNo);
	ClusterNo readEntryInIndex2(ClusterNo , ClusterNo );
	void writeEntryToIndex(ClusterNo, ClusterNo);
	void writeEntryToIndex2(ClusterNo, ClusterNo, ClusterNo);

};