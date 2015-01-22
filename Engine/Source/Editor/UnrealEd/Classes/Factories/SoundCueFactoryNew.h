// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// SoundCueFactoryNew
//=============================================================================

#pragma once
#include "SoundCueFactoryNew.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class USoundCueFactoryNew : public UFactory
{
	GENERATED_UCLASS_BODY()


	// Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	// Begin UFactory Interface	

	/** An initial sound wave to place in the newly created cue */
	UPROPERTY()
	class USoundWave* InitialSoundWave;
};



