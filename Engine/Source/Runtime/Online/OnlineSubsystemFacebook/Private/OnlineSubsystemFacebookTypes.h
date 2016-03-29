// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemFacebookPackage.h"

/**
 * Facebook specific implementation of the unique net id
 */
class FUniqueNetIdFacebook :
	public FUniqueNetId
{
PACKAGE_SCOPE:
	/** Holds the net id for a player */
	uint64 UniqueNetId;

	/** Hidden on purpose */
	FUniqueNetIdFacebook() :
		UniqueNetId(0)
	{
	}

	/**
	 * Copy Constructor
	 *
	 * @param Src the id to copy
	 */
	explicit FUniqueNetIdFacebook(const FUniqueNetIdFacebook& Src) :
		UniqueNetId(Src.UniqueNetId)
	{
	}

public:
	/**
	 * Constructs this object with the specified net id
	 *
	 * @param InUniqueNetId the id to set ours to
	 */
	explicit FUniqueNetIdFacebook(uint64 InUniqueNetId) :
		UniqueNetId(InUniqueNetId)
	{
	}

	//~ Begin FUniqueNetId Interface
	virtual const uint8* GetBytes() const override
	{
		return (uint8*)&UniqueNetId;
	}


	virtual int32 GetSize() const override
	{
		return sizeof(uint64);
	}


	virtual bool IsValid() const override
	{
		return UniqueNetId != 0;
	}


	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("%I64d"), UniqueNetId);
	}


	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("0%I64X"), UniqueNetId);
	}

	//~ End FUniqueNetId Interface


public:
	/** Needed for TMap::GetTypeHash() */
	friend uint32 GetTypeHash(const FUniqueNetIdFacebook& A)
	{
		return (uint32)(A.UniqueNetId) + ((uint32)((A.UniqueNetId) >> 32 ) * 23);
	}
};
