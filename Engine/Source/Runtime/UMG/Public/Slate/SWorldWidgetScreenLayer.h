// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class SWorldWidgetScreenLayer : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SWorldWidgetScreenLayer)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InPlayerContext);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void SetWidgetDrawSize(FVector2D DrawSize);

	void SetWidgetPivot(FVector2D Pivot);

	void AddComponent(USceneComponent* Component, TSharedPtr<SWidget> Widget);

	void RemoveComponent(USceneComponent* Component);

private:
	FLocalPlayerContext PlayerContext;

	FVector2D DrawSize;
	FVector2D Pivot;

	class FComponentEntry
	{
	public:
		FComponentEntry()
			: Slot(nullptr)
		{
		}

	public:

		TWeakObjectPtr<USceneComponent> Component;
		class UWidgetComponent* WidgetComponent;

		TSharedPtr<SWidget> ContainerWidget;
		TSharedPtr<SWidget> Widget;
		SConstraintCanvas::FSlot* Slot;
	};

	TMap<USceneComponent*, FComponentEntry> ComponentMap;
	TSharedPtr<SConstraintCanvas> Canvas;
};