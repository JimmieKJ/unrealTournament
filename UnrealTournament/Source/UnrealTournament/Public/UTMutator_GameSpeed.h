// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DefaultValueHelper.h"

#include "UTMutator_GameSpeed.generated.h"

UCLASS()
class AUTMutator_GameSpeed : public AUTMutator
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Config)
	float GameSpeed;

	AUTMutator_GameSpeed(const FObjectInitializer& OI)
		: Super(OI)
	{
		DisplayName = NSLOCTEXT("Mutator_GameSpeed", "Display Name", "Game Speed");
		GameSpeed = 1.2f;
#if !UE_SERVER
		if (!IsRunningDedicatedServer())
		{
			static ConstructorHelpers::FObjectFinder<UClass> ConfigMenuClass(TEXT("/Game/RestrictedAssets/Blueprints/GameSpeedMutatorMenu.GameSpeedMutatorMenu_C"));
			ConfigMenu = ConfigMenuClass.Object;
		}
#endif
	}

	virtual void GetGameURLOptions_Implementation(TArray<FString>& OptionsList) override
	{
		OptionsList.Add(FString::Printf(TEXT("GameSpeed=%f"), GameSpeed));
	}
	virtual void Init_Implementation(const FString& Options) override
	{
		Super::Init_Implementation(Options);

		FString NewSpeedStr = ParseOption(Options, TEXT("GameSpeed"));
		float NewSpeed = GameSpeed;
		if (!NewSpeedStr.IsEmpty() && FDefaultValueHelper::ParseFloat(NewSpeedStr, NewSpeed))
		{
			GameSpeed = NewSpeed;
		}
	}

	virtual void BeginPlay() override
	{
		Super::BeginPlay();

		GetWorldSettings()->TimeDilation *= FMath::Clamp<float>(GameSpeed, 0.5f, 2.0f);
	}
};