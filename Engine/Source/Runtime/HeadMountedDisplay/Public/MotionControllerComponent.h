// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IMotionController.h"
#include "SceneViewExtension.h"
#include "MotionControllerComponent.generated.h"

UCLASS(MinimalAPI, Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = MotionController)
class UMotionControllerComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	~UMotionControllerComponent();

	/** Which player index this motion controller should automatically follow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionController")
	int32 PlayerIndex;

	/** Which hand this component should automatically follow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionController")
	TEnumAsByte<EControllerHand> Hand;

	/** If false, render transforms within the motion controller hierarchy will be updated a second time immediately before rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionController")
	uint32 bDisableLowLatencyUpdate:1;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Whether or not this component had a valid tracked device this frame */
	UFUNCTION(BlueprintPure, Category = "MotionController")
	bool IsTracked() const
	{
		return bTracked;
	}

private:
	/** Whether or not this component had a valid tracked controller associated with it this frame*/
	bool bTracked;

	/** Whether or not this component has authority within the frame*/
	bool bHasAuthority;

	/** If true, the Position and Orientation args will contain the most recent controller state */
	bool PollControllerState(FVector& Position, FRotator& Orientation);

	/** View extension object that can persist on the render thread without the motion controller component */
	class FViewExtension : public ISceneViewExtension, public TSharedFromThis<FViewExtension, ESPMode::ThreadSafe>
	{
	public:
		FViewExtension(UMotionControllerComponent* InMotionControllerComponent) { MotionControllerComponent = InMotionControllerComponent; }
		virtual ~FViewExtension() {}

		/** ISceneViewExtension interface */
		virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
		virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
		virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
		virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	private:
		friend class UMotionControllerComponent;

		/** Walks the component hierarchy gathering scene proxies */
		void GatherSceneProxies(USceneComponent* Component);

		/** Motion controller component associated with this view extension */
		UMotionControllerComponent* MotionControllerComponent;

		/** Scene proxies that need late updates */
		FPrimitiveSceneProxy*	LateUpdateSceneProxies[16];
		int32					LateUpdateSceneProxyCount;
	};
	TSharedPtr< FViewExtension, ESPMode::ThreadSafe > ViewExtension;
};
