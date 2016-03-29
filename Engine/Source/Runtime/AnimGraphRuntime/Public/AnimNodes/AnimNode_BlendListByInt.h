// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "AnimNodes/AnimNode_BlendListBase.h"
#include "AnimNode_BlendListByInt.generated.h"

// Blend list node; has many children
USTRUCT()
struct ANIMGRAPHRUNTIME_API FAnimNode_BlendListByInt : public FAnimNode_BlendListBase
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Runtime, meta=(AlwaysAsPin))
	mutable int32 ActiveChildIndex;


public:	
	FAnimNode_BlendListByInt()
		:	FAnimNode_BlendListBase()
		,	ActiveChildIndex(0)
	{
	}

protected:
	virtual int32 GetActiveChildIndex();
	virtual FString GetNodeName(FNodeDebugData& DebugData) { return DebugData.GetNodeName(this); }
};
