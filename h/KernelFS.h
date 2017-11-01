#pragma once

#include "part.h"
#include "fs.h"
#include "file.h"
#include "KernelFile.h"
#include "Data.h"
#include "Cache.h"
#include <stack>
#include <vector>

using namespace std;



const unsigned int MAX_PARTITIONS = 26;


class PartitionData;

class KernelFS {
	friend class FS;
	
protected:
	~KernelFS();

	char mount(Partition*);
	char unmount(char);
	char format(char);

	char readRootDir(char, EntryNum, Directory&);

	char doesExist(char*);

	File* open(char*, char);
	char deleteFile(char*);

	KernelFS();

private:
	PartitionData *pData[MAX_PARTITIONS];	
	stack<char> freeLetters;
	void extractPath(char*, char&, char*);
};