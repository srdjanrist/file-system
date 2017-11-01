#include "Data.h"



PartitionData::PartitionData(Partition *p) {
	myPartition = p;
	numOpenedFiles = 0;
	clusterNumber = p->getNumOfClusters();
	this->cache = new Cache(myPartition);

	mutex = CreateSemaphore(NULL, 1, 1, NULL);
	unmountSem = CreateSemaphore(NULL, 0, 1, NULL);
	formatSem = CreateSemaphore(NULL, 0, 1, NULL);

	formatting = false;

	dirtyBitVector = false;
	loadBitVector(p);
}

PartitionData::~PartitionData() {

	wait(mutex);
	while (numOpenedFiles) {
		signal(mutex);
		wait(unmountSem);
		wait(mutex);
	}
	if (dirtyBitVector) {
		saveBitVector();
	}
	cache->flushCache();
	delete cache;
	signal(mutex);		//////mozda?
	CloseHandle(mutex);
	CloseHandle(unmountSem);
	CloseHandle(formatSem);
}

char PartitionData::format() {
	// privremeno
	char * zeros = new char[2048];
	memset(zeros, 0x00, 2048);

	wait(mutex);
	if (numOpenedFiles) {
		formatting = true;
		signal(mutex);
		wait(formatSem);
		wait(mutex);
	}
	for (int i = 2; i < myPartition->getNumOfClusters(); i++)
		//cache->writeCluster(i, zeros);
		myPartition->writeCluster(i, zeros);
	delete[]zeros;
	//////potrebno dodati ako je otvoren neki fajl da se ne formatira

	bitVector.erase(bitVector.begin(), bitVector.end());
	wait(setSem);
	freeClusters.erase(freeClusters.begin(), freeClusters.end());
	signal(setSem);

	bitVector.push_back(false);
	bitVector.push_back(false);
	wait(setSem);
	for (ClusterNo i = 2; i < clusterNumber; i++) {
		bitVector.push_back(true);
		freeClusters.insert(i);
	}
	signal(setSem);
	char * buffer = new char[ClusterSize];
	memset(buffer, 0x00, ClusterSize);
	cache->writeCluster(1, buffer);

	dirtyBitVector = true;
	numFiles = 0;
	formatting = false;
	signal(mutex);
	return 1;
}


void PartitionData::loadBitVector(Partition *p) {
	char *buffer = new char[ClusterSize];
	if (!p->readCluster(0, buffer)) {
		cout << "Unable to read from disk. Exiting";
		exit(1);
	}
	char mask = 0x01;
	bool val = false;

	wait(setSem);
	for (ClusterNo i = 0; i < clusterNumber; i++) {
		val = (buffer[i / 8] & mask) ? true : false;
		if (val) 
			freeClusters.insert(i);

		bitVector.push_back(val);
		mask <<= 1;
		if (!mask) mask = 0x01;
	}
	signal(setSem);
	delete[]buffer;
}

void PartitionData::saveBitVector() {
	char *buffer = new char[ClusterSize];
	memset(buffer, 0x00, ClusterSize);
	for (ClusterNo i = 0; i < clusterNumber; i++) {
		buffer[i / 8] = buffer[i / 8] | (bitVector[i] << (i % 8));
	}
	myPartition->writeCluster(0, buffer);
	delete[]buffer;
}

char PartitionData::doesExist(char *f) {
	unsigned dummy;
	return doesExist(f, dummy);
}


ClusterNo PartitionData::getClusterWithEntry(unsigned entry) {
	bool doubleIndexing = false;
	unsigned indexEntry = entry / FILES_PER_CLUSTER;
	if (indexEntry >= ENTRIES_PER_CLUSTER / 2) doubleIndexing = true;
	ClusterNo cluster = 0;
	if (!doubleIndexing) {
		cache->readBlock(1, (char*)&cluster, indexEntry * sizeof(ClusterNo), sizeof(ClusterNo));
	}
	else{
		unsigned tempEntry = entry - ENTRIES_PER_CLUSTER / 2;
		unsigned entryl2 = tempEntry / (ENTRIES_PER_CLUSTER * FILES_PER_CLUSTER) + ENTRIES_PER_CLUSTER / 2,
			entryl1 = tempEntry / ENTRIES_PER_CLUSTER;
		cache->readBlock(1, (char*)&cluster, entryl2 * sizeof(ClusterNo), sizeof(ClusterNo));
		if (cluster) {
			cache->readBlock(1, (char*)&cluster, entryl1 * sizeof(ClusterNo), sizeof(ClusterNo));
		}
	}
	return cluster;
}

