#include "KernelFS.h"

#include <iostream>
using namespace std;

#define charToPos(c) c - 'A'
#define posToChar(p) p + 'A'

KernelFS::~KernelFS() {

}

char KernelFS::mount(Partition* p) {
	if (freeLetters.empty()) return '0';
	char letter = freeLetters.top();
	freeLetters.pop();
	int pos = charToPos(letter);

	pData[pos] = new PartitionData(p);

	return letter;
}

char KernelFS::unmount(char c) {
	int pos = charToPos(c);	
	PartitionData *pd = pData[pos];
	if (!pd) return 0;
	delete pd;
	pData[pos] = nullptr;
	freeLetters.push(c);
	return 1;
}

char KernelFS::format(char c) {
	int pos = charToPos(c);
	PartitionData *pd = pData[pos];
	if (!pd) return 0;	
	return pd->format();
}

char KernelFS::readRootDir(char c, EntryNum n, Directory& d) {
	int pos = charToPos(c);
	PartitionData *pd = pData[pos];
	if (!pd) return 0;

	return pd->readRootDir(n, d);
}

char KernelFS::doesExist(char* f) {
	char part;
	char *filename = new char[strlen(f) - 2];

	extractPath(f, part, filename);
	int pos = charToPos(part);
	PartitionData *pd = pData[pos];
	if (!pd) {
		delete[]filename;
		return 0;
	}
	char rtn = pd->doesExist(filename);
	delete[]filename;
	return rtn;
}

File* KernelFS::open(char* f, char mode) {
	char part;
	char *filename = new char[strlen(f) - 2];

	extractPath(f, part, filename);
	int pos = charToPos(part);
	PartitionData *pd = pData[pos];
	if (!pd) {
		delete[]filename;
		return nullptr;
	}

	File *file = nullptr;
	KernelFile *kFile = pd->open(filename, mode);
	if (kFile != nullptr) {
		file = new File();
		file->myImpl = kFile;
	}
	delete[]filename;
	return file;
}

char KernelFS::deleteFile(char* f) {
	char part;
	char *filename = new char[strlen(f) + 1];
	strcpy(filename, f);
	extractPath(f, part, filename);

	int pos = charToPos(part);
	PartitionData* pd = pData[pos];

	if (!pd) return 0;

	char rtn = pd->deleteFile(filename);
	
	delete[]filename;
	return rtn;
}

KernelFS::KernelFS() {
	for (int i = 25; i >= 0; i--)
		freeLetters.push(posToChar(i));
	for (int i = 0; i < 26; i++)
		pData[i] = nullptr;
}

void KernelFS::extractPath(char* f, char& part, char* filename) {
	part = f[0];
	memcpy(filename, f + 3, strlen(f) - 2);
}