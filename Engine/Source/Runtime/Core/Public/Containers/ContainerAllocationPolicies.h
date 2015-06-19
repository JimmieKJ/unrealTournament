// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MemoryBase.h"
#include "Misc/OutputDevice.h"


/** Used to determine the alignment of an element type. */
template<typename ElementType>
class TElementAlignmentCalculator
{
private:
	/**
	 * This is a dummy type that's used to calculate the padding added between the byte and the element
	 * to fulfill the type's required alignment.
	 */
	struct FAlignedElements
	{
		uint8 MisalignmentPadding;
		ElementType Element;
		/** FAlignedElement's default constructor and destructor are declared but never implemented to avoid the need for a ElementType default constructor. */
		FAlignedElements();
		~FAlignedElements();
	};
public:
	enum { Alignment = sizeof(FAlignedElements) - sizeof(ElementType) };
};

#define ALIGNOF(T) (TElementAlignmentCalculator<T>::Alignment)


/** branchless pointer selection
* return A ? A : B;
**/
template<typename ReferencedType>
ReferencedType* IfAThenAElseB(ReferencedType* A,ReferencedType* B);

/** branchless pointer selection based on predicate
* return PTRINT(Predicate) ? A : B;
**/
template<typename PredicateType,typename ReferencedType>
ReferencedType* IfPThenAElseB(PredicateType Predicate,ReferencedType* A,ReferencedType* B);

/** The default slack calculation heuristic. */
extern CORE_API int32 DefaultCalculateSlack(int32 NumElements,int32 NumAllocatedElements,SIZE_T BytesPerElement);


/** A type which is used to represent a script type that is unknown at compile time. */
struct FScriptContainerElement
{
};

/**
 * Used to declare an untyped array of data with compile-time alignment.
 * It needs to use template specialization as the MS_ALIGN and GCC_ALIGN macros require literal parameters.
 */
template<int32 Size,uint32 Alignment>
struct TAlignedBytes; // this intentionally won't compile, we don't support the requested alignment

/** Unaligned storage. */
template<int32 Size>
struct TAlignedBytes<Size,1>
{
	uint8 Pad[Size];
};


// C++/CLI doesn't support alignment of native types in managed code, so we enforce that the element
// size is a multiple of the desired alignment
#ifdef __cplusplus_cli
	#define IMPLEMENT_ALIGNED_STORAGE(Align) \
		template<int32 Size>        \
		struct TAlignedBytes<Size,Align> \
		{ \
			uint8 Pad[Size]; \
			static_assert(Size % Align == 0, "CLR interop types must not be aligned."); \
		};
#else
/** A macro that implements TAlignedBytes for a specific alignment. */
#define IMPLEMENT_ALIGNED_STORAGE(Align) \
	template<int32 Size>        \
	struct TAlignedBytes<Size,Align> \
	{ \
		struct MS_ALIGN(Align) TPadding \
		{ \
			uint8 Pad[Size]; \
		} GCC_ALIGN(Align); \
		TPadding Padding; \
	};
#endif

// Implement TAlignedBytes for these alignments.
IMPLEMENT_ALIGNED_STORAGE(16);
IMPLEMENT_ALIGNED_STORAGE(8);
IMPLEMENT_ALIGNED_STORAGE(4);
IMPLEMENT_ALIGNED_STORAGE(2);

#undef IMPLEMENT_ALIGNED_STORAGE

/** An untyped array of data with compile-time alignment and size derived from another type. */
template<typename ElementType>
struct TTypeCompatibleBytes :
	public TAlignedBytes<
		sizeof(ElementType),
		ALIGNOF(ElementType)
		>
{};

template <typename AllocatorType>
struct TAllocatorTraitsBase
{
	enum { SupportsMove    = false };
	enum { IsZeroConstruct = false };
};

template <typename AllocatorType>
struct TAllocatorTraits : TAllocatorTraitsBase<AllocatorType>
{
};

/** This is the allocation policy interface; it exists purely to document the policy's interface, and should not be used. */
class FContainerAllocatorInterface
{
public:

	/** Determines whether the user of the allocator may use the ForAnyElementType inner class. */
	enum { NeedsElementType = true };
	enum { RequireRangeCheck = true };

	/**
	 * A class that receives both the explicit allocation policy template parameters specified by the user of the container,
	 * but also the implicit ElementType template parameter from the container type.
	 */
	template<typename ElementType>
	class ForElementType
	{
		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		void MoveToEmpty(ForElementType& Other);