char PartitionData::doesExist(char *f, unsigned& d) {
	d = ~0;
	char * file = new char[FNAMELEN + FEXTLEN + 1];
	unsigned size = 0;
	while (*f != '.') 
		file[size++] = *f++;
	f++;
	while (size < FNAMELEN) file[size++] = ' ';
	while (*f != 0)
		file[size++] = *f++;
	while (size < FNAMELEN + FEXTLEN)
		file[size++] = ' ';
	file[size] = 0;
	
	char * bufferCache = nullptr;
	char * buffer = new char[ClusterSize];

	char * testBlock = new char[sizeof(Entry)];
	memset(testBlock, 0x00, sizeof(Entry));

	ClusterNo cluster; 
	unsigned long entry = 0;
	bool endSearch = false, found = false;

	while (!endSearch) {
		cluster = getClusterWithEntry(entry);

		if (!cluster) {
			endSearch = true;
			found = false;
		}
		else {
			bufferCache = cache->readCluster(cluster);
			memcpy(buffer, bufferCache, ClusterSize);
			unsigned fileEntry = 0;
			char * fileInfo = new char[sizeof(Entry)];
			while (fileEntry < FILES_PER_CLUSTER) {
				memcpy(fileInfo, buffer + fileEntry * sizeof(Entry), sizeof(Entry));
				if (!memcmp(testBlock, fileInfo, sizeof(Entry))) {
					endSearch = true;
					break;
				}
				if (fileInfo[0] == 0x00) {
					fileEntry++;
					entry++;
					continue;
				}
				if (!memcmp(fileInfo, file, FNAMELEN + FEXTLEN)) {
					d = entry;
					endSearch = true;
					found = true;
					break;
				}
				fileEntry++;
				entry++;
			}
			delete[]fileInfo;
		}			
	}

	delete[]testBlock;
	delete[]file;
	delete[]buffer;
	return found ? 1 : 0;
}

int PartitionData::readRootDir(EntryNum n, Directory &d) {
	unsigned val = 0;

	unsigned fileEntry = n % FILES_PER_CLUSTER;

	char * bufferCache = nullptr;
	char * buffer = new char[ClusterSize];

	char * testBlock = new char[sizeof(Entry)];
	memset(testBlock, 0x00, sizeof(Entry));

	bool doubleIndexing = false;
	
	bool endSearch = false;
	ClusterNo cluster;

	while (!endSearch) {
		cluster = getClusterWithEntry(n);
		if (!cluster) {
			endSearch = true;
			break;
		}

		bufferCache = cache->readCluster(cluster);
		memcpy(buffer, bufferCache, ClusterSize);

		fileEntry %= FILES_PER_CLUSTER;
		char * fileInfo = new char[sizeof(Entry)];
		while (fileEntry < FILES_PER_CLUSTER) {
			memcpy(fileInfo, buffer + fileEntry * sizeof(Entry), sizeof(Entry));
			if (!memcmp(testBlock, fileInfo, sizeof(Entry))) {
				endSearch = true;
				break;
			}
			if (fileInfo[0] == 0x00) {
				fileEntry++; continue;
			}
			if (val < 64) {
				Entry e;
				memcpy(&e, fileInfo, FNAMELEN + FEXTLEN);
				e.reserved = 0;
				e.indexCluster = *(long*)(buffer + fileEntry * sizeof(Entry) + FNAMELEN + FEXTLEN + 1);
				e.size = *(long*)(buffer + fileEntry * sizeof(Entry) + FNAMELEN + FEXTLEN + sizeof(long) + 1);
				d[val++] = e;
			}
			else {
				val++;
				endSearch = true;
				break;
			}
			fileEntry++;
		}
		delete[]fileInfo;
	}

	return val;
}

