// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvQueryTypes.h"
#include "EnvQuery.generated.h"

class UEnvQueryOption;
#if WITH_EDITORONLY_DATA
class UEdGraph;
#endif // WITH_EDITORONLY_DATA

UCLASS()
class AIMODULE_API UEnvQuery : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Graph for query */
	UPROPERTY()
	UEdGraph*	EdGraph;
#endif

	UPROPERTY()
	TArray<UEnvQueryOption*> Options;

	/** Gather all required named params */
	void CollectQueryParams(TArray<FEnvNamedValue>& NamedValues) const;
};
