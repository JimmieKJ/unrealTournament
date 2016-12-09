// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CookerSettings.h"
#include "UObject/UnrealType.h"

void UCookerSettings::PostInitProperties()
{
	Super::PostInitProperties();
	UObject::UpdateClassesExcludedFromDedicatedServer(ClassesExcludedOnDedicatedServer);
	UObject::UpdateClassesExcludedFromDedicatedClient(ClassesExcludedOnDedicatedClient);
}

void UCookerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static FName NAME_ClassesExcludedOnDedicatedServer(TEXT("ClassesExcludedOnDedicatedServer"));
	static FName NAME_ClassesExcludedOnDedicatedClient(TEXT("ClassesExcludedOnDedicatedClient"));

	if(PropertyChangedEvent.Property)
	{
		if(PropertyChangedEvent.Property->GetFName() == NAME_ClassesExcludedOnDedicatedServer)
		{
			UObject::UpdateClassesExcludedFromDedicatedServer(ClassesExcludedOnDedicatedServer);
		}
		else if(PropertyChangedEvent.Property->GetFName() == NAME_ClassesExcludedOnDedicatedClient)
		{
			UObject::UpdateClassesExcludedFromDedicatedClient(ClassesExcludedOnDedicatedClient);
		}
	}
}
