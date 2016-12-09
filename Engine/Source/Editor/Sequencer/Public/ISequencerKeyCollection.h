// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSequencerDisplayNode;

enum class EFindKeyDirection
{
	Forwards, Backwards
};

/**
 * A collection of keys gathered recursively from a particular node or nodes
 */
class ISequencerKeyCollection
{
public:
	virtual ~ISequencerKeyCollection() {}

public:

	/** Iterate the keys contained within this collection */
	virtual void IterateKeys(const TFunctionRef<bool(float)>& Iter) = 0;

	/** Get a value specifying how close keys need to be in order to be considered equal by this collection */
	virtual float GetKeyGroupingThreshold() const = 0;

	/** Find the first key in the given range */
	virtual TOptional<float> FindFirstKeyInRange(const TRange<float>& InRange, EFindKeyDirection Direction) const = 0;

public:

	/** Initialize this key collection from the specified nodes. Only gathers keys from those explicitly specified. */
	virtual void InitializeExplicit(const TArray<FSequencerDisplayNode*>& InNodes, float DuplicateThreshold = SMALL_NUMBER) = 0;

	/** Initialize this key collection from the specified nodes. Gathers keys from all child nodes. */
	virtual void InitializeRecursive(const TArray<FSequencerDisplayNode*>& InNodes, float DuplicateThreshold = SMALL_NUMBER) = 0;

	/** Initialize this key collection from the specified node and section index. */
	virtual void InitializeRecursive(FSequencerDisplayNode& InNode, int32 InSectionIndex, float DuplicateThreshold = SMALL_NUMBER) = 0;
};
