// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 * Used to affect audio settings in the game and editor.
 */

#pragma once
#include "GameFramework/Volume.h"
#include "AudioVolume.generated.h"

/**
 * DEPRECATED: Exists for backwards compatibility
 * Indicates a reverb preset to use.
 *
 */
UENUM()
enum ReverbPreset
{
	REVERB_Default,
	REVERB_Bathroom,
	REVERB_StoneRoom,
	REVERB_Auditorium,
	REVERB_ConcertHall,
	REVERB_Cave,
	REVERB_Hallway,
	REVERB_StoneCorridor,
	REVERB_Alley,
	REVERB_Forest,
	REVERB_City,
	REVERB_Mountains,
	REVERB_Quarry,
	REVERB_Plain,
	REVERB_ParkingLot,
	REVERB_SewerPipe,
	REVERB_Underwater,
	REVERB_SmallRoom,
	REVERB_MediumRoom,
	REVERB_LargeRoom,
	REVERB_MediumHall,
	REVERB_LargeHall,
	REVERB_Plate,
	REVERB_MAX,
};

/** Struct encapsulating settings for reverb effects. */
USTRUCT(BlueprintType)
struct FReverbSettings
{
	GENERATED_USTRUCT_BODY()

	/* Whether to apply the reverb settings below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReverbSettings )
	uint32 bApplyReverb:1;

	/** The reverb preset to employ. */
	UPROPERTY()
	TEnumAsByte<enum ReverbPreset> ReverbType_DEPRECATED;

	/** The reverb asset to employ. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReverbSettings)
	class UReverbEffect* ReverbEffect;

	/** Volume level of the reverb affect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReverbSettings)
	float Volume;

	/** Time to fade from the current reverb settings into this setting, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ReverbSettings)
	float FadeTime;



		FReverbSettings()
		: bApplyReverb(true)
		, ReverbType_DEPRECATED(REVERB_Default)
		, ReverbEffect(NULL)
		, Volume(0.5f)
		, FadeTime(2.0f)
		{
		}

		void PostSerialize(const FArchive& Ar);
	
};

template<>
struct TStructOpsTypeTraits<FReverbSettings> : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithPostSerialize = true,
	};
};

/** Struct encapsulating settings for interior areas. */
USTRUCT(BlueprintType)
struct FInteriorSettings
{
	GENERATED_USTRUCT_BODY()

	// Whether these interior settings are the default values for the world
	UPROPERTY()
	uint32 bIsWorldSettings:1;

	// The desired volume of sounds outside the volume when the player is inside the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float ExteriorVolume;

	// The time over which to interpolate from the current volume to the desired volume of sounds outside the volume when the player enters the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float ExteriorTime;

	// The desired LPF of sounds outside the volume when the player is inside the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float ExteriorLPF;

	// The time over which to interpolate from the current LPF to the desired LPF of sounds outside the volume when the player enters the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float ExteriorLPFTime;

	// The desired volume of sounds inside the volume when the player is outside the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float InteriorVolume;

	// The time over which to interpolate from the current volume to the desired volume of sounds inside the volume when the player enters the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float InteriorTime;

	// The desired LPF of sounds inside  the volume when the player is outside the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float InteriorLPF;

	// The time over which to interpolate from the current LPF to the desired LPF of sounds inside the volume when the player enters the volume
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InteriorSettings)
	float InteriorLPFTime;

	FInteriorSettings()
		: bIsWorldSettings(false)
		, ExteriorVolume(1.0f)
		, ExteriorTime(0.5f)
		, ExteriorLPF(1.0f)
		, ExteriorLPFTime(0.5f)
		, InteriorVolume(1.0f)
		, InteriorTime(0.5f)
		, InteriorLPF(1.0f)
		, InteriorLPFTime(0.5f)
		{
		}

	bool operator==(const FInteriorSettings& Other) const;
	bool operator!=(const FInteriorSettings& Other) const;
};

UCLASS(hidecategories=(Advanced, Attachment, Collision, Volume), MinimalAPI)
class AAudioVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

	/**
	 * Priority of this volume. In the case of overlapping volumes the one with the highest priority
	 * is chosen. The order is undefined if two or more overlapping volumes have the same priority.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AudioVolume)
	float Priority;

	/** whether this volume is currently enabled and able to affect sounds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, replicated, Category=AudioVolume)
	uint32 bEnabled:1;

	/** Reverb settings to use for this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Reverb)
	struct FReverbSettings Settings;

	/** Interior settings used for this volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=AmbientZone)
	struct FInteriorSettings AmbientZoneSettings;

	/** Next volume in linked listed, sorted by priority in descending order. */
	UPROPERTY(transient)
	class AAudioVolume* NextLowerPriorityVolume;



	// Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	// End UObject Interface

	// Begin AActor Interface
	virtual void PostUnregisterAllComponents() override;
protected:
	virtual void PostRegisterAllComponents() override;
public:
	// End AActor Interface
};



