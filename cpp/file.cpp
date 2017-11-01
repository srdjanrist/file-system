#include "file.h"
#include "KernelFile.h"

File::~File() {
	myImpl->closeFile();
}

char File::write(BytesCnt n, char* buffer) {
	return myImpl ? myImpl->write(n, buffer) : 0;
}

BytesCnt File::read(BytesCnt n, char* buffer) {
	return myImpl ? myImpl->read(n, buffer) : 0;
}

char File::seek(BytesCnt n) {
	return myImpl ? myImpl->seek(n) : 1;
}

BytesCnt File::filePos() {
	return myImpl->filePos();
}

char File::eof() {
	return myImpl ? myImpl->eof() : 1;
}

BytesCnt File::getFileSize() {
	return myImpl ? myImpl->getFileSize() : 0;
}

char File::truncate() {
	return myImpl ? myImpl->truncate() : 0;
}

File::File() {

}

