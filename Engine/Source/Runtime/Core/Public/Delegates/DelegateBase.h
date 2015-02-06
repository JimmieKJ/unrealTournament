// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Base class for unicast delegates.
 */
template<typename ObjectPtrType = FWeakObjectPtr>
class FDelegateBase
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InDelegateInstance The delegate instance to assign.
	 */
	FDelegateBase( IDelegateInstance* InDelegateInstance )
		: DelegateInstance(InDelegateInstance)
	{ }

	/**
	 * If this is a UFunction or UObject delegate, return the UObject.
	 *
	 * @return The object associated with this delegate if there is one.
	 */
	inline class UObject* GetUObject( ) const
	{
		if (DelegateInstance != nullptr)
		{
			if ((DelegateInstance->GetType() == EDelegateInstanceType::UFunction) ||
				(DelegateInstance->GetType() == EDelegateInstanceType::UObjectMethod))
			{
				return (class UObject*)DelegateInstance->GetRawUserObject();
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
		return (DelegateInstance != nullptr) && (DelegateInstance->IsSafeToExecute());
	}

	/** 
	 * Checks to see if this delegate is bound to the given user object.
	 *
	 * @return True if this delegate is bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject( void const* InUserObject ) const
	{
		return (InUserObject != nullptr) && (DelegateInstance != nullptr) && DelegateInstance->HasSameObject(InUserObject);
	}

	/**
	 * Unbinds this delegate
	 */
	inline void Unbind( )
	{
		if (DelegateInstance != nullptr)
		{
			delete DelegateInstance;
			DelegateInstance = nullptr;
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
		return DelegateInstance;
	}

	/**
	 * Sets the delegate instance.
	 *
	 * @param InDelegateInstance The delegate instance to set.
	 * @see GetDelegateInstance.
	 */
	inline void SetDelegateInstance( IDelegateInstance* InDelegateInstance )
	{
		Unbind();
		DelegateInstance = InDelegateInstance;
	}

	/**
	 * Gets a handle to the delegate.
	 *
	 * @return The delegate instance.
	 */
	inline FDelegateHandle GetHandle() const
	{
		FDelegateHandle Result;
		if (DelegateInstance)
		{
			Result = DelegateInstance->GetHandle();
		}

		return Result;
	}

private:

	// Holds the delegate instance
	IDelegateInstance* DelegateInstance;
};
