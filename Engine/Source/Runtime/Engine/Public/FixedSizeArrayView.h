// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// #include "Containers/ContainerAllocationPolicies.h"
// #include "HAL/Platform.h"
// #include "Serialization/ArchiveBase.h"
// #include "Templates/EnableIf.h"
// #include "Templates/Sorting.h"
// #include "Templates/UnrealTemplate.h"

/**
 * Templated fixed-size view of another array
 *
 * A statically sized view of an array of typed elements.  Designed to allow functions to take either a fixed C array
 * or a TArray with an arbitrary allocator as an argument when the function neither adds nor removes elements
 *
 * Caution: When constructed from a TArray, the length and memory allocation of the underlying array must remain fixed for the lifetime of the array view!
 *
 **/
template<typename InElementType>
class TFixedSizeArrayView
{
public:
	typedef InElementType ElementType;

	/**
	 * Constructor.
	 */
	TFixedSizeArrayView()
		: DataPtr(nullptr)
		, ArrayNum(0)
	{}

	/**
	 * Copy constructor from another view
	 *
	 * @param Other The source array view to copy
	 */
	TFixedSizeArrayView(const TFixedSizeArrayView& Other)
		: DataPtr(Other.DataPtr)
		, ArrayNum(Other.ArrayNum)
	{
	}

	/**
	 * Construct a view of a TArray with a compatible element type
	 *
	 * @param Other The source array to view.
	 */
	template <typename OtherElementType, typename OtherAllocator>
	TFixedSizeArrayView(TArray<OtherElementType, OtherAllocator>& Other)
		: DataPtr(Other.GetData())
		, ArrayNum(Other.Num())
	{
	}

	/**
	 * Construct a view of an arbitrary pointer
	 *
	 * @param InData	The data to view
	 * @param InCount	The number of elements
	 */
	TFixedSizeArrayView(ElementType* InData, int32 InCount)
		: DataPtr(InData)
		, ArrayNum(InCount)
	{
	}

	/**
	 * Assignment operator.
	 *
	 * @param Other The source array view to assign from.
	 */
	TFixedSizeArrayView& operator=(const TFixedSizeArrayView& Other)
	{
		if (this != &Other)
		{
			DataPtr = Other.DataPtr;
			ArrayNum = Other.ArrayNum;
		}
		return *this;
	}

public:

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if ArrayMax == 0.
	 */
	FORCEINLINE ElementType* GetData()
	{
		return DataPtr;
	}

	/**
	 * Helper function for returning a typed pointer to the first array entry.
	 *
	 * @returns Pointer to first array entry or nullptr if ArrayMax == 0.
	 */
	FORCEINLINE const ElementType* GetData() const
	{
		return DataPtr;
	}

	/** 
	 * Helper function returning the size of the inner type.
	 *
	 * @returns Size in bytes of array type.
	 */
	FORCEINLINE uint32 GetTypeSize() const
	{
		return sizeof(ElementType);
	}

	/**
	 * Checks array invariants: if array size is greater than zero and less
	 * than maximum.
	 */
	FORCEINLINE void CheckInvariants() const
	{
		checkSlow(ArrayNum >= 0);
	}

	/**
	 * Checks if index is in array range.
	 *
	 * @param Index Index to check.
	 */
	FORCEINLINE void RangeCheck(int32 Index) const
	{
		CheckInvariants();

		checkf((Index >= 0) & (Index < ArrayNum),TEXT("Array index out of bounds: %i from an array of size %i"),Index,ArrayNum); // & for one branch
	}

	/**
	 * Tests if index is valid, i.e. than or equal to zero, and less than the number of elements in the array.
	 *
	 * @param Index Index to test.
	 *
	 * @returns True if index is valid. False otherwise.
	 */
	FORCEINLINE bool IsValidIndex(int32 Index) const
	{
		return (Index >= 0) && (Index < ArrayNum);
	}

	/**
	 * Returns number of elements in array.
	 *
	 * @returns Number of elements in array.
	 */
	FORCEINLINE int32 Num() const
	{
		return ArrayNum;
	}

	/**
	 * Array bracket operator. Returns reference to element at give index.
	 *
	 * @returns Reference to indexed element.
	 */
	FORCEINLINE ElementType& operator[](int32 Index)
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	/**
	 * Array bracket operator. Returns reference to element at give index.
	 *
	 * Const version of the above.
	 *
	 * @returns Reference to indexed element.
	 */
	FORCEINLINE const ElementType& operator[](int32 Index) const
	{
		RangeCheck(Index);
		return GetData()[Index];
	}

