// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Perception/AISenseEvent.h"
#include "Perception/AISenseEvent_Hearing.h"
#include "Perception/AISenseEvent_Damage.h"
#include "Perception/AISense_Damage.h"

//----------------------------------------------------------------------//
// UAISenseEvent_Hearing
//----------------------------------------------------------------------//
UAISenseEvent_Hearing::UAISenseEvent_Hearing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

FAISenseID UAISenseEvent_Hearing::GetSenseID() const
{
	return UAISense::GetSenseID<UAISense_Hearing>();
}

//----------------------------------------------------------------------//
// UAISenseEvent_Damage
//----------------------------------------------------------------------//
FAISenseID UAISenseEvent_Damage::GetSenseID() const
{
	return UAISense::GetSenseID<UAISense_Damage>();
}