// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UHTMakefile/PropertyBaseArchiveProxy.h"

class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FTokenArchiveProxy : public FPropertyBaseArchiveProxy
{
	FTokenArchiveProxy(const FUHTMakefile& UHTMakefile, const FToken* Token);
	FTokenArchiveProxy() { }

	static void AddReferencedNames(const FToken* Token, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FTokenArchiveProxy& TokenArchiveProxy);
	FToken* CreateToken() const;

	void PostConstruct(FToken* Token) const;

	void Resolve(FToken* Token, const FUHTMakefile& UHTMakefile) const;

	ETokenType TokenType;
	FNameArchiveProxy TokenName;
	int32 StartPos;
	int32 StartLine;
	TCHAR Identifier[NAME_SIZE];
	int32 TokenPropertyIndex;

	union
	{
		uint8 Byte;								// If CPT_Byte.
		int32 Int;								// If CPT_Int.
		bool NativeBool;						// if CPT_Bool
		float Float;							// If CPT_Float.
		double Double;							// If CPT_Double.
		uint8 NameBytes[sizeof(FName)];			// If CPT_Name.
		TCHAR String[MAX_STRING_CONST_SIZE];	// If CPT_String
	};
};
