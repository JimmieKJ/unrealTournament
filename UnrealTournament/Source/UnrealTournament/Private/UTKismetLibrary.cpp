// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTKismetLibrary.h"

UUTKismetLibrary::UUTKismetLibrary(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

class UAudioComponent* UUTKismetLibrary::PlaySoundTeamAdjusted(USoundCue* SoundToPlay, AActor* SoundInstigator, AActor* SoundTarget, bool Attached)
{
	if (!SoundToPlay || !SoundTarget)
	{
		return nullptr;
	}

	//Don't play sounds on dedicated servers
	ENetMode CurNetMode = (SoundTarget->GetWorld()) ? GEngine->GetNetMode(SoundTarget->GetWorld()) : NM_Standalone;
	if (SoundTarget->GetWorld() && (GEngine->GetNetMode(SoundTarget->GetWorld()) == NM_DedicatedServer))
	{
		return nullptr;
	}

	UAudioComponent* AudioComponent = FAudioDevice::CreateComponent(SoundToPlay, SoundTarget->GetWorld(), SoundTarget, false, false);
	if (AudioComponent)
	{
		const bool bIsInGameWorld = AudioComponent->GetWorld()->IsGameWorld();

		if (Attached)
		{
			AudioComponent->AttachTo(SoundTarget->GetRootComponent());
		}
		else
		{
			AudioComponent->SetWorldLocation(SoundTarget->GetActorLocation());
		}

		AudioComponent->SetVolumeMultiplier(1.0f);
		AudioComponent->SetPitchMultiplier(1.0f);
		AudioComponent->bAllowSpatialization = bIsInGameWorld;
		AudioComponent->bIsUISound = !bIsInGameWorld;
		AudioComponent->bAutoDestroy = true;
		AudioComponent->SubtitlePriority = 10000.f; // Fixme: pass in? Do we want that exposed to blueprints though?
		AudioComponent->AttenuationSettings = nullptr;

		AssignTeamAdjustmentValue(AudioComponent, SoundInstigator ? SoundInstigator : SoundTarget);
		AudioComponent->Play(0.0f);
	}

	return AudioComponent;
}


void UUTKismetLibrary::AssignTeamAdjustmentValue(UAudioComponent* AudioComponent, AActor* SoundInstigator)
{
	check(AudioComponent);
	check(SoundInstigator);

	AUTCharacter* UTCharInstigator = Cast<AUTCharacter>(SoundInstigator);
	if (UTCharInstigator && UTCharInstigator->IsLocallyControlled())
	{
		AudioComponent->SetIntParameter(FName("ListenerStyle"), 0);
	}
	else
	{
		bool bOnSameTeam = false;
		AUTPlayerController* const LocalPC = Cast<AUTPlayerController>(GEngine->GetFirstLocalPlayerController(SoundInstigator->GetWorld()));
		uint8 InstigatorTeam = 255;

		const IUTTeamInterface* TeamInterface = Cast<IUTTeamInterface>(SoundInstigator);
		if (TeamInterface != nullptr)
		{
			InstigatorTeam = TeamInterface->GetTeamNum();
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("AssignTeamAdjustmentValue called for invalid SoundInstigator"));
		}

		if (LocalPC && InstigatorTeam != 255 && InstigatorTeam == LocalPC->GetTeamNum())
		{
			AudioComponent->SetIntParameter(FName("ListenerStyle"), 1);
		}
		else
		{
			AudioComponent->SetIntParameter(FName("ListenerStyle"), 2);
		}
	}
}