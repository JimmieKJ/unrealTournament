// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Sound/SoundNodeAssetReferencer.h"
#include "SoundNodeWavePlayer.generated.h"

/** 
 * Sound node that contains a reference to the raw wave file to be played
 */
UCLASS(hidecategories=Object, editinlinenew, MinimalAPI, meta=( DisplayName="Wave Player" ))
class USoundNodeWavePlayer : public USoundNodeAssetReferencer
{
	GENERATED_BODY()

private:
	UPROPERTY(EditAnywhere, Category=WavePlayer, meta=(DisplayName="Sound Wave"))
	TAssetPtr<USoundWave> SoundWaveAssetPtr;

	UPROPERTY(transient)
	USoundWave* SoundWave;

	void OnSoundWaveLoaded(const FName& PackageName, UPackage * Package, EAsyncLoadingResult::Type Result);

public:	

	UPROPERTY(EditAnywhere, Category=WavePlayer)
	uint32 bLooping:1;

	ENGINE_API USoundWave* GetSoundWave() const { return SoundWave; }
	ENGINE_API void SetSoundWave(USoundWave* SoundWave);

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//~ Begin USoundNode Interface
	virtual int32 GetMaxChildNodes() const override;
	virtual float GetDuration() override;
	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
#if WITH_EDITOR
	virtual FText GetTitle() const override;
#endif
	//~ End USoundNode Interface

	//~ Begin USoundNodeAssetReferencer Interface
	virtual void LoadAsset() override;
	//~ End USoundNode Interface

};

