
#include "Buffer.h"
#include <malloc.h>
#include "string.h"


Buffer::Buffer()
{
	Initialize();
}


Buffer::~Buffer()
{
	if (_ptr) {
		free(_ptr);
	}
}

void Buffer::Initialize()
{
	_length = 2048;
	_ptr = (unsigned char*)malloc(_length);
}

void Buffer::MoveDataToBegin()
{
	size_t offset = _writePos - _readPos;
	if (_readPos && offset > 0) {
		memmove(_ptr, _ptr + _readPos, offset);
		_readPos = 0;
		_writePos = offset;
	}	
}

void Buffer::AppendData(void *data, size_t len)
{
	if (len + _writePos > _length)
	{
		MoveDataToBegin();
	}
	while (len + _writePos > _length)
	{
		_ptr = (unsigned char*)realloc(_ptr, _length * 2);
		_length *= 2;
	}

	memcpy(_ptr + _writePos, data, len);
	_writePos += len;
}

void Buffer::SkipData(size_t len)
{
	_readPos += len;
	if (_readPos >= _writePos)
	{
		_readPos = _writePos = 0;
	}
}

void* Buffer::GetReadPtr()
{
	return _ptr + _readPos;
}

size_t Buffer::GetDataLength()
{
	return _writePos - _readPos;
}
