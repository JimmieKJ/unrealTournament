// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Classes/Matinee/MatineeActor.h"

#include "UTMatineeActor.generated.h"

UCLASS()
class AUTMatineeActor : public AMatineeActor
{
	GENERATED_BODY()
public:
	/** set the matinee to this position during path building
	 * NOTE: this currently only affects the UT pathnode and link generation; it doesn't affect the raw mesh building
	 */
	UPROPERTY(EditInstanceOnly)
	float PathBuildingPosition;
};