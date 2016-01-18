// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "UTFlagInfo.generated.h"

/**
 * 
 */
UCLASS(CustomConstructor, perObjectConfig, Config = Flags)
class UNREALTOURNAMENT_API UUTFlagInfo : public UObject
{
	GENERATED_UCLASS_BODY()

	/**UV's into the CountryFlags texture*/
	UPROPERTY(Config)
	FTextureUVs UV;

	/**The roles that are allowed to use this flag*/
	UPROPERTY(Config)
	TArray<TEnumAsByte<EUnrealRoles::Type> > EntitledRoles;

	/**Flags are sorted by priority (lowest to highest), then alphabetically.*/
	UPROPERTY(Config)
	int32 Priority;

	/**Override the default filename for the slate image*/
	UPROPERTY(Config)
	FString FilenameOverride;

	/**The name of the atlas texture to use for non slate rendering*/
	UPROPERTY(Config)
	FString TextureName;

	UPROPERTY()
	UTexture2D* Texture;



	UUTFlagInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		UV = FTextureUVs(0.0f, 0.0f, 36.0f, 26.0f);
		Priority = 1;
		TextureName = "/Game/RestrictedAssets/UI/Textures/CountryFlags.CountryFlags";

		EntitledRoles.Add(EUnrealRoles::Gamer);
		EntitledRoles.Add(EUnrealRoles::Developer);
		EntitledRoles.Add(EUnrealRoles::Concepter);
		EntitledRoles.Add(EUnrealRoles::Contributor);
		EntitledRoles.Add(EUnrealRoles::Marketplace);
		EntitledRoles.Add(EUnrealRoles::Prototyper);
		EntitledRoles.Add(EUnrealRoles::Ambassador);
	}

	FString GetFriendlyName() const
	{
		//Config parser can't deal with an object with a space in the name so we use a . and convert here
		return GetFName().ToString().Replace(TEXT("."), TEXT(" "));
	}

	FName GetSlatePropertyName()
	{
		return FName(*FString::Printf(TEXT("UT.Flags.%s"), *GetFriendlyName()));
	}

	FString GetSlateFilePath()
	{
		FString Filename = FilenameOverride.IsEmpty() ? GetFriendlyName() : FilenameOverride;
		return TEXT("Flags/") + Filename;
	}

	bool IsEntitled(EUnrealRoles::Type Role)
	{
		return EntitledRoles.Contains(Role);
	}

	UTexture2D* GetTexture()
	{
		if (Texture == nullptr)
		{
			Texture = LoadObject<UTexture2D>(this, *TextureName);
		}
		return Texture;
	}
};
