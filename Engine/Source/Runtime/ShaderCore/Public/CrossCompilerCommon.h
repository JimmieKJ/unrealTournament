// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

	static FORCEINLINE uint8 ShaderStageIndexToTypeName(uint8 ShaderStage)
	{
		switch (ShaderStage)
		{
		case SHADER_STAGE_VERTEX:	return 'v';
		case SHADER_STAGE_PIXEL:	return 'p';
		case SHADER_STAGE_GEOMETRY:	return 'g';
		case SHADER_STAGE_HULL:		return 'h';
		case SHADER_STAGE_DOMAIN:	return 'd';
		case SHADER_STAGE_COMPUTE:	return 'c';
		default: break;
		}
		check(0);
		return 0;
	}

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

	struct FShaderBindings
	{
		TArray<TArray<FPackedArrayInfo>>	PackedUniformBuffers;
		TArray<FPackedArrayInfo>			PackedGlobalArrays;
		FShaderCompilerResourceTable		ShaderResourceTable;

		uint16	InOutMask;
		uint8	NumSamplers;
		uint8	NumUniformBuffers;
		uint8	NumUAVs;
		bool	bHasRegularUniformBuffers;
	};

	// Information for copying members from uniform buffers to packed
	struct FUniformBufferCopyInfo
	{
		uint16 SourceOffsetInFloats;
		uint8 SourceUBIndex;
		uint8 DestUBIndex;
		uint8 DestUBTypeName;
		uint8 DestUBTypeIndex;
		uint16 DestOffsetInFloats;
		uint16 SizeInFloats;
	};

	inline FArchive& operator<<(FArchive& Ar, FUniformBufferCopyInfo& Info)
	{
		Ar << Info.SourceOffsetInFloats;
		Ar << Info.SourceUBIndex;
		Ar << Info.DestUBIndex;
		Ar << Info.DestUBTypeName;
		if (Ar.IsLoading())
		{
			Info.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(Info.DestUBTypeName);
		}
		Ar << Info.DestOffsetInFloats;
		Ar << Info.SizeInFloats;
		return Ar;
	}
}
