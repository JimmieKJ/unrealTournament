// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "HeadMountedDisplayPrivate.h"
#include "MotionControllerComponent.h"
#include "PrimitiveSceneInfo.h"
#include "Features/IModularFeatures.h"

namespace {
	/** This is to prevent destruction of motion controller components while they are
		in the middle of being accessed by the render thread */
	FCriticalSection CritSect;

	/** Console variable for specifying whether motion controller late update is used */
	TAutoConsoleVariable<int32> CVarEnableMotionControllerLateUpdate(
		TEXT("vr.EnableMotionControllerLateUpdate"),
		1,
		TEXT("This command allows you to specify whether the motion controller late update is applied.\n")
		TEXT(" 0: don't use late update\n")
		TEXT(" 1: use late update (default)"),
		ECVF_Cheat);
} // anonymous namespace

//=============================================================================
UMotionControllerComponent::UMotionControllerComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	PlayerIndex = 0;
	Hand = EControllerHand::Left;
	bDisableLowLatencyUpdate = false;
	bHasAuthority = false;
}

//=============================================================================
UMotionControllerComponent::~UMotionControllerComponent()
{
	if (ViewExtension.IsValid())
	{
		{
			// This component could be getting accessed from the render thread so it needs to wait
			// before clearing MotionControllerComponent and allowing the destructor to continue
			FScopeLock ScopeLock(&CritSect);
			ViewExtension->MotionControllerComponent = NULL;
		}

		if (GEngine)
		{
			GEngine->ViewExtensions.Remove(ViewExtension);
		}
	}
	ViewExtension.Reset();
}

//=============================================================================
void UMotionControllerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FVector Position;
	FRotator Orientation;
	bTracked = PollControllerState(Position, Orientation);
	if (bTracked)
	{
		SetRelativeLocationAndRotation(Position, Orientation);
	}

	if (!ViewExtension.IsValid() && GEngine)
	{
		TSharedPtr< FViewExtension, ESPMode::ThreadSafe > NewViewExtension( new FViewExtension(this) );
		ViewExtension = NewViewExtension;
		GEngine->ViewExtensions.Add(ViewExtension);
	}
}

//=============================================================================
bool UMotionControllerComponent::PollControllerState(FVector& Position, FRotator& Orientation)
{
	if (IsInGameThread())
	{
		// Cache state from the game thread for use on the render thread
		const AActor* MyOwner = GetOwner();
		const APawn* MyPawn = Cast<APawn>(MyOwner);
		bHasAuthority = MyPawn ? MyPawn->IsLocallyControlled() : (MyOwner->Role == ENetRole::ROLE_Authority);
	}

	if ((PlayerIndex != INDEX_NONE) && bHasAuthority)
	{
		TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>( IMotionController::GetModularFeatureName() );
		for( auto MotionController : MotionControllers )
		{
			if ((MotionController != nullptr) && MotionController->GetControllerOrientationAndPosition(PlayerIndex, Hand, Orientation, Position))
			{
				CurrentTrackingStatus = MotionController->GetControllerTrackingStatus(PlayerIndex, Hand);
				return true;
			}
		}
	}
	return false;
}

//=============================================================================
void UMotionControllerComponent::FViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (!MotionControllerComponent)
	{
		return;
	}
	FScopeLock ScopeLock(&CritSect);
	if (!MotionControllerComponent || MotionControllerComponent->bDisableLowLatencyUpdate || !CVarEnableMotionControllerLateUpdate.GetValueOnGameThread())
	{
		return;
	}

	LateUpdatePrimitives.Reset();
	GatherLateUpdatePrimitives(MotionControllerComponent, LateUpdatePrimitives);
}

//=============================================================================
void UMotionControllerComponent::FViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	if (!MotionControllerComponent)
	{
		return;
	}
	FScopeLock ScopeLock(&CritSect);
	if (!MotionControllerComponent || MotionControllerComponent->bDisableLowLatencyUpdate || !CVarEnableMotionControllerLateUpdate.GetValueOnRenderThread())
	{
		return;
	}

	// Poll state for the most recent controller transform
	FVector Position;
	FRotator Orientation;
	if (!MotionControllerComponent->PollControllerState(Position, Orientation))
	{
		return;
	}
	
	if (LateUpdatePrimitives.Num())
	{
		// Calculate the late update transform that will rebase all children proxies within the frame of reference
		const FTransform OldLocalToWorldTransform = MotionControllerComponent->CalcNewComponentToWorld(MotionControllerComponent->GetRelativeTransform());
		const FTransform NewLocalToWorldTransform = MotionControllerComponent->CalcNewComponentToWorld(FTransform(Orientation, Position, MotionControllerComponent->GetComponentScale()));
		const FMatrix LateUpdateTransform = (OldLocalToWorldTransform.Inverse() * NewLocalToWorldTransform).ToMatrixWithScale();

		// Apply delta to the affected scene proxies
		for (auto PrimitiveInfo : LateUpdatePrimitives)
		{
			FPrimitiveSceneInfo* RetrievedSceneInfo = InViewFamily.Scene->GetPrimitiveSceneInfo(*PrimitiveInfo.IndexAddress);
			FPrimitiveSceneInfo* CachedSceneInfo = PrimitiveInfo.SceneInfo;
			// If the retrieved scene info is different than our cached scene info then the primitive was removed from the scene
			if (CachedSceneInfo == RetrievedSceneInfo && CachedSceneInfo->Proxy)
			{
				CachedSceneInfo->Proxy->ApplyLateUpdateTransform(LateUpdateTransform);
			}
		}
		LateUpdatePrimitives.Reset();
	}
}

void UMotionControllerComponent::FViewExtension::GatherLateUpdatePrimitives(USceneComponent* Component, TArray<LateUpdatePrimitiveInfo>& Primitives)
{
	// If a scene proxy is present, cache it
	UPrimitiveComponent* PrimitiveComponent = dynamic_cast<UPrimitiveComponent*>(Component);
	if (PrimitiveComponent && PrimitiveComponent->SceneProxy)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveComponent->SceneProxy->GetPrimitiveSceneInfo();
		if (PrimitiveSceneInfo)
		{
			LateUpdatePrimitiveInfo PrimitiveInfo;
			PrimitiveInfo.IndexAddress = PrimitiveSceneInfo->GetIndexAddress();
			PrimitiveInfo.SceneInfo = PrimitiveSceneInfo;
			Primitives.Add(PrimitiveInfo);
		}
	}

	// Gather children proxies
	const int32 ChildCount = Component->GetNumChildrenComponents();
	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		USceneComponent* ChildComponent = Component->GetChildComponent(ChildIndex);
		if (!ChildComponent)
		{
			continue;
		}

		GatherLateUpdatePrimitives(ChildComponent, Primitives);
	}
}