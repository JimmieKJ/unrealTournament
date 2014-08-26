// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "Editor/UnrealEd/Classes/Editor/UnrealEdEngine.h"
#include "UTWorldSettings.h"
#include "UTMenuGameMode.h"

#include "UTUnrealEdEngine.generated.h"

UCLASS(CustomConstructor)
class UUTUnrealEdEngine : public UUnrealEdEngine
{
	GENERATED_UCLASS_BODY()

	UUTUnrealEdEngine(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	// FIXME: workaround for engine bugs with default map prefixes
	virtual FString BuildPlayWorldURL(const TCHAR* MapName, bool bSpectatorMode = false, FString AdditionalURLOptions = FString())
	{
		FString URL = Super::BuildPlayWorldURL(MapName, bSpectatorMode, AdditionalURLOptions);
		FURL RealURL = FURL(NULL, *URL, TRAVEL_Absolute);
		if (!RealURL.HasOption(TEXT("game")))
		{
			FString MapNameNoPIE(FPaths::GetBaseFilename(MapName));
			MapNameNoPIE.RemoveFromStart(PLAYWORLD_PACKAGE_PREFIX);
			if (MapNameNoPIE.StartsWith(TEXT("_")))
			{
				MapNameNoPIE.RemoveFromStart(TEXT("_"));
				MapNameNoPIE = MapNameNoPIE.RightChop(MapNameNoPIE.Find(TEXT("_")) + 1);
			}
			
			// see if we have a per-prefix default specified
			bool bFound = false;
			AUTWorldSettings* Settings = AUTWorldSettings::StaticClass()->GetDefaultObject<AUTWorldSettings>();
			for (int32 Idx = 0; Idx < Settings->DefaultMapPrefixes.Num(); Idx++)
			{
				const FGameModePrefix& GTPrefix = Settings->DefaultMapPrefixes[Idx];
				if (GTPrefix.Prefix.Len() > 0 && MapNameNoPIE.StartsWith(GTPrefix.Prefix))
				{
					URL += TEXT("?game=") + GTPrefix.GameMode;
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				URL += TEXT("?game=") + AUTMenuGameMode::StaticClass()->GetDefaultObject<AUTMenuGameMode>()->SetGameMode(MapNameNoPIE, TEXT(""), TEXT(""))->GetPathName();
			}
		}

		return URL;
	}

	UT_LOADMAP_DEFINITION()
};
