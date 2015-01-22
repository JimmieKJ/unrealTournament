// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaSoundWaveFactoryNew.generated.h"


/**
 * Implements a factory for UMediaSoundWave objects.
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UMediaSoundWaveFactoryNew
	: public UFactory
{
	GENERATED_UCLASS_BODY()

	/** An initial UMediaPlayer asset to place in the newly created sound wave. */
	UPROPERTY()
	class UMediaPlayer* InitialMediaPlayer;

public:

	// UFactory Interface

	virtual UObject* FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn ) override;
	virtual bool ShouldShowInNewMenu() const override;
};
