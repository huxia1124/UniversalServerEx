#pragma once

#include "stddef.h"

class Buffer
{
public:
	Buffer();
	~Buffer();

protected:
	unsigned char *_ptr = nullptr;
	size_t _writePos = 0;
	size_t _length = 0;
	size_t _readPos = 0;

protected:
	void Initialize();
	void MoveDataToBegin();

public:
	void AppendData(void *data, size_t len);
	void SkipData(size_t len);
	void* GetReadPtr();
	size_t GetDataLength();

};

