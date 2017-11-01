#pragma once

typedef unsigned long BytesCnt;
typedef unsigned long EntryNum;

const unsigned long ENTRYCNT = 64;
const unsigned int FNAMELEN = 8;
const unsigned int FEXTLEN = 3;

struct Entry {
	char name[FNAMELEN];
	char ext[FEXTLEN];
	char reserved;
	unsigned long indexCluster;
	unsigned long size;
};

typedef Entry Directory[ENTRYCNT];

class KernelFS;
class Partition;
class File;
class FS {
public:

	~FS();

	static char mount(Partition* partition);
	static char unmount(char part);
	static char format(char part);

	static char readRootDir(char part, EntryNum n, Directory &d);

	static char doesExist(char* fname);

	static File* open(char* fname, char mode);
	static char deleteFile(char* fname);

protected:
	FS();
	static KernelFS *myImpl;
};
