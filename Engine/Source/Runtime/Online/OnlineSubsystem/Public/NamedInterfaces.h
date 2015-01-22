// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//
// Named interfaces for Online Subsystem
//

#pragma once
#include "NamedInterfaces.generated.h"

/** Holds a named object interface for dynamically bound interfaces */
USTRUCT()
struct FNamedInterface
{
	GENERATED_USTRUCT_BODY()

	FNamedInterface() :
		InterfaceName(NAME_None),
		InterfaceObject(NULL)
	{
	}

	/** The name to bind this object to */
	UPROPERTY()
	FName InterfaceName;
	/** The object to store at this location */
	UPROPERTY()
	UObject* InterfaceObject;
};

/** Holds a name to class name mapping for adding the named interfaces automatically */
USTRUCT()
struct FNamedInterfaceDef
{
	GENERATED_USTRUCT_BODY()

	FNamedInterfaceDef() : 
		InterfaceName(NAME_None)
	{
	}

	/** The name to bind this object to */
	UPROPERTY()
	FName InterfaceName;
	/** The class to load and create for the named interface */
	UPROPERTY()
	FString InterfaceClassName;
};

/**
 * Named interfaces are a registry of UObjects accessible by an FName key that will persist for the lifetime of the process
 */
UCLASS(transient, config=Engine)
class UNamedInterfaces: public UObject
{
	GENERATED_UCLASS_BODY()

	void Initialize();
	class UObject* GetNamedInterface(FName InterfaceName) const;
	void SetNamedInterface(FName InterfaceName, class UObject* NewInterface);

private:

	/** Holds the set of registered named interfaces */
	UPROPERTY()
	TArray<FNamedInterface> NamedInterfaces;

	/** The list of named interfaces to automatically create and store */
	UPROPERTY(config)
	TArray<FNamedInterfaceDef> NamedInterfaceDefs;
};

