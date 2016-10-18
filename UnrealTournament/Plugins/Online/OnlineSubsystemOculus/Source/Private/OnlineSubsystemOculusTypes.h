// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemOculusPackage.h"
#include "OVR_Platform.h"

class FUniqueNetIdOculus : public FUniqueNetId {
private:
	ovrID ID;

protected:
	virtual bool Compare(const FUniqueNetId& Other) const
	{
		if (Other.GetSize() != sizeof(ovrID))
		{
			return false;
		}

		return ID == static_cast<const FUniqueNetIdOculus&>(Other).ID;
	}

public:
	/** Default constructor */
	FUniqueNetIdOculus()
	{
		ID = 0;
	}

	FUniqueNetIdOculus(const ovrID& id)
	{
		ID = id;
	}

	FUniqueNetIdOculus(const FString& id)
	{
		ovrID_FromString(&ID, TCHAR_TO_ANSI(*id));
	}

	/**
	* Copy Constructor
	*
	* @param Src the id to copy
	*/
	explicit FUniqueNetIdOculus(const FUniqueNetId& Src)
	{
		if (Src.GetSize() == sizeof(ovrID))
		{
			ID = static_cast<const FUniqueNetIdOculus&>(Src).ID;
		}
	}

	// IOnlinePlatformData

	virtual const uint8* GetBytes() const override
	{
		uint8* byteArray = static_cast<uint8*>(malloc(sizeof(uint8) * 4));
		check(byteArray);

		// convert from an unsigned long int to a 4-byte array
		byteArray[0] = static_cast<uint8>((ID >> 24) & 0xFF);
		byteArray[1] = static_cast<uint8>((ID >> 16) & 0xFF);
		byteArray[2] = static_cast<uint8>((ID >> 8) & 0XFF);
		byteArray[3] = static_cast<uint8>((ID & 0XFF));

		return byteArray;
	}

	virtual int32 GetSize() const override
	{
		return sizeof(ID);
	}

	virtual bool IsValid() const override
	{
		return ID != 0;
	}

	ovrID GetID() const
	{
		return ID;
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("%llu"), ID);
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("ovrID: %llu"), ID);
	}

	/** Needed for TMap::GetTypeHash() */
	friend uint32 GetTypeHash(const FUniqueNetIdOculus& A)
	{
		return ::PointerHash(&A.ID, sizeof(A.ID));
	}
};

/**
* Implementation of session information
*/
class FOnlineSessionInfoOculus : public FOnlineSessionInfo
{
protected:

	/** Hidden on purpose */
	FOnlineSessionInfoOculus(const FOnlineSessionInfoOculus& Src)
	{
	}

	/** Hidden on purpose */
	FOnlineSessionInfoOculus& operator=(const FOnlineSessionInfoOculus& Src)
	{
		return *this;
	}

PACKAGE_SCOPE:

	FOnlineSessionInfoOculus(ovrID RoomId);

	/** Unique Id for this session */
	FUniqueNetIdOculus SessionId;

public:

	virtual ~FOnlineSessionInfoOculus() {}

	bool operator==(const FOnlineSessionInfoOculus& Other) const
	{
		return Other.SessionId == SessionId;
	}

	virtual const uint8* GetBytes() const override
	{
		return nullptr;
	}

	virtual int32 GetSize() const override
	{
		return 0;
	}

	virtual bool IsValid() const override
	{
		return true;
	}

	virtual FString ToString() const override
	{
		return SessionId.ToString();
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("SessionId: %s"), *SessionId.ToDebugString());
	}

	virtual const FUniqueNetId& GetSessionId() const override
	{
		return SessionId;
	}
};
