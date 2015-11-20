// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContainerAllocationPolicies.h"

#ifndef WIN32
	// Let delegates store up to 32 bytes which are 16-byte aligned before we heap allocate
	typedef TAlignedBytes<16, 16> AlignedInlineDelegateType;
	typedef TInlineAllocator<2> DelegateAllocatorType;
#else
	// ... except on Win32, because we can't pass 16-byte aligned types by value, as some delegates are
	// so we'll just keep it heap-allocated, which are always sufficiently aligned.
	typedef TAlignedBytes<16, 8> AlignedInlineDelegateType;
	typedef FHeapAllocator DelegateAllocatorType;
#endif


/**
 * Base class for unicast delegates.
 */
class FDelegateBase
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InDelegateInstance The delegate instance to assign.
	 */
	explicit FDelegateBase()
		: DelegateSize(0)
	{
	}

	/**
	 * Move constructor.
	 */
	FDelegateBase(FDelegateBase&& Other)
	{
		DelegateAllocator.MoveToEmpty(Other.DelegateAllocator);
		DelegateSize = Other.DelegateSize;
		Other.DelegateSize = 0;
	}

	/**
	 * Move assignment.
	 */
	FDelegateBase& operator=(FDelegateBase&& Other)
	{
		Unbind();
		DelegateAllocator.MoveToEmpty(Other.DelegateAllocator);
		DelegateSize = Other.DelegateSize;
		Other.DelegateSize = 0;
		return *this;
	}

	/**
	 * If this is a UFunction or UObject delegate, return the UObject.
	 *
	 * @return The object associated with this delegate if there is one.
	 */
	inline class UObject* GetUObject( ) const
	{
		if (IDelegateInstance* Ptr = GetDelegateInstance())
		{
			if (Ptr->GetType() == EDelegateInstanceType::UFunction || Ptr->GetType() == EDelegateInstanceType::UObjectMethod)
			{
				return (class UObject*)Ptr->GetRawUserObject();
			}
		}

		return nullptr;
	}

	/**
	 * Checks to see if the user object bound to this delegate is still valid.
	 *
	 * @return True if the user object is still valid and it's safe to execute the function call.
	 */
	inline bool IsBound( ) const
	{
		IDelegateInstance* Ptr = GetDelegateInstance();

		return Ptr && Ptr->IsSafeToExecute();
	}

	/** 
	 * Checks to see if this delegate is bound to the given user object.
	 *
	 * @return True if this delegate is bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject( void const* InUserObject ) const
	{
		if (!InUserObject)
		{
			return false;
		}

		IDelegateInstance* Ptr = GetDelegateInstance();

		return Ptr && Ptr->HasSameObject(InUserObject);
	}

	/**
	 * Unbinds this delegate
	 */
	inline void Unbind( )
	{
		if (IDelegateInstance* Ptr = GetDelegateInstance())
		{
			Ptr->~IDelegateInstance();
			DelegateAllocator.ResizeAllocation(0, 0, sizeof(AlignedInlineDelegateType));
			DelegateSize = 0;
		}
	}

	/**
	 * Gets the delegate instance.
	 *
	 * @return The delegate instance.
	 * @see SetDelegateInstance
	 */
	IDelegateInstance* GetDelegateInstance( ) const
	{
		return DelegateSize ? (IDelegateInstance*)DelegateAllocator.GetAllocation() : nullptr;
	}

	/**
	 * Gets a handle to the delegate.
	 *
	 * @return The delegate instance.
	 */
	inline FDelegateHandle GetHandle() const
	{
		FDelegateHandle Result;
		if (IDelegateInstance* Ptr = GetDelegateInstance())
		{
			Result = Ptr->GetHandle();
		}

		return Result;
	}

private:
	friend void* operator new(size_t Size, FDelegateBase& Base);

	void* Allocate(int32 Size)
	{
		if (IDelegateInstance* CurrentInstance = GetDelegateInstance())
		{
			CurrentInstance->~IDelegateInstance();
		}

		int32 NewDelegateSize = FMath::DivideAndRoundUp(Size, (int32)sizeof(AlignedInlineDelegateType));
		if (DelegateSize != NewDelegateSize)
		{
			DelegateAllocator.ResizeAllocation(0, NewDelegateSize, sizeof(AlignedInlineDelegateType));
			DelegateSize = NewDelegateSize;
		}

		return DelegateAllocator.GetAllocation();
	}

private:
	DelegateAllocatorType::ForElementType<AlignedInlineDelegateType> DelegateAllocator;
	int32 DelegateSize;
};

inline void* operator new(size_t Size, FDelegateBase& Base)
{
	return Base.Allocate(Size);
}
