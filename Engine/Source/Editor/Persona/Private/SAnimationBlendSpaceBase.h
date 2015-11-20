// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "Sorting.h"
#include "Animation/BlendSpaceBase.h"

extern FText GetErrorMessageForSequence(UBlendSpaceBase* BlendSpace, UAnimSequence* AnimSequence, FVector SampleValue, int32 OriginalIndex);

//delegates for editors to respond to widget changes */
DECLARE_DELEGATE(FOnParametersUpdated);
DECLARE_DELEGATE(FOnSamplesUpdated);

// delegate to refresh sample list
DECLARE_DELEGATE( FOnRefreshSamples )

struct FNotificationInfo;
// delegate to show user notifications
DECLARE_DELEGATE_OneParam( FOnNotifyUser, FNotificationInfo& )

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * States for drawing samples in editor
 */
namespace EBlendSpaceSampleState
{
	enum Type
	{
		Normal,
		Highlighted,
		Invalid
	};
}

/**
 * Utility class for Drawing lines in the editor
 */
struct FLine
{
	FVector2D	LinePoints[2]; 
	FColor		LineColor;
	int32		LayerID;

	bool operator==( const FLine& Other ) const 
	{
		// if same position, it's same point
		return ((LinePoints[0] == Other.LinePoints[0] && LinePoints[1] == Other.LinePoints[1]) ||
			(LinePoints[0] == Other.LinePoints[1] && LinePoints[1] == Other.LinePoints[0]));
	}

	FLine(const FVector2D& A, const FVector2D& B, const FColor InColor, int32 InLayerID)
	{
		LinePoints[0] = A;
		LinePoints[1] = B;
		LineColor = InColor;
		LayerID = InLayerID;
	}
};

struct FDrawLines
{
	TArray<FLine> DrawLines;

	void AddLine(const FVector2D& A, const FVector2D& B, const FColor InColor, int32 InLayerID)
	{
		FLine NewLine(A, B, InColor, InLayerID);

		for (auto Iter=DrawLines.CreateIterator(); Iter; ++Iter)
		{
			FLine& ExistingLine = (*Iter);
			if (ExistingLine == NewLine)
			{
				if (ExistingLine.LayerID < InLayerID)
				{
					// overwrite new layerID and color
					ExistingLine.LayerID = InLayerID;
					ExistingLine.LineColor = InColor;
					return;
				}
			}
		}

		// otherwise, add to DrawLines
		DrawLines.Add(NewLine);
	}

	void AddTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FColor InColor, int32 InLayerID)
	{
		AddLine(A, B, InColor, InLayerID);
		AddLine(B, C, InColor, InLayerID);
		AddLine(C, A, InColor, InLayerID);
	}

	void Draw(const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements)
	{
		TArray<FVector2D> LinePoints;
		LinePoints.AddUninitialized(2);

		for (auto Iter=DrawLines.CreateConstIterator(); Iter; ++Iter)
		{
			const FLine& ExistingLine = (*Iter);

			LinePoints[0] = ExistingLine.LinePoints[0];
			LinePoints[1] = ExistingLine.LinePoints[1];

			FSlateDrawElement::MakeLines( 
				OutDrawElements,
				ExistingLine.LayerID,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				MyClippingRect,
				ESlateDrawEffect::None,
				ExistingLine.LineColor, 
				false
				);
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// SBlendSpaceParameterWidget

class SBlendSpaceParameterWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceParameterWidget)
		: _BlendSpace(NULL)			
		{}
		SLATE_ARGUMENT(UBlendSpaceBase*, BlendSpace)
		SLATE_EVENT(FOnParametersUpdated, OnParametersUpdated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void ConstructParameterPanel();

	void UpdateParametersFromBlendSpace();

	void SetBlendSpace(UBlendSpaceBase* InBlendSpace){BlendSpace = InBlendSpace; ConstructParameterPanel();}
private:

	/** Handler for the Apply Parameters button */
	FReply OnApplyParameterChanges();

	UBlendSpaceBase*				BlendSpace;

	/** Parameter UI parts */
	TSharedPtr<SVerticalBox>		ParameterPanel;
	TSharedPtr<SEditableTextBox>	Param_DisplayNames[3];
	TSharedPtr<SEditableTextBox>	Param_MinValues[3];
	TSharedPtr<SEditableTextBox>	Param_MaxValues[3];
	TSharedPtr<SEditableTextBox>	Param_GridNumbers[3];

	/** Callback to notify parent interface when the parameters have been updated */
	FOnParametersUpdated OnParametersUpdated;
};

typedef TAttribute<ECheckBoxState>::FGetter FReturnCheckboxValueDelegate;

struct FBlendSpaceDisplayCheckBoxEntry
{
	const FString Name;
	const FOnCheckStateChanged OnCheckStateChanged;
	const FReturnCheckboxValueDelegate IsChecked;

	FBlendSpaceDisplayCheckBoxEntry(const FString& InName, const FOnCheckStateChanged InOnCheckStateChanged, const FReturnCheckboxValueDelegate InIsChecked)
		: Name(InName), OnCheckStateChanged(InOnCheckStateChanged), IsChecked(InIsChecked)
	{}
};

//////////////////////////////////////////////////////////////////////////
// SBlendSpaceDisplayOptionsWidget

class SBlendSpaceDisplayOptionsWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceDisplayOptionsWidget)
		{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	SVerticalBox::FSlot& AddCheckBoxSlot();

private:
	TSharedPtr<SVerticalBox> CheckBoxArea;
};

