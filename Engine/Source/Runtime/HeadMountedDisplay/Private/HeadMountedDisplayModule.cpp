// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayPrivate.h"
#include "PrimitiveSceneInfo.h"

void IHeadMountedDisplay::GatherLateUpdatePrimitives(USceneComponent* Component, TArray<LateUpdatePrimitiveInfo>& Primitives)
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

/**
* HMD device console vars
*/
static TAutoConsoleVariable<int32> CVarHiddenAreaMask(
	TEXT("vr.HiddenAreaMask"),
	1,
	TEXT("0 to disable hidden area mask, 1 to enable."),
	ECVF_Scalability | ECVF_RenderThreadSafe);


static void SetTrackingOrigin(const TArray<FString>& Args)
{
	int Origin = 0;
	if (Args.Num())
	{
		Origin = FCString::Atoi(*Args[0]);
		if (GEngine && GEngine->HMDDevice.IsValid())
		{
			GEngine->HMDDevice->SetTrackingOrigin(EHMDTrackingOrigin::Type(Origin));
		}
	}
	else
	{
		if (GEngine && GEngine->HMDDevice.IsValid())
		{
			Origin = GEngine->HMDDevice->GetTrackingOrigin();
		}
		if (GLog)
		{
			GLog->Logf(ELogVerbosity::Display, TEXT("Tracking orgin is set to %d"), Origin);
		}
	}
}

static FAutoConsoleCommand CTrackingOriginCmd(
	TEXT("vr.SetTrackingOrigin"),
	TEXT("0 - tracking origin is at the floor, 1 - tracking origin is at the eyes."),
	FConsoleCommandWithArgsDelegate::CreateStatic(SetTrackingOrigin));

class FHeadMountedDisplayModule : public IHeadMountedDisplayModule
{
	virtual TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > CreateHeadMountedDisplay()
	{
		TSharedPtr<IHeadMountedDisplay, ESPMode::ThreadSafe> DummyVal = NULL;
		return DummyVal;
	}

	FString GetModulePriorityKeyName() const
	{
		return FString(TEXT("Default"));
	}
};

IMPLEMENT_MODULE( FHeadMountedDisplayModule, HeadMountedDisplay );

IHeadMountedDisplay::IHeadMountedDisplay()
{
	LateUpdateGameWriteIndex = LateUpdateRenderReadIndex = 0;
}

void IHeadMountedDisplay::SetupLateUpdate(const FTransform& ParentToWorld, USceneComponent* Component)
{
	LateUpdateParentToWorld = ParentToWorld;
	LateUpdatePrimitives[LateUpdateGameWriteIndex].Reset();
	GatherLateUpdatePrimitives(Component, LateUpdatePrimitives[LateUpdateGameWriteIndex]);
	LateUpdateGameWriteIndex = (LateUpdateGameWriteIndex + 1) % 2;
}

void IHeadMountedDisplay::ApplyLateUpdate(FSceneInterface* Scene, const FTransform& OldRelativeTransform, const FTransform& NewRelativeTransform)
{
	if (!LateUpdatePrimitives[LateUpdateRenderReadIndex].Num())
	{
		return;
	}

	const FTransform OldCameraTransform = OldRelativeTransform * LateUpdateParentToWorld;
	const FTransform NewCameraTransform = NewRelativeTransform * LateUpdateParentToWorld;
	const FMatrix LateUpdateTransform = (OldCameraTransform.Inverse() * NewCameraTransform).ToMatrixWithScale();

	// Apply delta to the affected scene proxies
	for (auto PrimitiveInfo : LateUpdatePrimitives[LateUpdateRenderReadIndex])
	{
		FPrimitiveSceneInfo* RetrievedSceneInfo = Scene->GetPrimitiveSceneInfo(*PrimitiveInfo.IndexAddress);
		FPrimitiveSceneInfo* CachedSceneInfo = PrimitiveInfo.SceneInfo;
		// If the retrieved scene info is different than our cached scene info then the primitive was removed from the scene
		if (CachedSceneInfo == RetrievedSceneInfo && CachedSceneInfo->Proxy)
		{
			CachedSceneInfo->Proxy->ApplyLateUpdateTransform(LateUpdateTransform);
		}
	}
	LateUpdatePrimitives[LateUpdateRenderReadIndex].Reset();
	LateUpdateRenderReadIndex = (LateUpdateRenderReadIndex + 1) % 2;
}

bool IHeadMountedDisplay::DoesAppUseVRFocus() const
{
	return FApp::UseVRFocus();
}

bool IHeadMountedDisplay::DoesAppHaveVRFocus() const
{
	return FApp::HasVRFocus();
}


