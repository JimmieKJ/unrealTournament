// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Set.h"

class AActor;
class UWorld;

/**
 * Struct to store an actor pointer and any internal metadata for that actor used
 * internally by a UNetDriver.
 */
struct FNetworkObjectInfo
{
	/** Pointer to the replicated actor. */
	AActor* Actor;

	/** Next time to consider replicating the actor. Based on FPlatformTime::Seconds(). */
	double NextUpdateTime;

	/** Last absolute time in seconds since actor actually sent something during replication */
	double LastNetReplicateTime;

	/** Optimal delta between replication updates based on how frequently actor properties are actually changing */
	float OptimalNetUpdateDelta;

	FNetworkObjectInfo()
		: Actor(nullptr)
		, NextUpdateTime(0.0)
		, LastNetReplicateTime(0.0)
		, OptimalNetUpdateDelta(0.0f) {}

	FNetworkObjectInfo(AActor* InActor)
		: Actor(InActor)
		, NextUpdateTime(0.0)
		, LastNetReplicateTime(0.0)
		, OptimalNetUpdateDelta(0.0f) {}
};

/**
 * KeyFuncs to allow using the actor pointer as the comparison key in a set.
 */
struct FNetworkObjectKeyFuncs : BaseKeyFuncs<TSharedPtr<FNetworkObjectInfo>, AActor*, false>
{
	/**
	 * @return The key used to index the given element.
	 */
	static KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.Get()->Actor;
	}

	/**
	 * @return True if the keys match.
	 */
	static bool Matches(KeyInitType A,KeyInitType B)
	{
		return A == B;
	}

	/** Calculates a hash index for a key. */
	static uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

/**
 * Stores the list of replicated actors for a given UNetDriver.
 */
class FNetworkObjectList
{
public:
	typedef TSet<TSharedPtr<FNetworkObjectInfo>, FNetworkObjectKeyFuncs> FNetworkObjectSet;

	/**
	 * Adds replicated actors in World to the internal set of replicated actors.
	 * Used when a net driver is initialized after some actors may have already
	 * been added to the world.
	 *
	 * @param World The world from which actors are added.
	 * @param NetDriverName The name of the net driver to which this object list belongs.
	 */
	void AddInitialObjects(UWorld* const World, const FName NetDriverName);

	/** Adds Actor to the internal list if its NetDriverName matches, or if we're adding to the demo net driver. */
	void Add(AActor* const Actor, const FName NetDriverName);

	/** Returns a reference to the set of tracked actors. */
	FNetworkObjectSet& GetObjects() { return NetworkObjects; }

	/** Returns a const reference to the set of tracked actors. */
	const FNetworkObjectSet& GetObjects() const { return NetworkObjects; }

private:
	FNetworkObjectSet NetworkObjects;
};