//////////////////////////////////////////////////////////////////////////
// SBlendSpaceSamplesWidget

class SBlendSpaceSamplesWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceSamplesWidget)
		: _BlendSpace(NULL)
		{}
		SLATE_ARGUMENT(UBlendSpaceBase*, BlendSpace)
		SLATE_EVENT(FOnSamplesUpdated, OnSamplesUpdated)
		SLATE_EVENT(FOnNotifyUser, OnNotifyUser)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void ConstructSamplesPanel();

	void SetBlendSpace(UBlendSpaceBase* InBlendSpace){BlendSpace = InBlendSpace; ConstructSamplesPanel();}
private:
	/** Handler for "goto in asset browser" button */
	FReply GotoSample(int32 indexOfSample);

	/** Handler for the replace sample button */
	FReply ReplaceSample(int32 indexOfSample);

	/** Handler for the delete sample button */
	FReply DeleteSample(int32 indexOfSample);

	/** Handler for the remove all samples button */
	FReply OnClearSamples();

	UBlendSpaceBase*				BlendSpace;

	/** Samples UI parts */
	TSharedPtr<SVerticalBox>	SampleDataPanel;

	/** Callback to notify parent interface when the parameters have been updated */
	FOnSamplesUpdated OnSamplesUpdated;

	/** Callback to display user notification */
	FOnNotifyUser OnNotifyUser;
};

//////////////////////////////////////////////////////////////////////////
// SBlendSpaceWidget

class SBlendSpaceWidget : public SCompoundWidget
{
public:
		SLATE_BEGIN_ARGS(SBlendSpaceWidget)
			: _BlendSpace(NULL)
			{}
			SLATE_ARGUMENT(UBlendSpaceBase*, BlendSpace)
			SLATE_EVENT(FOnRefreshSamples, OnRefreshSamples)
			SLATE_EVENT(FOnNotifyUser, OnNotifyUser)
		SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FVector		GetLastValidMouseGridPoint() { return LastValidMouseEditorPoint; }

	// SWidget Interface
	virtual FReply	OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply	OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply	OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply	OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;	
	virtual FReply	OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply	OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void	OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	// End of SWidget

	/** Called by OnDrop to actually process sample drop */
	void HandleSampleDrop(const FGeometry& MyGeometry, const FVector2D& ScreenSpaceDropPosition, UAnimSequence* DroppedSequence, int32 OriginalIndex);

	/** Resample data **/
	virtual void ResampleData();

	void ValidateSamplePositions();

	/** 
	 * Mapping function between WidgetPos and GridPos
	 */
	virtual TOptional<FVector2D>	GetWidgetPosFromEditorPos(const FVector& EditorPos, const FSlateRect& WindowRect) const PURE_VIRTUAL(SBlendSpaceWidget::GetWidgetPosFromEditorPos, return TOptional<FVector2D>(););
	virtual TOptional<FVector>		GetEditorPosFromWidgetPos(const FVector2D& WidgetPos, const FSlateRect& WindowRect) const PURE_VIRTUAL(SBlendSpaceWidget::GetEditorPosFromWidgetPos, return TOptional<FVector>(););

