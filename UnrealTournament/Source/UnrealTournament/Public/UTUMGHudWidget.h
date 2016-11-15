// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UserWidget.h"
#include "AssetData.h"
#include "UTUMGHudWidget.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTUMGHudWidget : public UUserWidget
{
	GENERATED_UCLASS_BODY()

	/** Associated this UMG widget with a HUD. */
	virtual void AssociateHUD(class AUTHUD* NewHUDAnchor);

	/** Give this widget a set lifespan.  Once it expires it will automatically be removed from the hud */
	UFUNCTION(BlueprintCallable, Category = Hud)
	virtual void SetLifeTime(float Lifespan);

	/** Apply HUD Visibility to the UMG Widget */
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic, Category = Hud)
	ESlateVisibility ApplyHUDVisibility() const;

	/** Allow the UMG widget to signal this widget is done and should be removed */
	UFUNCTION(BlueprintCallable, Category = Hud)
	void NotifyHUDWidgetIsDone();

	/** Where in the viewport stack to sort this widget */
	UPROPERTY(EditDefaultsOnly, Category="Display")
	float DisplayZOrder;

protected:


	/** This is the UTHUD that anchors this UMG widget */
	TWeakObjectPtr<class AUTHUD> HUDAnchor;

	/** Allow these widgets to get their visibility from the hud */
	UFUNCTION()
	ESlateVisibility GetHUDVisibility() const;

	FTimerHandle LifeTimer;

};
