//
//Copyright(c) 2016. Huan Xia
//
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
//documentation files(the "Software"), to deal in the Software without restriction, including without limitation
//the rights to use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies of the Software,
//and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all copies or substantial portions
//of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL
//THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
//CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//DEALINGS IN THE SOFTWARE.


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
