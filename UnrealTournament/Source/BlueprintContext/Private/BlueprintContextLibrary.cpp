// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintContext.h"

#include "BlueprintContextLibrary.h"

/*static*/ UBlueprintContextBase* UBlueprintContextLibrary::GetContext( UObject* ContextObject, TSubclassOf<UBlueprintContextBase> Class )
{
	int32 LocalPlayerId         = INDEX_NONE;
	UGameInstance* GameInstance = nullptr;
	if ( UUserWidget* UserWidget = Cast<UUserWidget>( ContextObject ) )
	{
		if ( ULocalPlayer* LocalPlayer = UserWidget->GetOwningLocalPlayer() )
		{
			LocalPlayerId = LocalPlayer->GetControllerId();
			GameInstance  = LocalPlayer->GetGameInstance();
		}
	}
	else if ( APlayerController* PlayerController = Cast<APlayerController>( ContextObject ) )
	{
		if ( ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>( PlayerController->Player ) )
		{
			LocalPlayerId = LocalPlayer->GetControllerId();
			GameInstance  = LocalPlayer->GetGameInstance();
		}
	}
	else if ( ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>( ContextObject ) )
	{
		LocalPlayerId = LocalPlayer->GetControllerId();
		GameInstance  = LocalPlayer->GetGameInstance();
	}

	if ( LocalPlayerId == INDEX_NONE )
	{
		// if all else fails, we'll default to the first player
		if ( UWorld* World = ThisClass::GetWorldFrom( ContextObject ) )
		{
			GameInstance = World->GetGameInstance();

			if ( ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr )
			{
				LocalPlayerId = LocalPlayer->GetControllerId();
				GameInstance  = LocalPlayer->GetGameInstance();
			}
		}
	}

	if ( LocalPlayerId != INDEX_NONE )
	{
		return FBlueprintContextManager::Get( GameInstance ).GetContextFor( LocalPlayerId, Class );
	}

	return nullptr;
}

/*static*/ UWorld* UBlueprintContextLibrary::GetWorldFrom( UObject* ContextObject )
{
	return GEngine->GetWorldFromContextObject( ContextObject );
}