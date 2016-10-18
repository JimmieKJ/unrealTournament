// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VREditorBaseActor.h"
#include "VREditorFloatingUI.generated.h"


/**
 * Represents an interactive floating UI panel in the VR Editor
 */
UCLASS()
class AVREditorFloatingUI : public AVREditorBaseActor
{
	GENERATED_BODY()

public:

	/** Default constructor which sets up safe defaults */
	AVREditorFloatingUI();

	/** Creates a FVREditorFloatingUI using a Slate widget, and sets up safe defaults */
	void SetSlateWidget( class UVREditorUISystem& InitOwner, const TSharedRef<SWidget>& InitSlateWidget, const FIntPoint InitResolution, const float InitScale, const EDockedTo InitDockedTo );

	/** Creates a FVREditorFloatingUI using a UMG user widget, and sets up safe defaults */
	void SetUMGWidget( class UVREditorUISystem& InitOwner, class TSubclassOf<class UVREditorBaseUserWidget> InitUserWidgetClass, const FIntPoint InitResolution, const float InitScale, const EDockedTo InitDockedTo );

	/** @return Returns true if the UI is visible (or wants to be visible -- it might be transitioning */
	bool IsUIVisible() const
	{
		return bShouldBeVisible.GetValue();
	}

	/** Shows or hides the UI (also enables collision, and performs a transition effect) */
	void ShowUI( const bool bShow, const bool bAllowFading = true, const float InitFadeDelay = 0.0f );

	/** Returns the widget component for this UI, or nullptr if not spawned right now */
	class UVREditorWidgetComponent* GetWidgetComponent()
	{
		return WidgetComponent;
	}

	/** Returns the actual size of the UI in either axis, after scaling has been applied.  Does not take into animation or world scaling */
	FVector2D GetSize() const;

	/** Gets the scale */
	float GetScale() const;

	/** Sets a new size for the UI */
	void SetScale( const float NewSize );

	/** Sets the UI transform */
	virtual void SetTransform( const FTransform& Transform ) override;

	/** AActor overrides */
	virtual void Destroyed() override;

	/** Returns the owner of this object */
	UVREditorUISystem& GetOwner()
	{
		check( Owner != nullptr );
		return *Owner;
	}

	/** Returns the owner of this object (const) */
	const UVREditorUISystem& GetOwner() const
	{
		check( Owner != nullptr );
		return *Owner;
	}

	/** Returns casted userwidget */
	template<class T>
	T* GetUserWidget() const
	{
		return CastChecked<T>( UserWidget );
	}

	/** Sets if the collision is on when showing the UI*/
	void SetCollisionOnShow( const bool bInCollisionOnShow );

	/** Gets the initial size of this UI */
	float GetInitialScale() const;

protected:

	/** Returns a scale to use for this widget that takes into account animation */
	FVector CalculateAnimatedScale() const;

	/** Called to finish setting everything up, after a widget has been assigned */
	virtual void SetupWidgetComponent();

private:

	/** Called after spawning, and every Tick, to update opacity of the widget */
	virtual void UpdateFadingState( const float DeltaTime );

protected:
	/** Slate widget we're drawing, or null if we're drawing a UMG user widget */
	TSharedPtr<SWidget> SlateWidget;
	
	/** UMG user widget we're drawing, or nullptr if we're drawing a Slate widget */
	UPROPERTY()
	UVREditorBaseUserWidget* UserWidget;
	
	/** When in a spawned state, this is the widget component to represent the widget */
	UPROPERTY()
	class UVREditorWidgetComponent* WidgetComponent;

	/** Resolution we should draw this UI at, regardless of scale */
	FIntPoint Resolution;

private:

	/** Owning object */
	class UVREditorUISystem* Owner;

	/** Class of the UMG widget we're spawning */
	UPROPERTY()
	UClass* UserWidgetClass;

	/** True if the UI wants to be visible, or false if it wants to be hidden.  Remember, we might still be visually 
	    transitioning between states */
	TOptional<bool> bShouldBeVisible;

	/** Fade alpha, for visibility transitions */
	float FadeAlpha;

	/** Collision on when showing UI */
	bool bCollisionOnShowUI;

	/** Delay to fading in or out. Set on ShowUI and cleared on finish of fade in/out */
	float FadeDelay;

	/** The starting scale of this UI */
	float InitialScale;
};

