// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class FUnrealSourceFile;
class FUHTMakefile;

enum class EHeaderProviderSourceType
{
	ClassName,
	FileName,
	Resolved,
	Invalid,
};

class FHeaderProvider
{
	friend bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
public:
	FHeaderProvider(EHeaderProviderSourceType Type, const FString& Id, bool bAutoInclude = false);
	FHeaderProvider()
		: Type(EHeaderProviderSourceType::Invalid)
		, Id(FString())
		, Cache(nullptr)
		, bAutoInclude(false)
	{ }

	FUnrealSourceFile* Resolve();
	const FUnrealSourceFile* GetResolved() const;
	FUnrealSourceFile* GetResolved();

	FString ToString() const;

	const FString& GetId() const;

	EHeaderProviderSourceType GetType() const;

	bool IsAutoInclude() const { return bAutoInclude; }
	void SetCache(FUnrealSourceFile* InCache)
	{
		Cache = InCache;
	}
private:
	EHeaderProviderSourceType Type;
	FString Id;
	FUnrealSourceFile* Cache;

	// Tells if this include should be auto included in generated.h file.
	bool bAutoInclude;
};

bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
