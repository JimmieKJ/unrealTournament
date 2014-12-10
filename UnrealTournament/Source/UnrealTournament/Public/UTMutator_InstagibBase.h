// temp native class because blueprints don't support creating config properties
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTMutator_InstagibBase.generated.h"

UCLASS(Config = Game, Abstract)
class AUTMutator_InstagibBase : public AUTMutator
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, BlueprintReadWrite, EditDefaultsOnly)
	bool bAllowZoom;
};