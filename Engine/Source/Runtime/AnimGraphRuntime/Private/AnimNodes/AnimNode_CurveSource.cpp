// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_CurveSource.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

FAnimNode_CurveSource::FAnimNode_CurveSource()
	: SourceBinding(ICurveSourceInterface::DefaultBinding)
	, Alpha(1.0f)
{
}

void FAnimNode_CurveSource::PreUpdate(const UAnimInstance* AnimInstance)
{
	// re-bind to our named curve source in pre-update
	// we do this here to allow re-binding of the source without reinitializing the whole
	// anim graph. If the source goes away (e.g. if an audio component is destroyed) or the
	// binding changes then we can re-bind to a new object
	if (CurveSource.GetObject() == nullptr || Cast<ICurveSourceInterface>(CurveSource.GetObject())->Execute_GetBindingName(CurveSource.GetObject()) != SourceBinding)
	{
		ICurveSourceInterface* PotentialCurveSource = nullptr;

		AActor* Actor = AnimInstance->GetOwningActor();
		if (Actor)
		{
			// check if our actor implements our interface
			PotentialCurveSource = Cast<ICurveSourceInterface>(Actor);
			if (PotentialCurveSource && PotentialCurveSource->Execute_GetBindingName(Actor) == SourceBinding)
			{
				CurveSource.SetObject(Actor);
				CurveSource.SetInterface(PotentialCurveSource);
			}
			else
			{
				// then check each compoennt on the actor
				const TSet<UActorComponent*>& ActorOwnedComponents = Actor->GetComponents();
				for (UActorComponent* OwnedComponent : ActorOwnedComponents)
				{
					PotentialCurveSource = Cast<ICurveSourceInterface>(OwnedComponent);
					if (PotentialCurveSource && PotentialCurveSource->Execute_GetBindingName(OwnedComponent) == SourceBinding)
					{
						CurveSource.SetObject(OwnedComponent);
						CurveSource.SetInterface(PotentialCurveSource);
					}
				}
			}
		}
	}
}

class FExternalCurveScratchArea : public TThreadSingleton<FExternalCurveScratchArea>
{
public:
	TArray<FNamedCurveValue> NamedCurveValues;
};

void FAnimNode_CurveSource::Evaluate(FPoseContext& Output)
{
	SourcePose.Evaluate(Output);

	if (CurveSource.GetInterface() != nullptr)
	{
		TArray<FNamedCurveValue>& NamedCurveValues = FExternalCurveScratchArea::Get().NamedCurveValues;
		NamedCurveValues.Reset();
		CurveSource->Execute_GetCurves(CurveSource.GetObject(), NamedCurveValues);

		USkeleton* Skeleton = Output.AnimInstanceProxy->GetSkeleton();

		for (FNamedCurveValue NamedValue : NamedCurveValues)
		{
			SmartName::UID_Type NameUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, NamedValue.Name);
			if (NameUID != SmartName::MaxUID)
			{
				const float CurrentValue = Output.Curve.Get(NameUID);
				const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
				Output.Curve.Set(NameUID, FMath::Lerp(CurrentValue, NamedValue.Value, ClampedAlpha));
			}
		}
	}
}

void FAnimNode_CurveSource::Update(const FAnimationUpdateContext& Context)
{
	// Evaluate any BP logic plugged into this node
	EvaluateGraphExposedInputs.Execute(Context);
}
