#include "KernelFile.h"
#include <iostream>
using namespace std;

KernelFile::KernelFile(char m, PartitionData *pd, unsigned entry, BytesCnt size, ClusterNo index, string name) {
	this->mode = m;
	this->partitionData = pd;
	this->entry = entry;
	this->size = size;
	this->index = index;
	this->name = name;
	this->reg.valid = false;
	this->reg.dirty = false;

	if (m == 'w' || m == 'r')
		cursor = 0;
	else if (m == 'a')
		cursor = size;
}

KernelFile::~KernelFile() {

}

char KernelFile::readFromCluster(ClusterNo index, BytesCnt byte) {
	char * buffer = partitionData->cache->readCluster(index);
	return buffer[byte];
}

char* KernelFile::getCluster(ClusterNo cluster) {
	/*char * buffer = Cache::readCluster(cluster, partitionData->myPartition);
	return buffer;*/
	return 0;
}

ClusterNo KernelFile::readEntryInIndex(ClusterNo clusterToWrite) {
	ClusterNo cluster = 0;
	char buf[4];
	partitionData->cache->readBlock(index, buf, clusterToWrite * sizeof(ClusterNo), sizeof(ClusterNo));
	cluster = *(ClusterNo*)buf;
	return cluster;
}

ClusterNo KernelFile::readEntryInIndex2(ClusterNo index, ClusterNo cluster) {
	ClusterNo rtn = 0;
	char buf[4];
	partitionData->cache->readBlock(index, buf, cluster * sizeof(ClusterNo), sizeof(ClusterNo));
	rtn = *(ClusterNo*)buf;
	return rtn;
}

void KernelFile::writeEntryToIndex(ClusterNo e, ClusterNo w) {
	char toWrite[4];
	memcpy(toWrite, &w, sizeof(ClusterNo));
	partitionData->cache->writeBlock(index, toWrite, e*sizeof(ClusterNo), sizeof(ClusterNo));
}

void KernelFile::writeEntryToIndex2(ClusterNo l2cluster, ClusterNo l1entry, ClusterNo cluster) {
	char toWrite[4];
	memcpy(toWrite, &cluster, sizeof(ClusterNo));
	partitionData->cache->writeBlock(l2cluster, toWrite, l1entry * sizeof(ClusterNo), sizeof(ClusterNo));
}

char KernelFile::write(BytesCnt cnt, char* buffer) {
	if (cnt == 0) return 1;
	if (mode == 'r') return 0;
	if (size == MAX_FILE_SIZE) return 0;
	if (!index) {
		index = partitionData->getNewIndex();
	}
	if (!index) return 0;

	bool endWrite = false;

	BytesCnt start = 0;
	while (size < MAX_FILE_SIZE && !endWrite) {		
		ClusterNo entry = cursor / ClusterSize;
		ClusterNo cluster;
		ClusterNo oldEntry = entry;

		if (reg.valid && reg.oldEntry == entry) {
			cluster = reg.cluster;
		}
		else {
			if (reg.dirty) {
				saveLocalCluster();
			}
			if (entry < ENTRIES_PER_CLUSTER / 2) {
				cluster = readEntryInIndex(entry);
				if (!cluster) {
					cluster = partitionData->getFreeCluster();
					if (!cluster) {
						cout << "No free clusters. Can't write";
						return 0;
					}
					writeEntryToIndex(entry, cluster);
				}
			}
			else {				
				long halfEntries = ENTRIES_PER_CLUSTER / 2;
				entry -= halfEntries;
				ClusterNo l2entry = entry / ENTRIES_PER_CLUSTER;
				ClusterNo l2cluster = readEntryInIndex(l2entry + halfEntries);
				if (!l2cluster) {
					l2cluster = partitionData->getFreeCluster();
					if (!l2cluster) {
						cout << "No free clusters. Can't write";
						return 0;
					}
					writeEntryToIndex(l2entry + halfEntries, l2cluster);
				}
				ClusterNo l1entry = entry % ENTRIES_PER_CLUSTER;
				cluster = readEntryInIndex2(l2cluster, l1entry);
				if (!cluster) {
					cluster = partitionData->getFreeCluster();
					if (!cluster) {
						cout << "No free clusters. Can't write";
						//potrebno oslobiditi i klaster prethodon dodijeljen ako ne sadrzi ni jedan klaster 2. nivoa
						return 0;
					}
					//		cout << "  " << l2cluster <<  " " << l1entry << " " << cluster <<  endl;
					writeEntryToIndex2(l2cluster, l1entry, cluster);
				}
			}
			reg.cluster = cluster;
			loadLocalCluster();
		}

		BytesCnt from = cursor % ClusterSize;
		BytesCnt to = from + cnt;
		if (to > ClusterSize) {
			cnt = to - ClusterSize;
			to = ClusterSize;
		}
		else cnt = 0;

		if (reg.valid) 
			writeBlockLocally(buffer + start, from, to - from);
		else 
			partitionData->cache->writeBlock(cluster, buffer + start, from, to - from);

		start = start + to - from;
		cursor = cursor + to - from;
		size = (size < cursor) ? cursor : size;

		reg.oldEntry = oldEntry;

	//	cout << size << endl;
		if (!cnt) return 1;
	}
			
	return 0;
}

