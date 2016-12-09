// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Matinee/InterpTrack.h"

#define MATINEE_TO_LEVEL_SEQUENCE_MODULE_NAME "MatineeToLevelSequence"

class UMovieScene;

//////////////////////////////////////////////////////////////////////////
// IMatineeToLevelSequenceModule

class IMatineeToLevelSequenceModule : public IModuleInterface
{
public:
	
 	DECLARE_DELEGATE_ThreeParams(FOnConvertMatineeTrack, UInterpTrack*, FGuid, UMovieScene*);
 
 	FDelegateHandle RegisterTrackConverterForMatineeClass(TSubclassOf<UInterpTrack> InterpTrackClass, FOnConvertMatineeTrack);
 	void UnregisterTrackConverterForMatineeClass(FDelegateHandle RemoveDelegate);
};