KernelFile* PartitionData::open(char *name, char mode) {
	wait(mutex);
	if (formatting) {
		cout << "FORMATIRA SE. NO OPENING";
		signal(formatSem);
		signal(mutex);
		return nullptr;
	}

	unsigned entry = ~0;
	char existance = doesExist(name, entry);
	
	try {
		if (existance == 0 && (mode == 'r' || mode == 'a')) throw  ENonExistentFile();
	}
	catch (ENonExistentFile e) {
		cout << e.what() << endl;
		signal(mutex);
		return nullptr;
	}
	
	bool recentlyCreated = false;

	string filename = createString(name);

	bool wasOpened = false;
	if (existance == 0) {
		entry = createFile(name); 
		if (entry == ~0) {
			cout << "Can't create file!";
			signal(mutex);
			return nullptr;
		}
		recentlyCreated = true;
	}
	else if (existance == 1 && mode == 'w') {
		wasOpened = (openedFiles.find(filename) == openedFiles.end()) ? false : true;
	}

	FCB * pfcb = nullptr;
	if (recentlyCreated) {	
		pfcb = new FCB();
		openedFiles.insert(pair<string, FCB*>(filename, pfcb));
		pfcb->cnt++;
		numOpenedFiles++;
		BytesCnt size;
		ClusterNo index;
		getInfoForEntry(entry, size, index);
		pfcb->size = size;
		pfcb->index = index;
		KernelFile *file = new KernelFile(mode, this, entry, size, index, filename);
		signal(mutex);
		AcquireSRWLockExclusive(&pfcb->srwlock);
		return file;
	}
	else if (existance == 1) {
		map<string, FCB*>::iterator it = openedFiles.find(filename);

		if (it == openedFiles.end()) {
			pfcb = new FCB();
			openedFiles.insert(pair<string, FCB*>(filename, pfcb));
		}
		else {
			pfcb = it->second;
		}
		pfcb->cnt++;
		numOpenedFiles++;
		if (mode == 'r') {
			signal(mutex);
			AcquireSRWLockShared(&(pfcb->srwlock));
		}
		else if (mode == 'a') {
			signal(mutex);
			AcquireSRWLockExclusive(&(pfcb->srwlock));
		}
		else if (mode == 'w') {
			signal(mutex);
			AcquireSRWLockExclusive(&(pfcb->srwlock));
		}
		wait(mutex);
		if (wasOpened)
			truncateFile(entry);
		BytesCnt size;
		ClusterNo index;
		getInfoForEntry(entry, size, index);
		pfcb->size = size;
		pfcb->index = index;
		KernelFile *file = new KernelFile(mode, this, entry, size, index, filename);
		signal(mutex);
		return file;
	}

	return nullptr;
}

void PartitionData::deleteIndex(ClusterNo index) {
	//wait(mutex);
	char * zeros = new char[ClusterSize];
	memset(zeros, 0x00, ClusterSize);

	char * bufferCache = cache->readCluster(index);
	char * buffer = new char[ClusterSize];

	memcpy(buffer, bufferCache, ClusterSize);
	cache->writeCluster(index, zeros);

	char * bufferl2 = new char[ClusterSize];

	unsigned entry = 0;
	bool doubleIndexing = false;

	bool endClearing = false;

	while (!endClearing) {
		if (entry >= ENTRIES_PER_CLUSTER / 2) doubleIndexing = true;

		ClusterNo ind = *(ClusterNo*)(buffer + entry * sizeof(ClusterNo));
		entry++;
		if (!ind) break;

		if (doubleIndexing){
			bufferCache = cache->readCluster(ind );
			memcpy(bufferl2, bufferCache, ClusterSize);
			cache->writeCluster(ind, zeros);
			unsigned entry2 = 0;
			do {
				ClusterNo ind2 = *(ClusterNo*)(bufferl2 + entry2 * sizeof(ClusterNo));
				if (!ind2) {
					endClearing = true;
					break;
				}
				cache->writeCluster(ind2, zeros);
				freeCluster(ind2);
				entry2++;
			} while (entry2 < ENTRIES_PER_CLUSTER);			
		}

		cache->writeCluster(ind, zeros);
		freeCluster(ind);
	}
	////////
	//signal(mutex);
	delete[]zeros;
	delete[]bufferl2;
	delete[]buffer;
}

void PartitionData::truncateFile(unsigned entry) {
	ClusterNo cluster = getClusterWithEntry(entry);

	char * buffer = new char[sizeof(Entry)];

	entry %= ENTRIES_PER_CLUSTER;

	cache->readBlock(cluster, buffer, entry * sizeof(Entry), sizeof(Entry));

	ClusterNo index = *(ClusterNo*)(buffer + FNAMELEN + FEXTLEN + 1);
	BytesCnt size = *(BytesCnt*)(buffer + FNAMELEN + FEXTLEN + sizeof(ClusterNo) + 1);

	if (index)
		deleteIndex(index);
	
	freeCluster(index);

	memset(buffer + FNAMELEN + FEXTLEN + 1, 0x00, sizeof(BytesCnt) + sizeof(ClusterNo));

	cache->writeBlock(cluster, buffer, entry * sizeof(Entry), sizeof(Entry));
	
	delete[]buffer;
}