	/**
	 * Snaps a position in editor space to the editor grid
	 */
	virtual FVector					SnapEditorPosToGrid(const FVector& InPos) const PURE_VIRTUAL(SBlendSpaceWidget::SnapEditorPosToGrid, return FVector(); );

protected:
	/** Utility functions for OnPaint */
	void DrawToolTip(const FVector2D& LeftTopPos, const FText& Text, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, const FColor& InColor, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const;
	void DrawSamplePoint( const FVector2D& Point, EBlendSpaceSampleState::Type SampleState, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const;
	void DrawSamplePoints( const FSlateRect& WindowRect, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, int32 HighlightedSampleIndex ) const;
	void DrawText( const FVector2D& Point, const FText& Text, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, const FColor& InColor, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const;
	
	/** Utility functions */
	FText GetToolTipText(const FVector& GridPos, const TArray<UAnimSequence*> AnimSeqs, const TArray<float> BlendWeights) const;
	virtual FText GetInputText(const FVector& GridPos) const PURE_VIRTUAL(SBlendSpaceWidget::GetInputText, return FText::GetEmpty(); );
	int32 GetHighlightedSample(const FSlateRect& WindowRect) const;

	/** Update internal data based on mouse position */
	virtual FReply UpdateLastMousePosition( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition, bool bClampToWindowRect = false, bool bSnapToGrid = false  ) PURE_VIRTUAL(SBlendSpaceWidget::UpdateLastMousePosition, return FReply::Handled(););

	/** Display sample editing popup */
	void ShowPopupEditWindow(int32 SampleIndex, const FPointerEvent& MouseEvent);

	/** Handler for sample editing popup to delete selected sample */
	FReply DeleteSample(int32 SampleIndex);

	/** Handler for sample editing popup to open selected sample */
	FReply OpenAsset(int32 SampleIndex);

	/**
	 * Calculate window rect for this widget from given Geometry 
	 */
	FSlateRect GetWindowRectFromGeometry(const FGeometry& MyGeometry) const
	{
		FSlateRect WindowRect = FSlateRect(0, 0, MyGeometry.Size.X, MyGeometry.Size.Y);
		return WindowRect.InsetBy(FMargin(5.f));
	}

	/** Cache of blend space samples to reduce blend space acceses **/
	TArray<FBlendSample> CachedSamples;

	/** Track whether the user is dragging something in the UI or not */
	bool		bBeingDragged;

	// Save delegate to refresh samples. 
	FOnRefreshSamples OnRefreshSamples;

	// Save delegate to notify user
	FOnNotifyUser OnNotifyUser;

	/** The blend space we are editing */
	UBlendSpaceBase* BlendSpace;

public:
	/** Controls whether the widgets mouse tooltip is rendered */
	bool		bTooltipOn;
	/** Controls whether the we drive the Persona preview window or not */
	bool		bPreviewOn;

	/** Caches the last mouse position in editor space */
	FVector		LastValidMouseEditorPoint;
};


//////////////////////////////////////////////////////////////////////////
// SBlendSpaceEditorBase

class SBlendSpaceEditorBase : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceEditorBase)
		: _BlendSpace(NULL)			
		, _Persona()
		{}

		SLATE_ARGUMENT(UBlendSpaceBase*, BlendSpace)
		SLATE_ARGUMENT(TSharedPtr<class FPersona>, Persona)
	SLATE_END_ARGS()

	virtual ~SBlendSpaceEditorBase();

	void Construct(const FArguments& InArgs);

	void PostUndo();

protected:
	/** Updates the preview */
	EActiveTimerReturnType UpdatePreview( double InCurrentTime, float InDeltaTime );

	/** Creates the editor heading UI */
	TSharedRef<SWidget> MakeEditorHeader() const;

	/** Creates the options panel for the editor */
	virtual TSharedRef<SWidget> MakeDisplayOptionsBox() const;

	/** Handle preview on blend checkbox */
	ECheckBoxState IsPreviewOn() const;
	void ShowPreview_OnIsCheckedChanged( ECheckBoxState NewValue );
	void ShowToolTip_OnIsCheckedChanged( ECheckBoxState NewValue );

	/** Updates Persona's preview window */
	void UpdatePreviewParameter() const;

	/** Refresh the samples UI panel*/
	void RefreshSampleDataPanel();

	/** Handler for when blend space samples have been changed */
	void OnBlendSpaceSamplesChanged();

	/** Get preview blend input from Persona */
	FVector GetPreviewBlendInput() const;

	/* Notifies the user of failures */
	void NotifyUser( FNotificationInfo& Info );

	FText GetBlendSpaceDisplayName() const
	{
		return FText::FromString(BlendSpace->GetName());
	}

	/** Pointer back to the Persona that owns us */
	TWeakPtr<class FPersona> PersonaPtr;

	/** The blend space being edited */
	UBlendSpaceBase* BlendSpace;

	/** Pointer to the editor widget */
	TSharedPtr<SBlendSpaceWidget> BlendSpaceWidget;

	/** Pointer to the parameters display widget */
	TSharedPtr<SBlendSpaceParameterWidget> ParameterWidget;
	
	/** Pointer to the samples display widget */	
	TSharedPtr<SBlendSpaceSamplesWidget> SamplesWidget;

	/** This is used to tell the user when they do something wrong */
	TSharedPtr<SNotificationList> NotificationListPtr;

	/** Whether the active timer to update the preview is registered */
	bool bIsActiveTimerRegistered;
};

