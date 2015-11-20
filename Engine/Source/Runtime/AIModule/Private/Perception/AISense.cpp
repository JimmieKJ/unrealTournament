// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Prediction.h"
#include "Perception/AISense_Prediction.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif // !UE_BUILD_SHIPPING

const float UAISense::SuspendNextUpdate = FLT_MAX;

UAISense::UAISense(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TimeUntilNextUpdate(SuspendNextUpdate)
	, SenseID(FAISenseID::InvalidID())
{
	DebugName = GetName();
	DefaultExpirationAge = FAIStimulus::NeverHappenedAge;

	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		SenseID = ((const UAISense*)GetClass()->GetDefaultObject())->GetSenseID();
	}
}

UWorld* UAISense::GetWorld() const
{
	return PerceptionSystemInstance ? PerceptionSystemInstance->GetWorld() : nullptr;
}

void UAISense::HardcodeSenseID(TSubclassOf<UAISense> SenseClass, FAISenseID HardcodedID)
{
	UAISense* MutableCDO = GetMutableDefault<UAISense>(SenseClass);
	check(MutableCDO);
	MutableCDO->SenseID = HardcodedID;
}

void UAISense::PostInitProperties() 
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) == false) 
	{
		PerceptionSystemInstance = Cast<UAIPerceptionSystem>(GetOuter());
	}
}

AIPerception::FListenerMap* UAISense::GetListeners() 
{
	check(PerceptionSystemInstance);
	return &(PerceptionSystemInstance->GetListenersMap());
}

void UAISense::OnNewPawn(APawn& NewPawn)
{
	if (WantsNewPawnNotification())
	{
		UE_VLOG(GetPerceptionSystem(), LogAIPerception, Warning
			, TEXT("%s declars it needs New Pawn notification but does not override OnNewPawn"), *GetName());		
	}		
}

void UAISense::SetSenseID(FAISenseID Index)
{
	check(Index != FAISenseID::InvalidID());
	SenseID = Index;
}

void UAISense::ForceSenseID(FAISenseID InSenseID)
{
	check(GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint) == true);
	ensure(GetClass()->HasAnyClassFlags(CLASS_Abstract) == false);
	
	SenseID = InSenseID;
}

FAISenseID UAISense::UpdateSenseID()
{
	check(HasAnyFlags(RF_ClassDefaultObject) == true && GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_CompiledFromBlueprint) == false);

	if (SenseID.IsValid() == false)
	{
		SenseID = FAISenseID(GetFName());
	}

	return SenseID;
}

void UAISense::RegisterWrappedEvent(UAISenseEvent& PerceptionEvent)
{
	UE_VLOG(GetPerceptionSystem(), LogAIPerception, Error, TEXT("%s did not override UAISense::RegisterWrappedEvent!"), *GetName());
}

#if !UE_BUILD_SHIPPING
//----------------------------------------------------------------------//
// DEBUG
//----------------------------------------------------------------------//
FString UAISense::GetDebugLegend() const
{
	return FString::Printf(TEXT("{%s} %s,"), *DebugDrawColor.ToString(), *GetName());
}
#endif // !UE_BUILD_SHIPPING

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
UAISenseConfig::UAISenseConfig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), bStartsEnabled(true)
{
}

TSubclassOf<UAISense> UAISenseConfig::GetSenseImplementation() const 
{ 
	return UAISense::StaticClass(); 
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
TSubclassOf<UAISense> UAISenseConfig_Sight::GetSenseImplementation() const 
{ 
	return *Implementation; 
}

#if !UE_BUILD_SHIPPING
void UAISenseConfig_Sight::GetDebugData(TArray<FString>& OnScreenStrings, TArray<FDrawDebugShapeElement>& DebugShapes, const UAIPerceptionComponent& PerceptionComponent) const
{
	/*PeripheralVisionAngleDegrees*/
	const AActor* BodyActor = PerceptionComponent.GetBodyActor();
	if (BodyActor != nullptr)
	{
		UWorld* World = BodyActor->GetWorld();
		FVector BodyLocation, BodyFacing;
		PerceptionComponent.GetLocationAndDirection(BodyLocation, BodyFacing);

		DebugShapes.Add(FDrawDebugShapeElement::MakeCylinder(BodyLocation, BodyLocation + FVector(0, 0, -50), LoseSightRadius, 32, UAISense_Sight::GetDebugLoseSightColor()));
		DebugShapes.Add(FDrawDebugShapeElement::MakeCylinder(BodyLocation, BodyLocation + FVector(0, 0, -50), SightRadius, 32, UAISense_Sight::GetDebugSightRangeColor()));

		const float SightPieLength = FMath::Max(LoseSightRadius, SightRadius);
		DebugShapes.Add(FDrawDebugShapeElement::MakeLine(BodyLocation, BodyLocation + (BodyFacing * SightPieLength), UAISense_Sight::GetDebugSightRangeColor()));
		DebugShapes.Add(FDrawDebugShapeElement::MakeLine(BodyLocation, BodyLocation + (BodyFacing.RotateAngleAxis(PeripheralVisionAngleDegrees, FVector::UpVector) * SightPieLength), UAISense_Sight::GetDebugSightRangeColor()));
		DebugShapes.Add(FDrawDebugShapeElement::MakeLine(BodyLocation, BodyLocation + (BodyFacing.RotateAngleAxis(-PeripheralVisionAngleDegrees, FVector::UpVector) * SightPieLength), UAISense_Sight::GetDebugSightRangeColor()));
	}
}
#endif // !UE_BUILD_SHIPPING

//----------------------------------------------------------------------//
// UAISenseConfig_Hearing
//----------------------------------------------------------------------//

UAISenseConfig_Hearing::UAISenseConfig_Hearing(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer), HearingRange(3000.f)
{
}

TSubclassOf<UAISense> UAISenseConfig_Hearing::GetSenseImplementation() const 
{ 
	return *Implementation; 
}
#if !UE_BUILD_SHIPPING
void UAISenseConfig_Hearing::GetDebugData(TArray<FString>& OnScreenStrings, TArray<FDrawDebugShapeElement>& DebugShapes, const UAIPerceptionComponent& PerceptionComponent) const
{
	const AActor* BodyActor = PerceptionComponent.GetBodyActor();
	if (BodyActor != nullptr)
	{
		UWorld* World = BodyActor->GetWorld();
		FVector OwnerLocation = BodyActor->GetActorLocation();
		DebugShapes.Add(FDrawDebugShapeElement::MakeCylinder(OwnerLocation, OwnerLocation + FVector(0, 0, -50), HearingRange, 32, UAISense_Hearing::GetDebugHearingRangeColor()));
		if (bUseLoSHearing)
		{
			DebugShapes.Add(FDrawDebugShapeElement::MakeCylinder(OwnerLocation, OwnerLocation + FVector(0, 0, -50), LoSHearingRange, 32, UAISense_Hearing::GetDebugLoSHearingRangeeColor()));
		}
	}
}


#endif // !UE_BUILD_SHIPPING

//----------------------------------------------------------------------//
// UAISenseConfig_Prediction
//----------------------------------------------------------------------//
TSubclassOf<UAISense> UAISenseConfig_Prediction::GetSenseImplementation() const 
{ 
	return UAISense_Prediction::StaticClass(); 
}