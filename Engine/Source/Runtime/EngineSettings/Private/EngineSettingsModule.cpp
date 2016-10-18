// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EngineSettingsPrivatePCH.h"


/**
 * Implements the EngineSettings module.
 */
class FEngineSettingsModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return true;
	}
};


/* Class constructors
 *****************************************************************************/

UConsoleSettings::UConsoleSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGameMapsSettings::UGameMapsSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bUseSplitscreen(true)
	, TwoPlayerSplitscreenLayout(ETwoPlayerSplitScreenType::Horizontal)
	, ThreePlayerSplitscreenLayout(EThreePlayerSplitScreenType::FavorTop)
	, bOffsetPlayerGamepadIds(false)
{ }


UGameNetworkManagerSettings::UGameNetworkManagerSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGameSessionSettings::UGameSessionSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGeneralEngineSettings::UGeneralEngineSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGeneralProjectSettings::UGeneralProjectSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bShouldWindowPreserveAspectRatio(true)
	, bStartInVR(false)
{ }


UHudSettings::UHudSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


/* Static functions
 *****************************************************************************/

const FString UGameMapsSettings::GetGameDefaultMap( )
{
	return IsRunningDedicatedServer()
		? GetDefault<UGameMapsSettings>()->ServerDefaultMap.GetLongPackageName()
		: GetDefault<UGameMapsSettings>()->GameDefaultMap.GetLongPackageName();
}


const FString& UGameMapsSettings::GetGlobalDefaultGameMode( )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());

	return IsRunningDedicatedServer() && GameMapsSettings->GlobalDefaultServerGameMode.IsValid()
		? GameMapsSettings->GlobalDefaultServerGameMode.ToString()
		: GameMapsSettings->GlobalDefaultGameMode.ToString();
}


void UGameMapsSettings::SetGameDefaultMap( const FString& NewMap )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());

	if (IsRunningDedicatedServer())
	{
		GameMapsSettings->ServerDefaultMap = NewMap;
	}
	else
	{
		GameMapsSettings->GameDefaultMap = NewMap;
	}
}

void UGameMapsSettings::SetGlobalDefaultGameMode( const FString& NewGameMode )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());
	
	GameMapsSettings->GlobalDefaultGameMode = NewGameMode;
}

// Backwards compat for map strings
void FixMapAssetRef(FStringAssetReference& MapAssetReference)
{
	const FString AssetRefStr = MapAssetReference.ToString();
	int32 DummyIndex;
	if (!AssetRefStr.IsEmpty() && !AssetRefStr.FindLastChar(TEXT('.'), DummyIndex))
	{
		FString MapName, MapPath;
		AssetRefStr.Split(TEXT("/"), &MapPath, &MapName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		MapAssetReference.SetPath(FString::Printf(TEXT("%s/%s.%s"),*MapPath, *MapName, *MapName));
	}
};

void UGameMapsSettings::PostInitProperties()
{
	Super::PostInitProperties();

	FixMapAssetRef(EditorStartupMap);
	FixMapAssetRef(GameDefaultMap);
	FixMapAssetRef(ServerDefaultMap);
	FixMapAssetRef(TransitionMap);
}

void UGameMapsSettings::PostReloadConfig( UProperty* PropertyThatWasLoaded )
{
	if (PropertyThatWasLoaded)
	{
		if (PropertyThatWasLoaded->GetFName() == GET_MEMBER_NAME_CHECKED(UGameMapsSettings, EditorStartupMap))
		{
			FixMapAssetRef(EditorStartupMap);
		}
		else if (PropertyThatWasLoaded->GetFName() == GET_MEMBER_NAME_CHECKED(UGameMapsSettings, GameDefaultMap))
		{
			FixMapAssetRef(GameDefaultMap);
		}
		else if (PropertyThatWasLoaded->GetFName() == GET_MEMBER_NAME_CHECKED(UGameMapsSettings, ServerDefaultMap))
		{
			FixMapAssetRef(ServerDefaultMap);
		}
		else if (PropertyThatWasLoaded->GetFName() == GET_MEMBER_NAME_CHECKED(UGameMapsSettings, TransitionMap))
		{
			FixMapAssetRef(TransitionMap);
		}
	}
	else
	{
		FixMapAssetRef(EditorStartupMap);
		FixMapAssetRef(GameDefaultMap);
		FixMapAssetRef(ServerDefaultMap);
		FixMapAssetRef(TransitionMap);
	}
}


IMPLEMENT_MODULE(FEngineSettingsModule, EngineSettings);
