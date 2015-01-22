// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ClassViewerPrivatePCH.h"

#include "UnloadedBlueprintData.h"
#include "ClassViewerNode.h"
#include "Engine/BlueprintGeneratedClass.h"

FUnloadedBlueprintData::FUnloadedBlueprintData( TWeakPtr< FClassViewerNode > InClassViewerNode )
{
	ClassViewerNode = InClassViewerNode;

	ClassFlags = CLASS_None;
}

bool FUnloadedBlueprintData::HasAnyClassFlags( uint32 InFlagsToCheck ) const
{
	return (ClassFlags & InFlagsToCheck) != 0;
}

bool FUnloadedBlueprintData::HasAllClassFlags( uint32 InFlagsToCheck ) const
{
	return ((ClassFlags & InFlagsToCheck) == InFlagsToCheck);
}

void FUnloadedBlueprintData::SetClassFlags(uint32 InFlags)
{
	ClassFlags = InFlags;
}

bool FUnloadedBlueprintData::IsChildOf(const UClass* InClass) const
{
	TSharedPtr< FClassViewerNode > CurrentNode = ClassViewerNode.Pin()->ParentNode.Pin();

	// Keep going through parents till you find an invalid.
	while(CurrentNode.IsValid())
	{
		if(*CurrentNode->GetClassName() == InClass->GetName())
		{
			return true;
		}
		CurrentNode = CurrentNode->ParentNode.Pin();
	}

	return false;
}

bool FUnloadedBlueprintData::ImplementsInterface(const UClass* InInterface) const
{
	for(int InterfaceIdx = 0; InterfaceIdx < ImplementedInterfaces.Num(); ++InterfaceIdx)
	{
		if(ImplementedInterfaces[InterfaceIdx] == InInterface->GetName())
		{
			return true;
		}
	}

	return false;
}

bool FUnloadedBlueprintData::IsA(const UClass* InClass) const
{
	// Unloaded blueprints will always return true for IsA(UBlueprintGeneratedClass::StaticClass). With that in mind, even though we do not have the exact class, we can use that knowledge as a basis for a check.
	return ((UObject*)UBlueprintGeneratedClass::StaticClass())->IsA(InClass);
}

const TWeakPtr< class FClassViewerNode > FUnloadedBlueprintData::GetClassViewerNode() const
{
	return ClassViewerNode;
}

void FUnloadedBlueprintData::AddImplementedInterfaces(FString& InterfaceName)
{
	ImplementedInterfaces.Add(InterfaceName);
}
