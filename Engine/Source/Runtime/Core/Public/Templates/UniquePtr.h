// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealTemplate.h"
#include "Templates/EnableIf.h"

// Single-ownership smart pointer in the vein of std::unique_ptr.
// Use this when you need an object's lifetime to be strictly bound to the lifetime of a single smart pointer.
//
// This class is non-copyable - ownership can only be transferred via a move operation, e.g.:
//
// TUniquePtr<MyClass> Ptr1(new MyClass);    // The MyClass object is owned by Ptr1.
// TUniquePtr<MyClass> Ptr2(Ptr1);           // Error - TUniquePtr is not copyable
// TUniquePtr<MyClass> Ptr3(MoveTemp(Ptr1)); // Ptr3 now owns the MyClass object - Ptr1 is now nullptr.

template <typename T>
struct TDefaultDelete
{
	TDefaultDelete()
	{
	}

	template <typename U, typename = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	TDefaultDelete(TDefaultDelete<U>&&)
	{
	}

	template <typename U, typename = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	TDefaultDelete& operator=(TDefaultDelete<U>&&)
	{
		return *this;
	}

	~TDefaultDelete()
	{
	}

	void operator()(T* Ptr) const
	{
		delete Ptr;
	}
};

template <typename T, typename Deleter = TDefaultDelete<T>>
class TUniquePtr : private Deleter
{
	template <typename OtherT, typename OtherDeleter>
	friend class TUniquePtr;

public:
	/**
	 * Default constructor - initializes the TUniquePtr to null.
	 */
	FORCEINLINE TUniquePtr()
		: Ptr(nullptr)
	{
	}

	/**
	 * Pointer constructor - takes ownership of the pointed-to object
	 *
	 * @param InPtr The pointed-to object to take ownership of.
	 */
	explicit FORCEINLINE TUniquePtr(T* InPtr)
		: Ptr(InPtr)
	{
	}

	/**
	 * nullptr constructor - initializes the TUniquePtr to null.
	 */
	FORCEINLINE TUniquePtr(TYPE_OF_NULLPTR)
		: Ptr(nullptr)
	{
	}

	/**
	 * Move constructor
	 */
	FORCEINLINE TUniquePtr(TUniquePtr&& Other)
		: Deleter(MoveTemp(Other.GetDeleter()))
		, Ptr    (Other.Ptr)
	{
		Other.Ptr = nullptr;
	}

	/**
	 * Constructor from rvalues of other (usually derived) types
	 */
	template <typename OtherT, typename OtherDeleter>
	FORCEINLINE TUniquePtr(TUniquePtr<OtherT, OtherDeleter>&& Other)
		: Deleter(MoveTemp(Other.GetDeleter()))
		, Ptr    (Other.Ptr)
	{
		Other.Ptr = nullptr;
	}

	/**
	 * Move assignment operator
	 */
	FORCEINLINE TUniquePtr& operator=(TUniquePtr&& Other)
	{
		if (this != &Other)
		{
			// We delete last, because we don't want odd side effects if the destructor of T relies on the state of this or Other
			T* OldPtr = Ptr;
			Ptr = Other.Ptr;
			Other.Ptr = nullptr;
			GetDeleter()(OldPtr);
		}

		GetDeleter() = MoveTemp(Other.GetDeleter());

		return *this;
	}

	/**
	 * Assignment operator for rvalues of other (usually derived) types
	 */
	template <typename OtherT, typename OtherDeleter>
	FORCEINLINE TUniquePtr& operator=(TUniquePtr<OtherT>&& Other)
	{
		// We delete last, because we don't want odd side effects if the destructor of T relies on the state of this or Other
		T* OldPtr = Ptr;
		Ptr = Other.Ptr;
		Other.Ptr = nullptr;
		GetDeleter()(OldPtr);

		GetDeleter() = MoveTemp(Other.GetDeleter());

		return *this;
	}

	/**
	 * Nullptr assignment operator
	 */
	FORCEINLINE TUniquePtr& operator=(TYPE_OF_NULLPTR)
	{
		// We delete last, because we don't want odd side effects if the destructor of T relies on the state of this
		T* OldPtr = Ptr;
		Ptr = nullptr;
		GetDeleter()(OldPtr);

		return *this;
	}

	/**
	 * Destructor
	 */
	FORCEINLINE ~TUniquePtr()
	{
		GetDeleter()(Ptr);
	}

	/**
	 * Tests if the TUniquePtr currently owns an object.
	 *
	 * @return true if the TUniquePtr currently owns an object, false otherwise.
	 */
	bool IsValid() const
	{
		return Ptr != nullptr;
	}

	/**
	 * operator bool
	 *
	 * @return true if the TUniquePtr currently owns an object, false otherwise.
	 */
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/**
	 * Logical not operator
	 *
	 * @return false if the TUniquePtr currently owns an object, true otherwise.
	 */
	FORCEINLINE bool operator!() const
	{
		return !IsValid();
	}

	/**
	 * Indirection operator
	 *
	 * @return A pointer to the object owned by the TUniquePtr.
	 */
	FORCEINLINE T* operator->() const
	{
		return Ptr;
	}

