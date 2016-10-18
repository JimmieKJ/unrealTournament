// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace FGameplayTasks
{
	static const uint8 DefaultPriority = 127;
	static const uint8 ScriptedPriority = 192;
}

template<class TInterface>
struct TWeakInterfacePtr
{
	TWeakInterfacePtr() : InterfaceInstance(nullptr) {}

	TWeakInterfacePtr(const UObject* Object)
	{
		InterfaceInstance = Cast<TInterface>(Object);
		if (InterfaceInstance != nullptr)
		{
			ObjectInstance = Object;
		}
	}

	TWeakInterfacePtr(TInterface& Interace)
	{
		InterfaceInstance = &Interace;
		ObjectInstance = Cast<UObject>(&Interace);
	}

	bool IsValid(bool bEvenIfPendingKill = false, bool bThreadsafeTest = false) const
	{
		return InterfaceInstance != nullptr && ObjectInstance.IsValid();
	}

	FORCEINLINE TInterface& operator*() const
	{
		return *InterfaceInstance;
	}

	/**
	* Dereference the weak pointer
	**/
	FORCEINLINE TInterface* operator->() const
	{
		return InterfaceInstance;
	}

	bool operator==(const UObject* Other)
	{
		return Other == ObjectInstance.Get();
	}

private:
	TWeakObjectPtr<UObject> ObjectInstance;
	TInterface* InterfaceInstance;
};
