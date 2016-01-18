// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
#include "HeadMountedDisplayPrivate.h"
#include "MotionControllerComponent.h"

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
		const APlayerController* Actor = Cast<APlayerController>(GetOwner());
		bHasAuthority = !Actor || Actor->IsLocalPlayerController();
	}

	if ((PlayerIndex != INDEX_NONE) && bHasAuthority)
	{
		for (auto MotionController : GEngine->MotionControllerDevices)
		{
			if ((MotionController != nullptr) && MotionController->GetControllerOrientationAndPosition(PlayerIndex, Hand, Orientation, Position))
			{
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

	LateUpdateSceneProxyCount = 0;
	GatherSceneProxies(MotionControllerComponent);
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
	
	// Calculate the late update transform that will rebase all children proxies within the frame of reference
	const FTransform OldLocalToWorldTransform = MotionControllerComponent->CalcNewComponentToWorld(MotionControllerComponent->GetRelativeTransform());
	const FTransform NewLocalToWorldTransform = MotionControllerComponent->CalcNewComponentToWorld(FTransform(Orientation, Position));
	const FMatrix LateUpdateTransform = (OldLocalToWorldTransform.Inverse() * NewLocalToWorldTransform).ToMatrixWithScale();

	// Apply adjustment to the affected scene proxies
	for (int32 ProxyIndex = 0; ProxyIndex < LateUpdateSceneProxyCount; ++ProxyIndex)
	{
		LateUpdateSceneProxies[ProxyIndex]->ApplyLateUpdateTransform(LateUpdateTransform);
	}
}

//=============================================================================
void UMotionControllerComponent::FViewExtension::GatherSceneProxies(USceneComponent* Component)
{
	// If a scene proxy is present, cache it
	UPrimitiveComponent* PrimitiveComponent = dynamic_cast<UPrimitiveComponent*>(Component);
	if (PrimitiveComponent && PrimitiveComponent->SceneProxy)
	{
		check(LateUpdateSceneProxyCount < ARRAY_COUNT(LateUpdateSceneProxies));
		LateUpdateSceneProxies[LateUpdateSceneProxyCount++] = PrimitiveComponent->SceneProxy;
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

		GatherSceneProxies(ChildComponent);
	}
}