#ifndef _ssfBaseDataType_h_
#define _ssfBaseDataType_h_

#include <string>
#include <vector>
#include <list>
#include <memory>
#include <tchar.h>

namespace ssf
	{
	// specific size types
	typedef __int8 int8;
	typedef __int16 int16;
	typedef __int32 int32;
	typedef __int64 int64;
	typedef unsigned __int8 uint8;
	typedef unsigned __int16 uint16;
	typedef unsigned __int32 uint32;
	typedef unsigned __int64 uint64;
	typedef float real32;
	typedef double real64;

	// string type
	typedef std::string string;

	// vector & list types
	using std::vector;
	using std::list;
	using std::auto_ptr;

	// exception type
	typedef std::exception exception;
	
	// configuration
	const bool convert_endianness = false; // Intel x86/x64 are little-endian
	}

#include "ssfObject.h"
#include "ssfCountedPointer.h"
#include "ssfCRC32.h"
#include "ssfAttribute.h"

#endif//_ssfBaseDataType_h_