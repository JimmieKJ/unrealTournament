// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"


struct ECustomVersionSerializationFormat
{
	enum Type
	{
		Guids,
		Enums
	};
};


/**
 * Structure to hold unique custom key with its version.
 */
struct CORE_API FCustomVersion
{
	/** Unique custom key. */
	FGuid Key;

	/** Custom version. */
	int32 Version;

	/** Friendly name for error messages or whatever. */
	FString FriendlyName;

	/** Number of times this GUID has been registered */
	int32 ReferenceCount;

	/** Constructor. */
	FORCEINLINE FCustomVersion()
	{
	}

	/** Helper constructor. */
	FORCEINLINE FCustomVersion(FGuid InKey, int32 InVersion, FString InFriendlyName)
	: Key           (InKey)
	, Version       (InVersion)
	, FriendlyName  (MoveTemp(InFriendlyName))
	, ReferenceCount(1)
	{
	}

	/** Equality comparison operator for Key */
	FORCEINLINE bool operator==(FGuid InKey) const
	{
		return Key == InKey;
	}

	/** Inequality comparison operator for Key */
	FORCEINLINE bool operator!=(FGuid InKey) const
	{
		return Key != InKey;
	}

	CORE_API friend FArchive& operator<<(FArchive& Ar, FCustomVersion& Version);
};


class CORE_API FCustomVersionRegistration;


/**
 * Container for all available/serialized custom versions.
 */
class CORE_API FCustomVersionContainer
{
	friend class FCustomVersionRegistration;

public:
	/** Gets available custom versions in this container. */
	FORCEINLINE const TArray<FCustomVersion>& GetAllVersions() const
	{
		return Versions;
	}

	/**
	 * Gets a custom version from the container.
	 *
	 * @param CustomKey Custom key for which to retrieve the version.
	 * @return The FCustomVersion for the specified custom key, or nullptr if the key doesn't exist in the container.
	 */
	const FCustomVersion* GetVersion(FGuid CustomKey) const;

	/**
	 * Sets a specific custom version in the container.
	 *
	 * @param CustomKey Custom key for which to retrieve the version.
	 * @param Version The version number for the specified custom key
	 * @param FriendlyName A friendly name to assign to this version
	 */
	void SetVersion(FGuid CustomKey, int32 Version, FString FriendlyName);

	/** Serialization. */
	void Serialize(FArchive& Ar, ECustomVersionSerializationFormat::Type Format = ECustomVersionSerializationFormat::Guids);

	/**
	 * Gets a singleton with the registered versions.
	 *
	 * @return The registered version container.
	 */
	static const FCustomVersionContainer& GetRegistered();

	/**
	 * Empties the custom version container.
	 */
	void Empty();

	/** Return a string representation of custom versions. Used for debug. */
	FString ToString(const FString& Indent) const;

private:

	/** Private implementation getter */
	static FCustomVersionContainer& GetInstance();

	/** Array containing custom versions. */
	TArray<FCustomVersion> Versions;
};


/**
 * This class will cause the registration of a custom version number and key with the global
 * FCustomVersionContainer when instantiated, and unregister when destroyed.  It should be
 * instantiated as a global variable somewhere in the module which needs a custom version number.
 */
class FCustomVersionRegistration : FNoncopyable
{
public:

	FCustomVersionRegistration(FGuid InKey, int32 Version, const TCHAR*);
	~FCustomVersionRegistration();

private:

	FGuid Key;
};