		/** Accesses the container's current data. */
		ElementType* GetAllocation() const;

		/**
		 * Resizes the container's allocation.
		 * @param PreviousNumElements - The number of elements that were stored in the previous allocation.
		 * @param NumElements - The number of elements to allocate space for.
		 * @param NumBytesPerElement - The number of bytes/element.
		 */
		void ResizeAllocation(
			int32 PreviousNumElements,
			int32 NumElements,
			SIZE_T NumBytesPerElement
			);

		/**
		 * Calculates the amount of slack to allocate for an array that has just grown to a given number of elements.
		 * @param NumElements - The number of elements to allocate space for.
		 * @param CurrentNumSlackElements - The current number of slack elements allocated.
		 * @param NumBytesPerElement - The number of bytes/element.
		 */
		int32 CalculateSlack(
			int32 NumElements,
			int32 CurrentNumSlackElements,
			SIZE_T NumBytesPerElement
			) const;

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const;
	};

	/**
	 * A class that may be used when NeedsElementType=false is specified.
	 * If NeedsElementType=true, then this must be present but will not be used, and so can simply be a typedef to void
	 */
	typedef ForElementType<FScriptContainerElement> ForAnyElementType;
};

/** The indirect allocation policy always allocates the elements indirectly. */
template<uint32 Alignment = DEFAULT_ALIGNMENT>
class TAlignedHeapAllocator
{
public:

	enum { NeedsElementType = false };
	enum { RequireRangeCheck = true };

	class ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForAnyElementType()
			: Data(nullptr)
		{}

		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
		{
			check(this != &Other);

			if (Data)
			{
				FMemory::Free(Data);
			}

			Data       = Other.Data;
			Other.Data = nullptr;
		}

		/** Destructor. */
		FORCEINLINE ~ForAnyElementType()
		{
			if(Data)
			{
				FMemory::Free(Data);
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(
			int32 PreviousNumElements,
			int32 NumElements,
			SIZE_T NumBytesPerElement
			)
		{
			// Avoid calling FMemory::Realloc( nullptr, 0 ) as ANSI C mandates returning a valid pointer which is not what we want.
			if( Data || NumElements )
			{
				//checkSlow(((uint64)NumElements*(uint64)ElementTypeInfo.GetSize() < (uint64)INT_MAX));
				Data = (FScriptContainerElement*)FMemory::Realloc( Data, NumElements*NumBytesPerElement, Alignment );
			}
		}
		int32 CalculateSlack(
			int32 NumElements,
			int32 NumAllocatedElements,
			SIZE_T NumBytesPerElement
			) const
		{
			return DefaultCalculateSlack(NumElements,NumAllocatedElements,NumBytesPerElement);
		}

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

	private:
		ForAnyElementType(const ForAnyElementType&);
		ForAnyElementType& operator=(const ForAnyElementType&);

		/** A pointer to the container's elements. */
		FScriptContainerElement* Data;
	};

	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return (ElementType*)ForAnyElementType::GetAllocation();
		}
	};
};

template <uint32 Alignment>
struct TAllocatorTraits<TAlignedHeapAllocator<Alignment>> : TAllocatorTraitsBase<TAlignedHeapAllocator<Alignment>>
{
	enum { SupportsMove    = true };
	enum { IsZeroConstruct = true };
};

/** The indirect allocation policy always allocates the elements indirectly. */
class CORE_API FHeapAllocator
{
public:

	enum { NeedsElementType = false };
	enum { RequireRangeCheck = true };

	class CORE_API ForAnyElementType
	{
	public:
		/** Default constructor. */
		ForAnyElementType()
			: Data(nullptr)
		{}

		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
		{
			check(this != &Other);

			if (Data)
			{
				FMemory::Free(Data);
			}

			Data       = Other.Data;
			Other.Data = nullptr;
		}

		/** Destructor. */
		FORCEINLINE ~ForAnyElementType()
		{
			if(Data)
			{
				FMemory::Free(Data);
			}
		}