	/**
	 * Returns n-th last element from the array.
	 *
	 * @param IndexFromTheEnd (Optional) Index from the end of array.
	 *                        Default is 0.
	 *
	 * @returns Reference to n-th last element from the array.
	 */
	ElementType& Last(int32 IndexFromTheEnd = 0)
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}

	/**
	 * Returns n-th last element from the array.
	 *
	 * Const version of the above.
	 *
	 * @param IndexFromTheEnd (Optional) Index from the end of array.
	 *                        Default is 0.
	 *
	 * @returns Reference to n-th last element from the array.
	 */
	const ElementType& Last(int32 IndexFromTheEnd = 0) const
	{
		RangeCheck(ArrayNum - IndexFromTheEnd - 1);
		return GetData()[ArrayNum - IndexFromTheEnd - 1];
	}

	/**
	 * Finds element within the array.
	 *
	 * @param Item Item to look for.
	 * @param Index Output parameter. Found index.
	 *
	 * @returns True if found. False otherwise.
	 */
	FORCEINLINE bool Find(const ElementType& Item, int32& Index) const
	{
		Index = this->Find(Item);
		return Index != INDEX_NONE;
	}

	/**
	 * Finds element within the array.
	 *
	 * @param Item Item to look for.
	 *
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 */
	int32 Find(const ElementType& Item) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds element within the array starting from the end.
	 *
	 * @param Item Item to look for.
	 * @param Index Output parameter. Found index.
	 *
	 * @returns True if found. False otherwise.
	 */
	FORCEINLINE bool FindLast(const ElementType& Item, int32& Index) const
	{
		Index = this->FindLast(Item);
		return Index != INDEX_NONE;
	}

	/**
	 * Finds element within the array starting from the end.
	 *
	 * @param Item Item to look for.
	 *
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 */
	int32 FindLast(const ElementType& Item) const
	{
		for (const ElementType* RESTRICT Start = GetData(), *RESTRICT Data = Start + ArrayNum; Data != Start; )
		{
			--Data;
			if (*Data == Item)
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds element within the array starting from StartIndex and going backwards. Uses predicate to match element.
	 *
	 * @param Pred Predicate taking array element and returns true if element matches search criteria, false otherwise.
	 * @param StartIndex Index of element from which to start searching.
	 *
	 * @returns Index of the found element. INDEX_NONE otherwise.
	 */
	template <typename Predicate>
	int32 FindLastByPredicate(Predicate Pred, int32 StartIndex) const
	{
		check(StartIndex >= 0 && StartIndex <= this->Num());
		for (const ElementType* RESTRICT Start = GetData(), *RESTRICT Data = Start + StartIndex; Data != Start; )
		{
			--Data;
			if (Pred(*Data))
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	* Finds element within the array starting from the end. Uses predicate to match element.
	*
	* @param Pred Predicate taking array element and returns true if element matches search criteria, false otherwise.
	*
	* @returns Index of the found element. INDEX_NONE otherwise.
	*/
	template <typename Predicate>
	FORCEINLINE int32 FindLastByPredicate(Predicate Pred) const
	{
		return FindLastByPredicate(Pred, ArrayNum);
	}

	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for
	 * the comparison).
	 *
	 * @param Key The key to search by.
	 *
	 * @returns Index to the first matching element, or INDEX_NONE if none is
	 *          found.
	 */
	template <typename KeyType>
	int32 IndexOfByKey(const KeyType& Key) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Start + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Key)
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds an item by predicate.
	 *
	 * @param Pred The predicate to match.
	 *
	 * @returns Index to the first matching element, or INDEX_NONE if none is
	 *          found.
	 */
	template <typename Predicate>
	int32 IndexOfByPredicate(Predicate Pred) const
	{
		const ElementType* RESTRICT Start = GetData();
		for (const ElementType* RESTRICT Data = Start, *RESTRICT DataEnd = Start + ArrayNum; Data != DataEnd; ++Data)
		{
			if (Pred(*Data))
			{
				return static_cast<int32>(Data - Start);
			}
		}
		return INDEX_NONE;
	}

	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for
	 * the comparison).
	 *
	 * @param Key The key to search by.
	 *
	 * @returns Pointer to the first matching element, or nullptr if none is found.
	 */
	template <typename KeyType>
	FORCEINLINE const ElementType* FindByKey(const KeyType& Key) const
	{
		return const_cast<TFixedSizeArrayView*>(this)->FindByKey(Key);
	}

	/**
	 * Finds an item by key (assuming the ElementType overloads operator== for
	 * the comparison).
	 *
	 * @param Key The key to search by.
	 *
	 * @returns Pointer to the first matching element, or nullptr if none is found.
	 */
	template <typename KeyType>
	ElementType* FindByKey(const KeyType& Key)
	{
		for (ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Key)
			{
				return Data;
			}
		}

		return nullptr;
	}

	/**
	 * Finds an element which matches a predicate functor.
	 *
	 * @param Pred The functor to apply to each element.
	 *
	 * @returns Pointer to the first element for which the predicate returns
	 *          true, or nullptr if none is found.
	 */
	template <typename Predicate>
	FORCEINLINE const ElementType* FindByPredicate(Predicate Pred) const
	{
		return const_cast<TFixedSizeArrayView*>(this)->FindByPredicate(Pred);
	}

	/**
	 * Finds an element which matches a predicate functor.
	 *
	 * @param Pred The functor to apply to each element.
	 *
	 * @return Pointer to the first element for which the predicate returns
	 *         true, or nullptr if none is found.
	 */
	template <typename Predicate>
	ElementType* FindByPredicate(Predicate Pred)
	{
		for (ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (Pred(*Data))
			{
				return Data;
			}
		}

		return nullptr;
	}

	/**
	 * Filters the elements in the array based on a predicate functor.
	 *
	 * @param Pred The functor to apply to each element.
	 *
	 * @returns TArray with the same type as this object which contains
	 *          the subset of elements for which the functor returns true.
	 */
	template <typename Predicate>
	TArray<ElementType> FilterByPredicate(Predicate Pred) const
	{
		TArray<ElementType> FilterResults;
		for (const ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (Pred(*Data))
			{
				FilterResults.Add(*Data);
			}
		}
		return FilterResults;
	}

	/**
	 * Checks if this array contains the element.
	 *
	 * @returns	True if found. False otherwise.
	 */
	template <typename ComparisonType>
	bool Contains(const ComparisonType& Item) const
	{
		for (const ElementType* RESTRICT Data = GetData(), *RESTRICT DataEnd = Data + ArrayNum; Data != DataEnd; ++Data)
		{
			if (*Data == Item)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Checks if this array contains element for which the predicate is true.
	 *
	 * @param Predicate to use
	 *
	 * @returns	True if found. False otherwise.
	 */
	template <typename Predicate>
	FORCEINLINE bool ContainsByPredicate(Predicate Pred) const
	{
		return FindByPredicate(Pred) != nullptr;
	}

	/**
	 * Equality operator.
	 *
	 * @param OtherArray Array to compare.
	 *
	 * @returns True if this array is the same as OtherArray. False otherwise.
	 */
	bool operator==(const TFixedSizeArrayView& OtherArray) const
	{
		const int32 Count = Num();

		return (Count == OtherArray.Num()) && CompareItems(GetData(), OtherArray.GetData(), Count);
	}

	/**
	 * Inequality operator.
	 *
	 * @param OtherArray Array to compare.
	 *
	 * @returns True if this array is NOT the same as OtherArray. False otherwise.
	 */
	bool operator!=(const TFixedSizeArrayView& OtherArray) const
	{
		return !(*this == OtherArray);
	}

	// Iterators
	typedef TIndexedContainerIterator<      TFixedSizeArrayView,       ElementType, int32> TIterator;
	typedef TIndexedContainerIterator<const TFixedSizeArrayView, const ElementType, int32> TConstIterator;

	/**
	 * Creates an iterator for the contents of this array
	 *
	 * @returns The iterator.
	 */
	TIterator CreateIterator()
	{
		return TIterator(*this);
	}

	/**
	 * Creates a const iterator for the contents of this array
	 *
	 * @returns The const iterator.
	 */
	TConstIterator CreateConstIterator() const
	{
		return TConstIterator(*this);
	}

private:
	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE friend TIterator      begin(      TFixedSizeArrayView& Array) { return TIterator     (Array); }
	FORCEINLINE friend TConstIterator begin(const TFixedSizeArrayView& Array) { return TConstIterator(Array); }
	FORCEINLINE friend TIterator      end  (      TFixedSizeArrayView& Array) { return TIterator     (Array, Array.Num()); }
	FORCEINLINE friend TConstIterator end  (const TFixedSizeArrayView& Array) { return TConstIterator(Array, Array.Num()); }

public:
	/**
	 * Sorts the array assuming < operator is defined for the item type.
	 */
	void Sort()
	{
		::Sort(GetData(), Num());
	}

	/**
	 * Sorts the array using user define predicate class.
	 *
	 * @param Predicate Predicate class instance.
	 */
	template <class PREDICATE_CLASS>
	void Sort(const PREDICATE_CLASS& Predicate)
	{
		::Sort(GetData(), Num(), Predicate);
	}

	/**
	 * Stable sorts the array assuming < operator is defined for the item type.
	 *
	 * Stable sort is slower than non-stable algorithm.
	 */
	void StableSort()
	{
		::StableSort(GetData(), Num());
	}

	/**
	 * Stable sorts the array using user defined predicate class.
	 *
	 * Stable sort is slower than non-stable algorithm.
	 *
	 * @param Predicate Predicate class instance
	 */
	template <class PREDICATE_CLASS>
	void StableSort(const PREDICATE_CLASS& Predicate)
	{
		::StableSort(GetData(), Num(), Predicate);
	}

private:
	ElementType* DataPtr;
	int32 ArrayNum;
};

template <typename InElementType>
struct TIsZeroConstructType<TFixedSizeArrayView<InElementType>>
{
	enum { Value = true };
};
