// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CrossCompilerCommon.h: Common functionality between compiler & runtime.
=============================================================================*/

#pragma once

#include "Core.h"

namespace CrossCompiler
{
	enum
	{
		SHADER_STAGE_VERTEX = 0,
		SHADER_STAGE_PIXEL,
		SHADER_STAGE_GEOMETRY,
		SHADER_STAGE_HULL,
		SHADER_STAGE_DOMAIN,
		NUM_NON_COMPUTE_SHADER_STAGES,
		SHADER_STAGE_COMPUTE = NUM_NON_COMPUTE_SHADER_STAGES,
		NUM_SHADER_STAGES,

		PACKED_TYPENAME_HIGHP	= 'h',	// Make sure these enums match hlslcc
		PACKED_TYPENAME_MEDIUMP	= 'm',
		PACKED_TYPENAME_LOWP		= 'l',
		PACKED_TYPENAME_INT		= 'i',
		PACKED_TYPENAME_UINT		= 'u',
		PACKED_TYPENAME_SAMPLER	= 's',
		PACKED_TYPENAME_IMAGE	= 'g',

		PACKED_TYPEINDEX_HIGHP	= 0,
		PACKED_TYPEINDEX_MEDIUMP	= 1,
		PACKED_TYPEINDEX_LOWP	= 2,
		PACKED_TYPEINDEX_INT		= 3,
		PACKED_TYPEINDEX_UINT	= 4,
		PACKED_TYPEINDEX_MAX		= 5,
	};

	static FORCEINLINE uint8 PackedTypeIndexToTypeName(uint8 ArrayType)
	{
		switch (ArrayType)
		{
		case PACKED_TYPEINDEX_HIGHP:	return PACKED_TYPENAME_HIGHP;
		case PACKED_TYPEINDEX_MEDIUMP:	return PACKED_TYPENAME_MEDIUMP;
		case PACKED_TYPEINDEX_LOWP:		return PACKED_TYPENAME_LOWP;
		case PACKED_TYPEINDEX_INT:		return PACKED_TYPENAME_INT;
		case PACKED_TYPEINDEX_UINT:		return PACKED_TYPENAME_UINT;
		}
		check(0);
		return 0;
	}

	static FORCEINLINE uint8 PackedTypeNameToTypeIndex(uint8 ArrayName)
	{
		switch (ArrayName)
		{
		case PACKED_TYPENAME_HIGHP:		return PACKED_TYPEINDEX_HIGHP;
		case PACKED_TYPENAME_MEDIUMP:	return PACKED_TYPEINDEX_MEDIUMP;
		case PACKED_TYPENAME_LOWP:		return PACKED_TYPEINDEX_LOWP;
		case PACKED_TYPENAME_INT:		return PACKED_TYPEINDEX_INT;
		case PACKED_TYPENAME_UINT:		return PACKED_TYPEINDEX_UINT;
		}
		check(0);
		return 0;
	}

	struct FPackedArrayInfo
	{
		uint16	Size;		// Bytes
		uint8	TypeName;	// PACKED_TYPENAME
		uint8	TypeIndex;	// PACKED_TYPE
	};

	inline FArchive& operator<<(FArchive& Ar, FPackedArrayInfo& Info)
	{
		Ar << Info.Size;
		Ar << Info.TypeName;
		Ar << Info.TypeIndex;
		return Ar;
	}
}
