// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNodeBase.h"
#include "Animation/AnimNode_BlendListBase.h"
#include "AnimNode_BlendListByBool.generated.h"

// This node is effectively a 'branch', picking one of two input poses based on an input Boolean value
USTRUCT()
struct ENGINE_API FAnimNode_BlendListByBool : public FAnimNode_BlendListBase
{
	GENERATED_USTRUCT_BODY()
public:
	// Which input should be connected to the output?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Runtime, meta=(AlwaysAsPin))
	bool bActiveValue;

public:	
	FAnimNode_BlendListByBool()
		: FAnimNode_BlendListBase()
	{
#if	WITH_EDITOR
		AddPose();
		AddPose();
#endif
	}

protected:
	virtual int32 GetActiveChildIndex();
	virtual FString GetNodeName(FNodeDebugData& DebugData) { return DebugData.GetNodeName(this); }
};
