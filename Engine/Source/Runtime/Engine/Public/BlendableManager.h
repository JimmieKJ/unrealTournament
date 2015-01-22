// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// used to blend IBlendableInterface object data, the member act as a header for a following data block
struct FBlendableEntry
{
	// weight 0..1, 0 to disable this entry
	float Weight;

private:
	// to ensure type safety
	FName BlendableType;
	// to be able to jump over data
	uint32 DataSize;

	// @return pointer to the next object or end (can be compared with container end pointer)
	uint8* GetDataPtr() { check(this); return (uint8*)(this + 1); }
	// @return next or end of the array
	FBlendableEntry* GetNext() { return (FBlendableEntry*)(GetDataPtr() + DataSize); }

	friend class FBlendableManager;
};

// Manager class which allows blending of FBlendableBase objects, stores a copy (for the render thread)
class FBlendableManager
{
public:

	FBlendableManager()
	{
		// can be increased if needed (to avoid reallocations and copies at the cost of more memory allocation)
		Scratch.Reserve(10 * 1024);
	}

	// @param InWeight 0..1, excluding 0 as this is used to disable entries
	// @param InData is copied with a memcpy
	template <class T>
	void PushBlendableData(float InWeight, const T& InData)
	{
		FName BlendableType = T::GetFName();

		PushBlendableDataPtr(InWeight, BlendableType, (const uint8*)&InData, sizeof(T));
	}

	// find next that has the given type, O(n), n is number of elements after the given one or all if started with 0
	// @param InIterator 0 to start iteration
	// @return 0 if no further one of that type was found
	template <class T>
	T* IterateBlendables(FBlendableEntry*& InIterator) const
	{
		FName BlendableType = T::GetFName();

		do
		{
			InIterator = GetNextBlendableEntryPtr(InIterator);

		} while (InIterator && InIterator->Weight <= 0.0f && InIterator->BlendableType != BlendableType);

		if (InIterator)
		{
			return (T*)InIterator->GetDataPtr();
		}

		return 0;
	}

private:

	// stored multiple elements with a FBlendableEntry header and following data of a size specified in the header
	TArray<uint8> Scratch;

	// find next, no restriction on the type O(n), n is number of elements after the given one or all if started with 0
	FBlendableEntry* GetNextBlendableEntryPtr(FBlendableEntry* InIterator = 0) const
	{
		if (!InIterator)
		{
			// start at the beginning
			InIterator = (FBlendableEntry*)Scratch.GetData();
		}
		else
		{
			// go to next
			InIterator = InIterator->GetNext();
		}

		// end reached?
		if ((uint8*)InIterator == Scratch.GetData() + Scratch.Num())
		{
			// no more entries
			return 0;
		}

		return InIterator;
	}

	// @param InWeight 0..1, excluding 0 as this is used to disable entries
	// @param InData is copied
	// @param InDataSize >0
	void PushBlendableDataPtr(float InWeight, FName InBlendableType, const uint8* InData, uint32 InDataSize)
	{
		check(InWeight > 0.0f && InWeight <= 1.0f);
		check(InData);
		check(InDataSize);

		uint32 OldSize = Scratch.AddUninitialized(sizeof(FBlendableEntry)+InDataSize);

		FBlendableEntry* Dst = (FBlendableEntry*)&Scratch[OldSize];

		Dst->Weight = InWeight;
		Dst->BlendableType = InBlendableType;
		Dst->DataSize = InDataSize;
		memcpy(Dst->GetDataPtr(), InData, InDataSize);
	}
};