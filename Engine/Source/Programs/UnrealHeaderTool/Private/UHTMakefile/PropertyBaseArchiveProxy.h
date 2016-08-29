// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "ParserHelper.h"
/* See UHTMakefile.h for overview how makefiles work. */

class FPropertyBase;
class FUHTMakefile;

inline FArchive& operator<<(FArchive& Ar, EPropertyType& PropertyType)
{
	if (Ar.IsLoading())
	{
		int32 Value;
		Ar << Value;
		PropertyType = (EPropertyType)Value;
	}
	else if (Ar.IsSaving())
	{
		int32 Value = (int32)PropertyType;
		Ar << Value;
	}

	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, ETokenType& TokenType)
{
	if (Ar.IsLoading())
	{
		int32 Value;
		Ar << Value;
		TokenType = (ETokenType)Value;
	}
	else if (Ar.IsSaving())
	{
		int32 Value = (int32)TokenType;
		Ar << Value;
	}

	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, EArrayType::Type& ArrayType)
{
	if (Ar.IsLoading())
	{
		int32 Value;
		Ar << Value;
		ArrayType = (EArrayType::Type)Value;
	}
	else if (Ar.IsSaving())
	{
		int32 Value = (int32)ArrayType;
		Ar << Value;
	}

	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, ERefQualifier::Type& RefQualifier)
{
	if (Ar.IsLoading())
	{
		int32 Value;
		Ar << Value;
		RefQualifier = (ERefQualifier::Type)Value;
	}
	else if (Ar.IsSaving())
	{
		int32 Value = (int32)RefQualifier;
		Ar << Value;
	}

	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, EPointerType::Type& PointerType)
{
	if (Ar.IsLoading())
	{
		int32 Value;
		Ar << Value;
		PointerType = (EPointerType::Type)Value;
	}
	else if (Ar.IsSaving())
	{
		int32 Value = (int32)PointerType;
		Ar << Value;
	}

	return Ar;
}

struct FPropertyBaseArchiveProxy
{
	FPropertyBaseArchiveProxy(const FUHTMakefile& UHTMakefile, const FPropertyBase* PropertyBase);
	FPropertyBaseArchiveProxy() { }

	static void AddReferencedNames(const FPropertyBase* PropertyBase, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FPropertyBaseArchiveProxy& PropertyBaseArchiveProxy);
	FPropertyBase* CreatePropertyBase(const FUHTMakefile& UHTMakefile) const;

	void PostConstruct(FPropertyBase* PropertyBase) const;

	void Resolve(FPropertyBase* PropertyBase, const FUHTMakefile& UHTMakefile) const;

	EPropertyType Type;
	EArrayType::Type ArrayType;
	uint64 PropertyFlags;
	uint64 ImpliedPropertyFlags;
	ERefQualifier::Type RefQualifier;
	int32 MapKeyPropIndex;

	uint32 PropertyExportFlags;
	FSerializeIndex PropertyClassIndex;
	FSerializeIndex StructIndex;
	union
	{
		int32 EnumIndex;
		int32 FunctionIndex;
#if PLATFORM_64BITS
		int64 StringSize;
#else
		int32 StringSize;
#endif
	};

	FSerializeIndex MetaClassIndex;
	FNameArchiveProxy DelegateName;
	FNameArchiveProxy RepNotifyName;
	FString	ExportInfo;
	TArray<TPair<FNameArchiveProxy, FString>> MetaData;
	EPointerType::Type PointerType;
};