	/**
	 * Dereference operator
	 *
	 * @return A reference to the object owned by the TUniquePtr.
	 */
	FORCEINLINE T& operator*() const
	{
		return *Ptr;
	}

	/**
	 * Returns a pointer to the owned object without relinquishing ownership.
	 *
	 * @return A copy of the pointer to the object owned by the TUniquePtr, or nullptr if no object is being owned.
	 */
	FORCEINLINE T* Get() const
	{
		return Ptr;
	}

	/**
	 * Relinquishes control of the owned object to the caller and nulls the TUniquePtr.
	 *
	 * @return The pointer to the object that was owned by the TUniquePtr, or nullptr if no object was being owned.
	 */
	FORCEINLINE T* Release()
	{
		T* Result = Ptr;
		Ptr = nullptr;
		return Result;
	}

	/**
	 * Gives the TUniquePtr a new object to own, destroying any previously-owned object.
	 *
	 * @param InPtr A pointer to the object to take ownership of.
	 */
	FORCEINLINE void Reset(T* InPtr = nullptr)
	{
		// We delete last, because we don't want odd side effects if the destructor of T relies on the state of this
		T* OldPtr = Ptr;
		Ptr = InPtr;
		GetDeleter()(OldPtr);
	}

	/**
	 * Returns a reference to the deleter subobject.
	 *
	 * @return A reference to the deleter.
	 */
	FORCEINLINE Deleter& GetDeleter()
	{
		return static_cast<Deleter&>(*this);
	}

	/**
	 * Returns a reference to the deleter subobject.
	 *
	 * @return A reference to the deleter.
	 */
	FORCEINLINE const Deleter& GetDeleter() const
	{
		return static_cast<const Deleter&>(*this);
	}

private:
	// Non-copyable
	TUniquePtr(const TUniquePtr&);
	TUniquePtr& operator=(const TUniquePtr&);

	T* Ptr;
};

/**
 * Equality comparison operator
 *
 * @param Lhs The first TUniquePtr to compare.
 * @param Rhs The second TUniquePtr to compare.
 *
 * @return true if the two TUniquePtrs are logically substitutable for each other, false otherwise.
 */
template <typename LhsT, typename RhsT>
FORCEINLINE bool operator==(const TUniquePtr<LhsT>& Lhs, const TUniquePtr<RhsT>& Rhs)
{
	return Lhs.Get() == Rhs.Get();
}
template <typename T>
FORCEINLINE bool operator==(const TUniquePtr<T>& Lhs, const TUniquePtr<T>& Rhs)
{
	return Lhs.Get() == Rhs.Get();
}

/**
 * Inequality comparison operator
 *
 * @param Lhs The first TUniquePtr to compare.
 * @param Rhs The second TUniquePtr to compare.
 *
 * @return false if the two TUniquePtrs are logically substitutable for each other, true otherwise.
 */
template <typename LhsT, typename RhsT>
FORCEINLINE bool operator!=(const TUniquePtr<LhsT>& Lhs, const TUniquePtr<RhsT>& Rhs)
{
	return Lhs.Get() != Rhs.Get();
}
template <typename T>
FORCEINLINE bool operator!=(const TUniquePtr<T>& Lhs, const TUniquePtr<T>& Rhs)
{
	return Lhs.Get() != Rhs.Get();
}

/**
 * Equality comparison operator against nullptr.
 *
 * @param Lhs The TUniquePtr to compare.
 *
 * @return true if the TUniquePtr is null, false otherwise.
 */
template <typename T>
FORCEINLINE bool operator==(const TUniquePtr<T>& Lhs, TYPE_OF_NULLPTR)
{
	return !Lhs.IsValid();
}
template <typename T>
FORCEINLINE bool operator==(TYPE_OF_NULLPTR, const TUniquePtr<T>& Rhs)
{
	return !Rhs.IsValid();
}

/**
 * Inequality comparison operator against nullptr.
 *
 * @param Rhs The TUniquePtr to compare.
 *
 * @return true if the TUniquePtr is not null, false otherwise.
 */
template <typename T>
FORCEINLINE bool operator!=(const TUniquePtr<T>& Lhs, TYPE_OF_NULLPTR)
{
	return Lhs.IsValid();
}
template <typename T>
FORCEINLINE bool operator!=(TYPE_OF_NULLPTR, const TUniquePtr<T>& Rhs)
{
	return Rhs.IsValid();
}

// Trait which allows TUniquePtr to be default constructed by memsetting to zero.
template <typename T>
struct TIsZeroConstructType<TUniquePtr<T>>
{
	enum { Value = true };
};

// Trait which allows TUniquePtr to be memcpy'able from pointers.
template <typename T>
struct TIsBitwiseConstructible<TUniquePtr<T>, T*>
{
	enum { Value = true };
};

/**
 * Constructs a new object with the given arguments and returns it as a TUniquePtr.
 *
 * @param Args The arguments to pass to the constructor of T.
 *
 * @return A TUniquePtr which points to a newly-constructed T with the specified Args.
 */
template <typename T, typename... TArgs>
FORCEINLINE TUniquePtr<T> MakeUnique(TArgs&&... Args)
{
	return TUniquePtr<T>(new T(Forward<TArgs>(Args)...));
}
