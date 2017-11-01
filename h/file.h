#pragma once

typedef unsigned long BytesCnt;

class KernelFile;
class File {
public:
	~File();
	char write(BytesCnt, char* buffer);
	BytesCnt read(BytesCnt, char* buffer);
	char seek(BytesCnt);
	BytesCnt filePos();
	char eof();
	BytesCnt getFileSize();
	char truncate();

//private:
	friend class FS;
	friend class KernelFS;
	File();
	KernelFile *myImpl;
};