		// FContainerAllocatorInterface
		FORCEINLINE FScriptContainerElement* GetAllocation() const
		{
			return Data;
		}
		void ResizeAllocation(int32 PreviousNumElements,int32 NumElements,SIZE_T NumBytesPerElement)
		{
			// Avoid calling FMemory::Realloc( nullptr, 0 ) as ANSI C mandates returning a valid pointer which is not what we want.
			if( Data || NumElements )
			{
				//checkSlow(((uint64)NumElements*(uint64)ElementTypeInfo.GetSize() < (uint64)INT_MAX));
				Data = (FScriptContainerElement*)FMemory::Realloc( Data, NumElements*NumBytesPerElement );
			}
		}
		int32 CalculateSlack(int32 NumElements,int32 NumAllocatedElements,SIZE_T NumBytesPerElement) const
		{
			return DefaultCalculateSlack(NumElements,NumAllocatedElements,NumBytesPerElement);
		}

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return NumAllocatedElements * NumBytesPerElement;
		}

	private:
		ForAnyElementType(const ForAnyElementType&);
		ForAnyElementType& operator=(const ForAnyElementType&);

		/** A pointer to the container's elements. */
		FScriptContainerElement* Data;
	};
	
	template<typename ElementType>
	class ForElementType : public ForAnyElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{}

		FORCEINLINE ElementType* GetAllocation() const
		{
			return (ElementType*)ForAnyElementType::GetAllocation();
		}
	};
};

template <>
struct TAllocatorTraits<FHeapAllocator> : TAllocatorTraitsBase<FHeapAllocator>
{
	enum { SupportsMove    = true };
	enum { IsZeroConstruct = true };
};

class FDefaultAllocator;

/**
 * The inline allocation policy allocates up to a specified number of elements in the same allocation as the container.
 * Any allocation needed beyond that causes all data to be moved into an indirect allocation.
 * It always uses DEFAULT_ALIGNMENT.
 */
template <uint32 NumInlineElements, typename SecondaryAllocator = FDefaultAllocator>
class TInlineAllocator
{
public:

	enum { NeedsElementType = true };
	enum { RequireRangeCheck = true };

	template<typename ElementType>
	class ForElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{
		}

		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		FORCEINLINE void MoveToEmpty(ForElementType& Other)
		{
			check(this != &Other);

			if (!Other.SecondaryData.GetAllocation())
			{
				// Relocate objects from other inline storage only if it was stored inline in Other
				RelocateConstructItems<ElementType>((void*)InlineData, Other.GetInlineElements(), NumInlineElements);
			}

			// Move secondary storage in any case.
			// This will move secondary storage if it exists but will also handle the case where secondary storage is used in Other but not in *this.
			SecondaryData.MoveToEmpty(Other.SecondaryData);
		}

		// FContainerAllocatorInterface
		FORCEINLINE ElementType* GetAllocation() const
		{
			return IfAThenAElseB<ElementType>(SecondaryData.GetAllocation(),GetInlineElements());
		}

		void ResizeAllocation(int32 PreviousNumElements,int32 NumElements,SIZE_T NumBytesPerElement)
		{
			// Check if the new allocation will fit in the inline data area.
			if(NumElements <= NumInlineElements)
			{
				// If the old allocation wasn't in the inline data area, relocate it into the inline data area.
				if(SecondaryData.GetAllocation())
				{
					RelocateConstructItems<ElementType>((void*)InlineData, (ElementType*)SecondaryData.GetAllocation(), PreviousNumElements);

					// Free the old indirect allocation.
					SecondaryData.ResizeAllocation(0,0,NumBytesPerElement);
				}
			}
			else
			{
				if(!SecondaryData.GetAllocation())
				{
					// Allocate new indirect memory for the data.
					SecondaryData.ResizeAllocation(0,NumElements,NumBytesPerElement);

					// Move the data out of the inline data area into the new allocation.
					RelocateConstructItems<ElementType>((void*)SecondaryData.GetAllocation(), GetInlineElements(), PreviousNumElements);
				}
				else
				{
					// Reallocate the indirect data for the new size.
					SecondaryData.ResizeAllocation(PreviousNumElements,NumElements,NumBytesPerElement);
				}
			}
		}

		int32 CalculateSlack(int32 NumElements,int32 NumAllocatedElements,SIZE_T NumBytesPerElement) const
		{
			// If the elements use less space than the inline allocation, only use the inline allocation as slack.
			return NumElements <= NumInlineElements ?
				NumInlineElements :
				SecondaryData.CalculateSlack(NumElements,NumAllocatedElements,NumBytesPerElement);
		}

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return SecondaryData.GetAllocatedSize(NumAllocatedElements, NumBytesPerElement);
		}

	private:
		ForElementType(const ForElementType&);
		ForElementType& operator=(const ForElementType&);

		/** The data is stored in this array if less than NumInlineElements is needed. */
		TTypeCompatibleBytes<ElementType> InlineData[NumInlineElements];

		/** The data is allocated through the indirect allocation policy if more than NumInlineElements is needed. */
		typename SecondaryAllocator::template ForElementType<ElementType> SecondaryData;

		/** @return the base of the aligned inline element data */
		ElementType* GetInlineElements() const
		{
			return (ElementType*)InlineData;
		}
	};

	typedef void ForAnyElementType;
};

