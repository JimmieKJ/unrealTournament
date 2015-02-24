// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Types of delegate instances
 */
namespace EDelegateInstanceType
{
	enum Type
	{
		/** Member function pointer to method in (fast, not thread-safe) shared pointer-based class */
		SharedPointerMethod,

		/** Member function pointer to method in (conditionally thread-safe) shared pointer-based class */
		ThreadSafeSharedPointerMethod,

		/** Raw C++ member function pointer (pointer to class method) */
		RawMethod,

		/** UFunction delegate */
		UFunction,

		/** Member function pointer to method in UObject-based class */
		UObjectMethod,

		/** Raw C++ static function pointer */
		Raw,

		/** C++ functor, e.g. Lambda */
		Functor,
	};
}


/**
 * Class representing an handle to a delegate.
 */
class FDelegateHandle
{
public:
	enum EGenerateNewHandleType
	{
		GenerateNewHandle
	};

	FDelegateHandle()
		: ID(0)
	{
	}

	explicit FDelegateHandle(EGenerateNewHandleType)
		: ID(GenerateNewID())
	{
	}

	bool IsValid() const
	{
		return ID != 0;
	}

	void Reset()
	{
		ID = 0;
	}

private:
	friend bool operator==(const FDelegateHandle& Lhs, const FDelegateHandle& Rhs)
	{
		return Lhs.ID == Rhs.ID;
	}

	friend bool operator!=(const FDelegateHandle& Lhs, const FDelegateHandle& Rhs)
	{
		return Lhs.ID != Rhs.ID;
	}

	/**
	 * Generates a new ID for use the delegate handle.
	 *
	 * @return A unique ID for the delegate.
	 */
	static CORE_API uint64 GenerateNewID();

	uint64 ID;
};


/**
 * Interface for delegate instances.
 */
class IDelegateInstance
{
public:

	/**
	 * Returns the name of the UFunction that this delegate instance is bound to.
	 *
	 * @return Name of the function, or NAME_None if not bound to a UFunction.
	 */
	virtual FName GetFunctionName( ) const = 0;

	/**
	 * Returns raw pointer to the delegate method.
	 *
	 * @return Raw pointer to the delegate method.
	 */
	virtual const void* GetRawMethodPtr( ) const = 0;

	/**
	 * Returns raw pointer to UserObject,
	 *
	 * @return Raw pointer to UserObject.
	 */
	virtual const void* GetRawUserObject( ) const = 0;

	/**
	 * Returns the type of delegate instance
	 *
	 * @return Delegate instance type
	 */
	virtual EDelegateInstanceType::Type GetType( ) const = 0;

	/**
	 * Returns true if this delegate is bound to the specified UserObject,
	 *
	 * @param InUserObject
	 *
	 * @return True if delegate is bound to the specified UserObject
	 */
	virtual bool HasSameObject( const void* InUserObject ) const = 0;

	/**
	 * Checks to see if the user object bound to this delegate can ever be valid again.
	 * used to compact multicast delegate arrays so they don't expand without limit.
	 *
	 * @return True if the user object can never be used again
	 */
	virtual bool IsCompactable( ) const
	{
		return !IsSafeToExecute();
	}

	/**
	 * Checks to see if the user object bound to this delegate is still valid
	 *
	 * @return True if the user object is still valid and it's safe to execute the function call
	 */
	virtual bool IsSafeToExecute( ) const = 0;

	/**
	 * Returns a handle for the delegate.
	 */
	virtual FDelegateHandle GetHandle() const = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IDelegateInstance( ) { }
};
