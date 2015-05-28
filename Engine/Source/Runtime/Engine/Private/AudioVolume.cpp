// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioVolume.cpp: Used to affect audio settings in the game and editor.
=============================================================================*/

#include "EnginePrivate.h"
#include "AudioDevice.h"
#include "Components/BrushComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sound/ReverbEffect.h"

void FReverbSettings::PostSerialize(const FArchive& Ar)
{
	if( Ar.UE4Ver() < VER_UE4_REVERB_EFFECT_ASSET_TYPE )
	{
		FString ReverbAssetName;
		switch(ReverbType_DEPRECATED)
		{
			case REVERB_Default:
				// No replacement asset for default reverb type
				return;

			case REVERB_Bathroom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Bathroom.Bathroom");
				break;

			case REVERB_StoneRoom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/StoneRoom.StoneRoom");
				break;

			case REVERB_Auditorium:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Auditorium.Auditorium");
				break;

			case REVERB_ConcertHall:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/ConcertHall.ConcertHall");
				break;

			case REVERB_Cave:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Cave.Cave");
				break;

			case REVERB_Hallway:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Hallway.Hallway");
				break;

			case REVERB_StoneCorridor:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/StoneCorridor.StoneCorridor");
				break;

			case REVERB_Alley:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Alley.Alley");
				break;

			case REVERB_Forest:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Forest.Forest");
				break;

			case REVERB_City:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/City.City");
				break;

			case REVERB_Mountains:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Mountains.Mountains");
				break;

			case REVERB_Quarry:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Quarry.Quarry");
				break;

			case REVERB_Plain:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Plain.Plain");
				break;

			case REVERB_ParkingLot:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/ParkingLot.ParkingLot");
				break;

			case REVERB_SewerPipe:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/SewerPipe.SewerPipe");
				break;

			case REVERB_Underwater:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Underwater.Underwater");
				break;

			case REVERB_SmallRoom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/SmallRoom.SmallRoom");
				break;

			case REVERB_MediumRoom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/MediumRoom.MediumRoom");
				break;

			case REVERB_LargeRoom:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/LargeRoom.LargeRoom");
				break;

			case REVERB_MediumHall:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/MediumHall.MediumHall");
				break;

			case REVERB_LargeHall:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/LargeHall.LargeHall");
				break;

			case REVERB_Plate:
				ReverbAssetName = TEXT("/Engine/EngineSounds/ReverbSettings/Plate.Plate");
				break;

			default:
				// This should cover every type of reverb preset
				checkNoEntry();
				break;
		}

		ReverbEffect = LoadObject<UReverbEffect>(NULL, *ReverbAssetName);
		check( ReverbEffect );
	}
}

bool FInteriorSettings::operator==(const FInteriorSettings& Other) const
{
	return (Other.bIsWorldSettings == bIsWorldSettings)
		&& (Other.ExteriorVolume == ExteriorVolume) && (Other.ExteriorTime == ExteriorTime)
		&& (Other.ExteriorLPF == ExteriorLPF) && (Other.ExteriorLPFTime == ExteriorLPFTime)
		&& (Other.InteriorVolume == InteriorVolume) && (Other.InteriorTime == InteriorTime)
		&& (Other.InteriorLPF == InteriorLPF) && (Other.InteriorLPFTime == InteriorLPFTime);
}

bool FInteriorSettings::operator!=(const FInteriorSettings& Other) const
{
	return !(*this == Other);
}

AAudioVolume::AAudioVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GetBrushComponent()->bAlwaysCreatePhysicsState = true;

	bColored = true;
	BrushColor = FColor(255, 255, 0, 255);

	bEnabled = true;
}

void AAudioVolume::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAudioVolume, bEnabled);
}

void AAudioVolume::PostUnregisterAllComponents( void )
{
	// Route clear to super first.
	Super::PostUnregisterAllComponents();

	// World will be NULL during exit purge.
	UWorld* World = GetWorld();
	if (World)
	{
		AAudioVolume* CurrentVolume = World->HighestPriorityAudioVolume;
		AAudioVolume* PreviousVolume = NULL;

		// Iterate over linked list, removing this volume if found.
		while (CurrentVolume)
		{
			// Found.
			if (CurrentVolume == this)
			{
				// Remove from linked list.
				if (PreviousVolume)
				{
					PreviousVolume->NextLowerPriorityVolume = NextLowerPriorityVolume;
				}
				else
				{
					// Special case removal from first entry.
					World->HighestPriorityAudioVolume = NextLowerPriorityVolume;
				}

				break;
			}

			// List traversal.
			PreviousVolume = CurrentVolume;
			CurrentVolume = CurrentVolume->NextLowerPriorityVolume;
		}

		// Reset next pointer to avoid dangling end bits and also for GC.
		NextLowerPriorityVolume = nullptr;

		if (FAudioDevice* AudioDevice = World->GetAudioDevice())
		{
			AudioDevice->InvalidateCachedInteriorVolumes();
		}
	}
}

void AAudioVolume::PostRegisterAllComponents()
{
	// Route update to super first.
	Super::PostRegisterAllComponents();

	UWorld* World = GetWorld();
	AAudioVolume* CurrentVolume = World->HighestPriorityAudioVolume;
	AAudioVolume* PreviousVolume = nullptr;

	// Find where to insert in sorted linked list.
	if (CurrentVolume)
	{
		// Avoid double insertion!
		while (CurrentVolume && CurrentVolume != this)
		{
			// We use > instead of >= to be sure that we are not inserting twice in the case of multiple volumes having
			// the same priority and the current one already having being inserted after one with the same priority.
			if (Priority > CurrentVolume->Priority)
			{
				if (PreviousVolume)
				{
					// Insert before current node by fixing up previous to point to current.
					PreviousVolume->NextLowerPriorityVolume = this;
				}
				else
				{
					// Special case for insertion at the beginning.
					World->HighestPriorityAudioVolume = this;
				}

				// Point to current volume, finalizing insertion.
				NextLowerPriorityVolume = CurrentVolume;
				return;
			}

			// List traversal.
			PreviousVolume = CurrentVolume;
			CurrentVolume = CurrentVolume->NextLowerPriorityVolume;
		}

		// We're the lowest priority volume, insert at the end.
		if (!CurrentVolume)
		{
			checkSlow(PreviousVolume);
			PreviousVolume->NextLowerPriorityVolume = this;
			NextLowerPriorityVolume = nullptr;
		}
	}
	else
	{
		// First volume in the world info.
		World->HighestPriorityAudioVolume = this;
		NextLowerPriorityVolume	= nullptr;
	}

	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		AudioDevice->InvalidateCachedInteriorVolumes();
	}
}

#if WITH_EDITOR
void AAudioVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Settings.Volume = FMath::Clamp<float>( Settings.Volume, 0.0f, 1.0f );
	AmbientZoneSettings.InteriorTime = FMath::Max<float>( 0.01f, AmbientZoneSettings.InteriorTime );
	AmbientZoneSettings.InteriorLPFTime = FMath::Max<float>( 0.01f, AmbientZoneSettings.InteriorLPFTime );
	AmbientZoneSettings.ExteriorTime = FMath::Max<float>( 0.01f, AmbientZoneSettings.ExteriorTime );
	AmbientZoneSettings.ExteriorLPFTime = FMath::Max<float>( 0.01f, AmbientZoneSettings.ExteriorLPFTime );
}
#endif // WITH_EDITOR