template <uint32 NumInlineElements, typename SecondaryAllocator>
struct TAllocatorTraits<TInlineAllocator<NumInlineElements, SecondaryAllocator>> : TAllocatorTraitsBase<TInlineAllocator<NumInlineElements, SecondaryAllocator>>
{
	enum { SupportsMove = TAllocatorTraits<SecondaryAllocator>::SupportsMove };
};

/**
 * The fixed allocation policy allocates up to a specified number of elements in the same allocation as the container.
 * It's like the inline allocator, except it doesn't provide secondary storage when the inline storage has been filled.
 */
template <uint32 NumInlineElements>
class TFixedAllocator
{
public:

	enum { NeedsElementType = true };
	enum { RequireRangeCheck = true };

	template<typename ElementType>
	class ForElementType
	{
	public:

		/** Default constructor. */
		ForElementType()
		{
		}

		/**
		 * Moves the state of another allocator into this one.
		 * Assumes that the allocator is currently empty, i.e. memory may be allocated but any existing elements have already been destructed (if necessary).
		 * @param Other - The allocator to move the state from.  This allocator should be left in a valid empty state.
		 */
		FORCEINLINE void MoveToEmpty(ForElementType& Other)
		{
			check(this != &Other);

			// Relocate objects from other inline storage
			RelocateConstructItems<ElementType>((void*)InlineData, Other.GetInlineElements(), NumInlineElements);
		}

		// FContainerAllocatorInterface
		FORCEINLINE ElementType* GetAllocation() const
		{
			return GetInlineElements();
		}

		void ResizeAllocation(int32 PreviousNumElements,int32 NumElements,SIZE_T NumBytesPerElement)
		{
			// Ensure the requested allocation will fit in the inline data area.
			check(NumElements <= NumInlineElements);
		}

		int32 CalculateSlack(int32 NumElements,int32 NumAllocatedElements,SIZE_T NumBytesPerElement) const
		{
			// Ensure the requested allocation will fit in the inline data area.
			check(NumElements <= NumInlineElements);

			return NumElements;
		}

		SIZE_T GetAllocatedSize(int32 NumAllocatedElements, SIZE_T NumBytesPerElement) const
		{
			return 0;
		}

	private:
		ForElementType(const ForElementType&);
		ForElementType& operator=(const ForElementType&);

		/** The data is stored in this array if less than NumInlineElements is needed. */
		TTypeCompatibleBytes<ElementType> InlineData[NumInlineElements];

		/** @return the base of the aligned inline element data */
		ElementType* GetInlineElements() const
		{
			return (ElementType*)InlineData;
		}
	};

	typedef void ForAnyElementType;
};

template <uint32 NumInlineElements>
struct TAllocatorTraits<TFixedAllocator<NumInlineElements>> : TAllocatorTraitsBase<TFixedAllocator<NumInlineElements>>
{
	enum { SupportsMove = true };
};


enum { NumBitsPerDWORD = 32 };
enum { NumBitsPerDWORDLogTwo = 5 };

//
// Sparse array allocation definitions
//

class FDefaultAllocator;
class FDefaultBitArrayAllocator;

/** Encapsulates the allocators used by a sparse array in a single type. */
template<typename InElementAllocator = FDefaultAllocator,typename InBitArrayAllocator = FDefaultBitArrayAllocator>
class TSparseArrayAllocator
{
public:

	typedef InElementAllocator ElementAllocator;
	typedef InBitArrayAllocator BitArrayAllocator;
};

/** An inline sparse array allocator that allows sizing of the inline allocations for a set number of elements. */
template<
	uint32 NumInlineElements,
	typename SecondaryAllocator = TSparseArrayAllocator<FDefaultAllocator,FDefaultAllocator>
	>
class TInlineSparseArrayAllocator
{
private:

	/** The size to allocate inline for the bit array. */
	enum { InlineBitArrayDWORDs = (NumInlineElements + NumBitsPerDWORD - 1) / NumBitsPerDWORD};

public:

