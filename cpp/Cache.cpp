#include "Cache.h"

#include <iostream>
#include <stdlib.h>
using namespace std;


unsigned long Cache::mask = CACHE_SIZE-1;

Cache::Cache(Partition *p) {
	for (int i = 0; i < CACHE_SIZE; i++) {
		mutex[i] = CreateSemaphore(NULL, 1, 32, NULL);
		dataMemory[i] = nullptr;
		dirty[i] = false;
		valid[i] = false;
		tag[i].cluster = -1;
	}
	this->partition = p;
}

Cache::~Cache() {
//	flushCache();
	for (int i = 0; i < CACHE_SIZE; i++) {
		CloseHandle(mutex[i]);
		if (valid[i]) {
			valid[i] = false;
			delete[]dataMemory[i];
		}
	}
}
char* Cache::readCluster(ClusterNo cluster){

	unsigned long entry = cluster & mask;
	char * rtn = nullptr;

	wait(mutex[entry]);
	
	if (valid[entry] && tag[entry].cluster == cluster) 
		rtn = dataMemory[entry];
	else {
		if (valid[entry]) {
			if (dirty[entry])
				partition->writeCluster(tag[entry].cluster, dataMemory[entry]);
		}
		else
			dataMemory[entry] = new char[ClusterSize];

		tag[entry].cluster = cluster;
		partition->readCluster(cluster, dataMemory[entry]);
		rtn = dataMemory[entry];
		valid[entry] = true;
		dirty[entry] = false;
	}
	signal(mutex[entry]);
	return rtn;
}

void Cache::writeCluster(ClusterNo cluster, char *buffer) {

	unsigned long entry = cluster & mask;

	wait(mutex[entry]);

	if (valid[entry] && tag[entry].cluster == cluster) {
		memcpy(dataMemory[entry], buffer, ClusterSize);
		dirty[entry] = true;
	} else {
		if (valid[entry]) {
			if (dirty[entry])
				partition->writeCluster(tag[entry].cluster, dataMemory[entry]);
		}
		else
			dataMemory[entry] = new char[ClusterSize];
			
		tag[entry].cluster = cluster;
		memcpy(dataMemory[entry], buffer, ClusterSize);

		valid[entry] = true;
		dirty[entry] = true;
	}
	signal(mutex[entry]);
}

void Cache::readBlock(ClusterNo cluster, char *buffer, BytesCnt from, BytesCnt cnt) {

	unsigned long entry = cluster & mask;

	wait(mutex[entry]);

	if (valid[entry] && tag[entry].cluster == cluster) {
		memcpy(buffer, dataMemory[entry] + from, cnt);
	}
	else {
		if (valid[entry]) {
			if (dirty[entry])
				partition->writeCluster(tag[entry].cluster, dataMemory[entry]);
		}
		else
			dataMemory[entry] = new char[ClusterSize];

		tag[entry].cluster = cluster;
		partition->readCluster(cluster, dataMemory[entry]);
		memcpy(buffer, dataMemory[entry] + from, cnt);

		valid[entry] = true;
	}
	signal(mutex[entry]);
}

void Cache::writeBlock(ClusterNo cluster, char *buffer, BytesCnt from, BytesCnt cnt) {

	unsigned long entry = cluster & mask;

	wait(mutex[entry]);

	if (valid[entry] && tag[entry].cluster == cluster) {
		memcpy(dataMemory[entry] + from, buffer, cnt);
		dirty[entry] = true;
	}
	else {
		if (valid[entry]) {
			if (dirty[entry])
				partition->writeCluster(tag[entry].cluster, dataMemory[entry]);
		}
		else 
			dataMemory[entry] = new char[ClusterSize];

		tag[entry].cluster = cluster;
		partition->readCluster(cluster, dataMemory[entry]);
		memcpy(dataMemory[entry] + from, buffer, cnt);

		valid[entry] = true;
		dirty[entry] = true;
	}
	signal(mutex[entry]);
}

void Cache::flushCache(){
	for (int i = 0; i < CACHE_SIZE; i++) {
		wait(mutex[i]);
		if (valid[i] && dirty[i]) {
			partition->writeCluster(tag[i].cluster, dataMemory[i]);
			dirty[i] = false;
			valid[i] = false;
			delete[]dataMemory[i];
		}
		signal(mutex[i]);
	}
}

void Cache::dumpCache() {
/*	flushCache();
	for (int i = 0; i < CACHE_SIZE; i++)
		if (valid[i]) {
			valid[i] = false;
			delete[]dataMemory[i];
		}*/
}
