// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIPerceptionTypes.h"
#include "AISenseEvent.generated.h"

UCLASS(ClassGroup = AI, abstract, EditInlineNew, config = Game)
class AIMODULE_API UAISenseEvent : public UObject
{
	GENERATED_BODY()

public:
	virtual FAISenseID GetSenseID() const PURE_VIRTUAL(UAISenseEvent::GetSenseID, return FAISenseID::InvalidID(););

#if ENABLE_VISUAL_LOG
	virtual void DrawToVLog(UObject& LogOwner) const {}
#else
	void DrawToVLog(UObject& LogOwner) const {}
#endif // ENABLE_VISUAL_LOG
};