void PartitionData::truncateFile(ClusterNo indexCluster, ClusterNo start) {
	bool doubleIndexing = false;
	ClusterNo entry = start;
	
	char * buffer = new char[ClusterSize];
	char * bufferCache = cache->readCluster(indexCluster);
	memcpy(buffer, bufferCache, ClusterSize);

	char * bufferl2 = nullptr;

	char * zeros = new char[ClusterSize];
	memset(zeros, 0x00, ClusterSize);


	bool endClearing = false;
	while (!endClearing) {
		if (entry >= ENTRIES_PER_CLUSTER / 2) doubleIndexing = true;
		ClusterNo ind = *(ClusterNo*)(buffer + entry * sizeof(ClusterNo));
		
		if (!ind) break;

		memset(buffer + entry * sizeof(ClusterNo), 0x00, sizeof(ClusterNo));
		entry++;
		if (doubleIndexing) {
			if (!bufferl2)
				bufferl2 = new char[ClusterSize];
			bufferCache = cache->readCluster(ind);
			memcpy(bufferl2, bufferCache, ClusterSize);
			cache->writeCluster(ind, zeros);
			unsigned entry2 = 0;
			do {
				ClusterNo ind2 = *(ClusterNo*)(bufferl2 + entry2 * sizeof(ClusterNo));
				if (!ind2) {
					endClearing = true;
					break;
				}
				cache->writeCluster(ind2, zeros);
				freeCluster(ind2);
				entry2++;
			} while (entry2 < ENTRIES_PER_CLUSTER);
		}

		cache->writeCluster(ind, zeros);
		freeCluster(ind);
	}
	cache->writeCluster(indexCluster, buffer);
	delete[]buffer;
	delete[]zeros;
	if (bufferl2) delete[]bufferl2;
}

unsigned PartitionData::findFreeEntry() {

	char * bufferCache = nullptr;
	char * buffer = new char[ClusterSize];

	ClusterNo cluster;
	unsigned long entry = 0;
	bool endSearch = false, found = false;

	while (!endSearch) {
		cluster = getClusterWithEntry(entry);

		if (!cluster) {
			cluster = allocateIndex();
		}
		if (!cluster) {
			endSearch = true;
			found = false;
		}
		else {
			bufferCache = cache->readCluster(cluster);
			memcpy(buffer, bufferCache, ClusterSize);
			unsigned fileEntry = 0;
			char fileInfo;
			while (fileEntry < FILES_PER_CLUSTER) {
				memcpy(&fileInfo, buffer + fileEntry * sizeof(Entry), sizeof(char));
				if (fileInfo == 0x00) {
					endSearch = true;
					found = true;
					break;
				}
				fileEntry++;
				entry++;
			}
		}
	}
	delete[]buffer;
	
	return found ? entry : -1;
}
long PartitionData::createFile(char* name) {
	long entry = findFreeEntry();
	if (entry == -1) {
		cout << "Can't create file. No free entries";
		return -1;
	}

	/*ClusterNo index = getFreeCluster();
	if (!index) {
		cout << "Can't create file. No free clusters";
		return -1;
	}*/
	
	ClusterNo index = 0;
	Entry e;
	memset(e.name, ' ', FNAMELEN);
	memset(e.ext, ' ', FEXTLEN);
	extractNameExt(name, e.name, e.ext);
	e.reserved = 0;
	e.size = 0;
	e.indexCluster = index;

	char * indexEntry = new char[sizeof(Entry)];
	memcpy(indexEntry, &e.name, sizeof(Entry));

	ClusterNo cluster = getClusterWithEntry(entry);
/*	char * bufferCache = nullptr;
	char * buffer = new char[ClusterSize];
	bufferCache = cache->readCluster(cluster);
	memcpy(buffer, bufferCache, ClusterSize);
	memcpy(buffer + (entry % FILES_PER_CLUSTER) * sizeof(Entry), indexEntry, sizeof(Entry));*/
	
	cache->writeBlock(cluster, indexEntry, (entry % FILES_PER_CLUSTER) * sizeof(Entry), sizeof(Entry));
	
	/*cache->writeCluster(cluster, buffer);

	memset(buffer, 0x00, ClusterSize);
	cache->writeCluster(index, buffer);*/

//	delete[]buffer;
	delete[]indexEntry;
	return entry;
}

