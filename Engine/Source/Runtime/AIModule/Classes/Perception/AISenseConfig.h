// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIPerceptionTypes.h"
#include "AISenseConfig.generated.h"

class UAISenseImplementation;
class UCanvas;
class UAIPerceptionComponent;

UCLASS(ClassGroup = AI, abstract, EditInlineNew, config=Game)
class AIMODULE_API UAISenseConfig : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense")
	uint32 bStartsEnabled : 1;

	UAISenseConfig(const FObjectInitializer&) : bStartsEnabled(true) {}

	virtual TSubclassOf<UAISense> GetSenseImplementation() const { return UAISense::StaticClass(); }
	FAISenseID GetSenseID() const;
	
#if !UE_BUILD_SHIPPING
	//----------------------------------------------------------------------//
	// DEBUG	
	//----------------------------------------------------------------------//
	virtual void DrawDebugInfo(UCanvas& Canvas, UAIPerceptionComponent& PerceptionComponent) const;
#endif // !UE_BUILD_SHIPPING
};