BytesCnt KernelFile::read(BytesCnt cnt, char* buffer) {
	if (cnt == 0) return 0;
	if (size == 0) return 0;
	if (size == cursor) return 0;

	BytesCnt rtn = 0;

	bool endRead = false;
	BytesCnt start = 0;

	BytesCnt end = cursor + cnt;
	if (end > size) {
		cnt = cnt - (end - size);
		end = size;
	}

	if (!cnt) return 0;
	while (!endRead) {
		ClusterNo entry = cursor / ClusterSize;
		ClusterNo oldEntry = entry;
		ClusterNo cluster;

		if (reg.valid && entry == reg.oldEntry)
			cluster = reg.cluster;
		else {
			if (reg.dirty) {
				saveLocalCluster();
			}
			if (entry < ENTRIES_PER_CLUSTER / 2) {
				cluster = readEntryInIndex(entry);				
			}
			else {
				entry -= ENTRIES_PER_CLUSTER / 2;
				ClusterNo l2entry = entry / ENTRIES_PER_CLUSTER;
				ClusterNo l2cluster = readEntryInIndex(l2entry + ENTRIES_PER_CLUSTER / 2);
				ClusterNo l1entry = entry % ENTRIES_PER_CLUSTER;
				cluster = readEntryInIndex2(l2cluster, l1entry);
			}
			reg.cluster = cluster;
			loadLocalCluster();
		}

		BytesCnt from = cursor % ClusterSize;
		BytesCnt to = from + cnt;
		if (to > ClusterSize) {
			cnt = to - ClusterSize;
			to = ClusterSize;
		}
		else cnt = 0;

		if (reg.valid)
			readBlockLocally(buffer + start, from, to - from);
		else 
			partitionData->cache->readBlock(cluster, buffer + start, from, to - from);

		rtn += to - from;
		start = start + to - from; 
		cursor = cursor + to - from;

		reg.oldEntry = oldEntry;

		if (!cnt) return rtn;
	}
	return rtn;
}

void KernelFile::loadLocalCluster() {
	memcpy(reg.buffer, partitionData->cache->readCluster(reg.cluster), ClusterSize);
	reg.valid = true;
	reg.dirty = false;
}

void KernelFile::saveLocalCluster() {
	partitionData->cache->writeCluster(reg.cluster, reg.buffer);
	reg.dirty = false;
}

void KernelFile::readBlockLocally(char*buffer, BytesCnt from, BytesCnt cnt) {
	memcpy(buffer, reg.buffer + from, cnt);
}

void KernelFile::writeBlockLocally(char*buffer, BytesCnt from, BytesCnt cnt) {
	memcpy(reg.buffer + from, buffer, cnt);
	reg.dirty = true;
}

char KernelFile::seek(BytesCnt n) {
	if (n >= 0 && n < size) {
		cursor = n;
		return 1;
	}
	else return 0;
}

BytesCnt KernelFile::filePos() {
	return cursor;
}

char KernelFile::eof() {
	if (cursor == size) return 2;
	return 0;
}

BytesCnt KernelFile::getFileSize() {
	return size;
}

char KernelFile::truncate() {
	if (mode == 'r') return 0;
	if (cursor == size) return 1;
	if (cursor == 0)
		partitionData->truncateFile(entry);
	else {
		ClusterNo startEntry = (cursor - 1) / ClusterSize + 1;
		size = cursor;
		partitionData->truncateFile(index, startEntry);
	}
	return 1;
}

void KernelFile::closeFile() {
	if (reg.dirty)
		saveLocalCluster();
	partitionData->closeFile(this);
}