void PartitionData::extractNameExt(char* str, char* name, char* ext) {
	while (*str != '.')
		*name++ = *str++;
	str++;
	while (*str != 0)
		*ext++ = *str++;
}

ClusterNo PartitionData::getFreeCluster() {
	wait(setSem);
	if (freeClusters.empty()) {
		signal(setSem);
		return 0;
	}
	/*unordered_*/set<ClusterNo>::iterator it = freeClusters.begin();
	ClusterNo rtn = *it;
	freeClusters.erase(it);
	signal(setSem);
	bitVector[rtn] = false;
	dirtyBitVector = true;
	return rtn;

	/*for (int i = 2; i < clusterNumber; i++)
		if (bitVector[i]) {
			bitVector[i] = false;
			dirtyBitVector = true;
			return i;
		}*/	
	//return 0;
}
char PartitionData::deleteFile(char* file) {
	wait(mutex);
	unsigned entry = ~0;
	string name = createString(file);
	char existance = doesExist(file, entry);
	bool opened = (openedFiles.find(name) == openedFiles.end()) ? false : true;
	if (existance == 0 || opened) {
		signal(mutex);
		return 0;
	}
	deleteFile(entry);
	signal(mutex);
	return 1;
}

void PartitionData::deleteFile(unsigned entry) {
	ClusterNo index;
	
	ClusterNo cluster = getClusterWithEntry(entry);
	char * str = new char[sizeof(Entry)];
	entry %= FILES_PER_CLUSTER;
	cache->readBlock(cluster, str, entry * sizeof(Entry), sizeof(Entry));

	index = *(ClusterNo*)(str + FNAMELEN + FEXTLEN + 1);

	deleteIndex(index);
	
	freeCluster(index);

	memset(str, 0x00, 1);
	memset(str + FNAMELEN + FEXTLEN + 1, 0x00, sizeof(ClusterNo));

	cache->writeBlock(cluster, str, entry * sizeof(Entry), sizeof(Entry));
	
	delete[]str;	
}

void PartitionData::closeFile(KernelFile* file) {
	long entry = file->entry;
	BytesCnt newSize = file->size;
	char mode = file->mode;
	string name = file->name;
	ClusterNo newIndex = file->index;
	
	wait(mutex);
	map<string, FCB*>::iterator it = openedFiles.find(name);
	if (it == openedFiles.end()) {
		signal(mutex);
		return;
	}
	FCB* pfcb = it->second;
	if (mode == 'r') {
		pfcb->cnt--;
		numOpenedFiles--;
		ReleaseSRWLockShared(&pfcb->srwlock);
		if (!pfcb->cnt) openedFiles.erase(name);
	}
	else if (mode == 'a' || mode == 'w') {
		ClusterNo clusterWithEntry = getClusterWithEntry(entry);
		if (newSize != pfcb->size) {
			BytesCnt writeFrom = (entry % FILES_PER_CLUSTER) * sizeof(Entry) + FNAMELEN + FEXTLEN + sizeof(ClusterNo) + 1;
			cache->writeBlock(clusterWithEntry, (char*)&newSize, writeFrom, sizeof(BytesCnt));
		}
		if (newIndex != pfcb->index) {
			BytesCnt writeFrom = (entry % FILES_PER_CLUSTER) * sizeof(Entry) + FNAMELEN + FEXTLEN + 1;
			cache->writeBlock(clusterWithEntry, (char*)&newIndex, writeFrom, sizeof(ClusterNo));
		}
		pfcb->cnt--; 
		if (!pfcb->cnt) openedFiles.erase(name);
		numOpenedFiles--;
		ReleaseSRWLockExclusive(&pfcb->srwlock);
	}
	if (!numOpenedFiles)
		signal(unmountSem);
	if (formatting)
		signal(formatSem);
	signal(mutex);
}

