// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContext.h"

#include "BlueprintContextBase.h"

UBlueprintContextBase::UBlueprintContextBase()
    : LocalPlayerIndex( INDEX_NONE )
{
}

void UBlueprintContextBase::PostInitProperties()
{
	Super::PostInitProperties();

	if ( HasAnyFlags( RF_ClassDefaultObject ) )
	{
		return;
	}

	if ( auto Owner = GetOwningPlayer() )
	{
		LocalPlayerIndex = Owner->GetControllerId();
	}

	Initialize();
}

void UBlueprintContextBase::BeginDestroy()
{
	Super::BeginDestroy();

	if ( HasAnyFlags( RF_ClassDefaultObject ) )
	{
		return;
	}

	Finalize();

	LocalPlayerIndex = INDEX_NONE;
}

void UBlueprintContextBase::Initialize()
{
	// stub
}

void UBlueprintContextBase::Finalize()
{
	// stub
}

void UBlueprintContextBase::PostInitWorld( UWorld* World )
{
	// stub
}

void UBlueprintContextBase::OnWorldCleanup( UWorld* World )
{
	// stub
}

void UBlueprintContextBase::PreDestructContext( UWorld* World )
{
	// stub
}

UWorld* UBlueprintContextBase::GetWorld() const
{
	if ( auto Game = GetGameInstance() )
	{
		return Game->GetWorld();
	}

	return nullptr;
}

UBlueprintContextBase* UBlueprintContextBase::GetContextPrivate( TSubclassOf<UBlueprintContextBase> Class ) const
{
	return FBlueprintContextManager::Get( GetGameInstance<UGameInstance>() ).GetContextFor( LocalPlayerIndex, Class );
}