	typedef TInlineAllocator<NumInlineElements,typename SecondaryAllocator::ElementAllocator>		ElementAllocator;
	typedef TInlineAllocator<InlineBitArrayDWORDs,typename SecondaryAllocator::BitArrayAllocator>	BitArrayAllocator;
};

//
// Set allocation definitions.
//

#define DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET	2
#define DEFAULT_BASE_NUMBER_OF_HASH_BUCKETS			8
#define DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS		4

/** Encapsulates the allocators used by a set in a single type. */
template<
	typename InSparseArrayAllocator               = TSparseArrayAllocator<>,
	typename InHashAllocator                      = TInlineAllocator<1,FDefaultAllocator>,
	uint32   AverageNumberOfElementsPerHashBucket = DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET,
	uint32   BaseNumberOfHashBuckets              = DEFAULT_BASE_NUMBER_OF_HASH_BUCKETS,
	uint32   MinNumberOfHashedElements            = DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS
	>
class TSetAllocator
{
public:

	/** Computes the number of hash buckets to use for a given number of elements. */
	static FORCEINLINE uint32 GetNumberOfHashBuckets(uint32 NumHashedElements)
	{
		if(NumHashedElements >= MinNumberOfHashedElements)
		{
			return FMath::RoundUpToPowerOfTwo(NumHashedElements / AverageNumberOfElementsPerHashBucket + BaseNumberOfHashBuckets);
		}

		return 1;
	}

	typedef InSparseArrayAllocator SparseArrayAllocator;
	typedef InHashAllocator        HashAllocator;
};

class FDefaultAllocator;

/** An inline set allocator that allows sizing of the inline allocations for a set number of elements. */
template<
	uint32   NumInlineElements,
	typename SecondaryAllocator                   = TSetAllocator<TSparseArrayAllocator<FDefaultAllocator,FDefaultAllocator>,FDefaultAllocator>,
	uint32   AverageNumberOfElementsPerHashBucket = DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET,
	uint32   MinNumberOfHashedElements            = DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS
	>
class TInlineSetAllocator
{
private:

	enum { NumInlineHashBuckets = (NumInlineElements + AverageNumberOfElementsPerHashBucket - 1) / AverageNumberOfElementsPerHashBucket };

public:

	/** Computes the number of hash buckets to use for a given number of elements. */
	static FORCEINLINE uint32 GetNumberOfHashBuckets(uint32 NumHashedElements)
	{
		const uint32 NumDesiredHashBuckets = FMath::RoundUpToPowerOfTwo(NumHashedElements / AverageNumberOfElementsPerHashBucket);
		if (NumDesiredHashBuckets < NumInlineHashBuckets)
		{
			return NumInlineHashBuckets;
		}

		if (NumHashedElements < MinNumberOfHashedElements)
		{
			return NumInlineHashBuckets;
		}

		return NumDesiredHashBuckets;
	}

	typedef TInlineSparseArrayAllocator<NumInlineElements,typename SecondaryAllocator::SparseArrayAllocator> SparseArrayAllocator;
	typedef TInlineAllocator<NumInlineHashBuckets,typename SecondaryAllocator::HashAllocator>                HashAllocator;
};


/**
 * 'typedefs' for various allocator defaults.
 *
 * These should be replaced with actual typedefs when Core.h include order is sorted out, as then we won't need to
 * 'forward' these TAllocatorTraits specializations below.
 */

class FDefaultAllocator            : public FHeapAllocator          { public: typedef FHeapAllocator          Typedef; };
class FDefaultSetAllocator         : public TSetAllocator<>         { public: typedef TSetAllocator<>         Typedef; };
class FDefaultBitArrayAllocator    : public TInlineAllocator<4>     { public: typedef TInlineAllocator<4>     Typedef; };
class FDefaultSparseArrayAllocator : public TSparseArrayAllocator<> { public: typedef TSparseArrayAllocator<> Typedef; };

template <> struct TAllocatorTraits<FDefaultAllocator>            : TAllocatorTraits<typename FDefaultAllocator           ::Typedef> {};
template <> struct TAllocatorTraits<FDefaultSetAllocator>         : TAllocatorTraits<typename FDefaultSetAllocator        ::Typedef> {};
template <> struct TAllocatorTraits<FDefaultBitArrayAllocator>    : TAllocatorTraits<typename FDefaultBitArrayAllocator   ::Typedef> {};
template <> struct TAllocatorTraits<FDefaultSparseArrayAllocator> : TAllocatorTraits<typename FDefaultSparseArrayAllocator::Typedef> {};
