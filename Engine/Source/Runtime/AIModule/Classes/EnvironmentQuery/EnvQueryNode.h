// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvQueryNode.generated.h"

UCLASS(Abstract)
class AIMODULE_API UEnvQueryNode : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Versioning for updating deprecated properties */
	UPROPERTY()
	int32 VerNum;

	virtual void UpdateNodeVersion();

	virtual FText GetDescriptionTitle() const;
	virtual FText GetDescriptionDetails() const;

#if WITH_EDITOR && USE_EQS_DEBUGGER
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR && USE_EQS_DEBUGGER
};
