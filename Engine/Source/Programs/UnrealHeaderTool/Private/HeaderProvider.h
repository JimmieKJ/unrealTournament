// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealString.h"

class FUnrealSourceFile;

enum class EHeaderProviderSourceType
{
	ClassName,
	FileName,
	Resolved
};

class FHeaderProvider
{
	friend bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
public:
	FHeaderProvider(EHeaderProviderSourceType Type, const FString& Id, bool bAutoInclude = false);

	FUnrealSourceFile* Resolve();
	const FUnrealSourceFile* GetResolved() const;

	FString ToString() const;

	const FString& GetId() const;

	EHeaderProviderSourceType GetType() const;

	bool IsAutoInclude() const { return bAutoInclude; }

private:
	EHeaderProviderSourceType Type;
	FString Id;
	FUnrealSourceFile* Cache;

	// Tells if this include should be auto included in generated.h file.
	bool bAutoInclude;
};

bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);