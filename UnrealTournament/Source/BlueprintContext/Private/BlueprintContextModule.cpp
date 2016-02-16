// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContext.h"

#include "ModuleManager.h"
#include "BlueprintContextBase.h"
#include "Engine/LocalPlayer.h"
#include "BlueprintContextManager.h"

DEFINE_LOG_CATEGORY( LogBlueprintContext );

class FBlueprintContextModule : public IBlueprintContextModule
{
public:
	FBlueprintContextModule() {}

	virtual void LocalPlayerAdded( ULocalPlayer* NewPlayer ) override
	{
		UE_LOG( LogBlueprintContext, Display, TEXT( "Creating blueprint contexts for Local Player ' %s ' of Game Instance ' %s '" ), *NewPlayer->GetName(), *NewPlayer->GetGameInstance()->GetName() );
		FBlueprintContextManager::Get(NewPlayer->GetGameInstance()).ConstructContextsForPlayer( NewPlayer );
	}

	virtual void LocalPlayerRemoved( ULocalPlayer* OldPlayer ) override 
	{
		UE_LOG( LogBlueprintContext, Display, TEXT( "Destroying blueprint contexts for Local Player ' %s ' of Game Instance ' %s '" ), *OldPlayer->GetName(), *OldPlayer->GetGameInstance()->GetName() );
		FBlueprintContextManager::Get(OldPlayer->GetGameInstance()).DestructContextsForPlayer( OldPlayer );
	}
};

IMPLEMENT_MODULE( FBlueprintContextModule, BlueprintContext );