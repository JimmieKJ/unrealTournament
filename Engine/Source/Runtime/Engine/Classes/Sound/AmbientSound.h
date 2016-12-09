// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "AmbientSound.generated.h"

/** A sound actor that can be placed in a level */
UCLASS(AutoExpandCategories=Audio, ClassGroup=Sounds, hideCategories(Collision, Input, Game), showCategories=("Input|MouseInput", "Input|TouchInput", "Game|Damage"), ComponentWrapperClass)
class ENGINE_API AAmbientSound : public AActor
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** Audio component that handles sound playing */
	DEPRECATED_FORGAME(4.6, "AudioComponent should not be accessed directly, please use GetAudioComponent() function instead. AudioComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = Sound, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Sound,Audio,Audio|Components|Audio", AllowPrivateAccess = "true"))
	class UAudioComponent* AudioComponent;
public:
	
	FString GetInternalSoundCueName();

	//~ Begin AActor Interface.
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual bool GetReferencedContentObjects( TArray<UObject*>& Objects ) const override;
#endif
	virtual void PostRegisterAllComponents() override;
	//~ End AActor Interface.

	// BEGIN DEPRECATED (use component functions now in level script)
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(DeprecatedFunction))
	void FadeIn(float FadeInDuration, float FadeVolumeLevel = 1.f);
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(DeprecatedFunction))
	void FadeOut(float FadeOutDuration, float FadeVolumeLevel);
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(DeprecatedFunction))
	void AdjustVolume(float AdjustVolumeDuration, float AdjustVolumeLevel);
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(DeprecatedFunction))
	void Play(float StartTime = 0.f);
	UFUNCTION(BlueprintCallable, Category="Audio", meta=(DeprecatedFunction))
	void Stop();
	// END DEPRECATED

public:
	/** Returns AudioComponent subobject **/
	class UAudioComponent* GetAudioComponent() const;
};



