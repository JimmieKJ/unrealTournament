// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNode_BlendListBase.h"
#include "AnimNode_BlendListByEnum.generated.h"

// Blend List by Enum, it changes based on enum input that enters
USTRUCT()
struct ENGINE_API FAnimNode_BlendListByEnum : public FAnimNode_BlendListBase
{
	GENERATED_USTRUCT_BODY()
public:
	// Mapping from enum value to BlendPose index; there will be one entry per entry in the enum; entries out of range always map to pose index 0
	UPROPERTY()
	TArray<int32> EnumToPoseIndex;

	// The currently selected pose (as an enum value)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Runtime, meta=(AlwaysAsPin))
	mutable uint8 ActiveEnumValue;

public:	
	FAnimNode_BlendListByEnum()
		: FAnimNode_BlendListBase()
	{
	}

protected:
	virtual int32 GetActiveChildIndex();
	virtual FString GetNodeName(FNodeDebugData& DebugData) { return DebugData.GetNodeName(this); }
};