void PartitionData::getInfoForEntry(unsigned n, BytesCnt &size, ClusterNo &ind) {
	unsigned indexEntry = n / FILES_PER_CLUSTER;
	unsigned fileEntry = n % FILES_PER_CLUSTER;

	char * indexCache = cache->readCluster(1);
	char * index = new char[ClusterSize];
	memcpy(index, indexCache, ClusterSize);

	char * bufferCache = nullptr;
	
	char * buffer = new char[ClusterSize];

	ClusterNo cluster = getClusterWithEntry(n);

	bufferCache = cache->readCluster(cluster);
	memcpy(buffer, bufferCache, ClusterSize);

	ind = *(ClusterNo*)(buffer + fileEntry * sizeof(Entry) + FNAMELEN + FEXTLEN + 1);
	size = *(BytesCnt*)(buffer + fileEntry * sizeof(Entry) + FNAMELEN + FEXTLEN + sizeof(ClusterNo) + 1); 
	

	delete[] index;
	delete[] buffer;
}

ClusterNo PartitionData::allocateIndex() {
	
	char * indexCache = cache->readCluster(1);
	char * index = new char[ClusterSize];
	memcpy(index, indexCache, ClusterSize);

	char * bufferCache = nullptr;
	char * buffer = new char[ClusterSize];


	ClusterNo entry = 0;
	ClusterNo cluster = 0;

	bool endSearch = false;
	bool doubleIndexing = false;
	bool found = false;

	while (!endSearch) {
		if (entry == ENTRIES_PER_CLUSTER / 2) doubleIndexing = true;

		if (!doubleIndexing) {
			cluster = *(ClusterNo*)(index + entry * sizeof(ClusterNo));
			if (!cluster) {
				cluster = getFreeCluster();
				if (!cluster) {
					cout << "Can't allocate free clusters";
					endSearch = true;
					break;
				}
				endSearch = true;
				found = true;
				cache->writeBlock(1, (char*)&cluster, entry * sizeof(ClusterNo), sizeof(ClusterNo));
				break;
			}
			entry++;
		}
		else {
			ClusterNo tempEntry = entry - ENTRIES_PER_CLUSTER / 2;
			ClusterNo entryl2 = tempEntry / ENTRIES_PER_CLUSTER + ENTRIES_PER_CLUSTER / 2,
				entryl1 = tempEntry % ENTRIES_PER_CLUSTER;

			cluster = *(ClusterNo*)(index + entryl2 * sizeof(ClusterNo));
			if (cluster) {
				bufferCache = cache->readCluster(cluster);
				memcpy(buffer, bufferCache, ClusterSize);
				while (entryl1 < ENTRIES_PER_CLUSTER) {
					ClusterNo cluster2 = *(ClusterNo*)(buffer + entryl1 * sizeof(ClusterNo));
					if (cluster2) {
						entryl1++;
						entry++;
						continue;
					}
					cluster2 = getFreeCluster();

					if (!cluster2) {
						cout << "Can't allocate clusters";
						return 0;
					}

					cache->writeBlock(cluster, (char*)&cluster2, entryl1 * sizeof(ClusterNo), sizeof(ClusterNo));
					endSearch = true;
					found = true;
					cluster = cluster2;
					break;
				}
			}
			else {
				cluster = getFreeCluster();
				if (!cluster) {
					cout << "Can't allocate cluster";
					return 0;
				}
				cache->writeBlock(1, (char*)&cluster, entryl1 * sizeof(ClusterNo), sizeof(ClusterNo));

				ClusterNo cluster2 = getFreeCluster();
				if (!cluster2) {
					cout << "Can't allocate cluster";
					endSearch = true;
					break;
				}
				cache->writeBlock(cluster, (char*)&cluster2, 0, sizeof(ClusterNo) - 1);
				endSearch = true;
				found = true;
				cluster = cluster2;
			}
		}
	}

	delete[]buffer;
	delete[]index;

	return found ? cluster : 0;
}

string PartitionData::createString(char * str) {
	int size = 0;
	int pos = 0;
	string rtn;
	while (str[pos] != '.') {
		rtn.append(1, str[pos]);
		pos++;
		size++;
	}
	while (size++ < FNAMELEN)
		rtn.append(1, ' ');
	rtn.append(1, '.');
	size++;
	pos++;
	while (str[pos] != 0x00) {
		rtn.append(1, str[pos]);
		pos++;
		size++;
	}
	while (size++ < FNAMELEN + FEXTLEN + 1)
		rtn.append(1, ' ');

	return rtn;
}

void PartitionData::freeCluster(ClusterNo cluster) {
	bitVector[cluster] = true;
	dirtyBitVector = true;
	wait(setSem);
	freeClusters.insert(cluster);
	signal(setSem);
}

ClusterNo PartitionData::getNewIndex() {
	return getFreeCluster();
}