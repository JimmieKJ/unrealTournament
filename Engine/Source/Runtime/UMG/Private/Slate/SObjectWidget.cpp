// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "UMGDragDropOp.h"

void SObjectWidget::Construct(const FArguments& InArgs, UUserWidget* InWidgetObject)
{
	WidgetObject = InWidgetObject;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

SObjectWidget::~SObjectWidget(void)
{
	ResetWidget();
}

void SObjectWidget::ResetWidget()
{
	if ( UObjectInitialized() && WidgetObject )
	{
		// NOTE: When the SObjectWidget gets released we know that the User Widget has
		// been removed from the slate widget hierarchy.  When this occurs, we need to 
		// immediately release all slate widget widgets to prevent deletion from taking
		// n-frames due to widget nesting.
		const bool bReleaseChildren = true;
		WidgetObject->ReleaseSlateResources(bReleaseChildren);

		WidgetObject = nullptr;
	}

	// Remove slate widget from our container
	ChildSlot
	[
		SNullWidget::NullWidget
	];
}

void SObjectWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(WidgetObject);
}

void SObjectWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->NativeTick(AllottedGeometry, InDeltaTime);
	}
}

int32 SObjectWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MaxLayer = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		FPaintContext Context(AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
		WidgetObject->OnPaint(Context);
		
		return FMath::Max(MaxLayer, Context.MaxLayer);
	}
	
	return MaxLayer;
}

bool SObjectWidget::SupportsKeyboardFocus() const
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->bSupportsKeyboardFocus;
	}

	return false;
}

FReply SObjectWidget::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnFocusReceived(MyGeometry, InFocusEvent).NativeReply;
	}

	return FReply::Unhandled();
}

void SObjectWidget::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		WidgetObject->OnFocusLost(InFocusEvent);
	}
}

void SObjectWidget::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath)
{
	// TODO UMG
}

FReply SObjectWidget::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnKeyChar(MyGeometry, InCharacterEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnPreviewKeyDown(MyGeometry, InKeyEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnKeyDown(MyGeometry, InKeyEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnKeyUp(MyGeometry, InKeyEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	if (WidgetObject && !WidgetObject->IsDesignTime())
	{
		return WidgetObject->OnAnalogValueChanged(MyGeometry, InAnalogInputEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnMouseButtonDown(MyGeometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnPreviewMouseButtonDown(MyGeometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnMouseButtonUp(MyGeometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnMouseMove(MyGeometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

void SObjectWidget::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		WidgetObject->OnMouseEnter(MyGeometry, MouseEvent);
	}
}

void SObjectWidget::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		WidgetObject->OnMouseLeave(MouseEvent);
	}
}

FReply SObjectWidget::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnMouseWheel(MyGeometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FCursorReply SObjectWidget::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	// TODO UMG  Allow conditional overriding of the cursor logic.
	return FCursorReply::Unhandled();
}

FReply SObjectWidget::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnMouseButtonDoubleClick(MyGeometry, MouseEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent)
{
	UDragDropOperation* Operation = nullptr;
	WidgetObject->OnDragDetected(MyGeometry, PointerEvent, Operation);

	if ( Operation )
	{
		FVector2D ScreenCursorPos = PointerEvent.GetScreenSpacePosition();
		FVector2D ScreenDrageePosition = MyGeometry.AbsolutePosition;

		return FReply::Handled().BeginDragDrop(FUMGDragDropOp::New(Operation, ScreenCursorPos, ScreenDrageePosition, SharedThis(this)));
	}

	return FReply::Unhandled();
}

void SObjectWidget::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FUMGDragDropOp> NativeOp = DragDropEvent.GetOperationAs<FUMGDragDropOp>();
	if ( NativeOp.IsValid() )
	{
		WidgetObject->OnDragEnter(MyGeometry, DragDropEvent, NativeOp->GetOperation());
	}
}

void SObjectWidget::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FUMGDragDropOp> NativeOp = DragDropEvent.GetOperationAs<FUMGDragDropOp>();
	if ( NativeOp.IsValid() )
	{
		WidgetObject->OnDragLeave(DragDropEvent, NativeOp->GetOperation());
	}
}

FReply SObjectWidget::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FUMGDragDropOp> NativeOp = DragDropEvent.GetOperationAs<FUMGDragDropOp>();
	if ( NativeOp.IsValid() )
	{
		if ( WidgetObject->OnDragOver(MyGeometry, DragDropEvent, NativeOp->GetOperation()) )
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FUMGDragDropOp> NativeOp = DragDropEvent.GetOperationAs<FUMGDragDropOp>();
	if ( NativeOp.IsValid() )
	{
		if ( WidgetObject->OnDrop(MyGeometry, DragDropEvent, NativeOp->GetOperation()) )
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SObjectWidget::OnDragCancelled(const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation)
{
	TSharedPtr<FUMGDragDropOp> NativeOp = DragDropEvent.GetOperationAs<FUMGDragDropOp>();
	if ( NativeOp.IsValid() )
	{
		WidgetObject->OnDragCancelled(DragDropEvent, NativeOp->GetOperation());
	}
}

FReply SObjectWidget::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnTouchGesture(MyGeometry, GestureEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnTouchStarted(MyGeometry, InTouchEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnTouchMoved(MyGeometry, InTouchEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnTouchEnded(MyGeometry, InTouchEvent).NativeReply;
	}

	return FReply::Unhandled();
}

FReply SObjectWidget::OnMotionDetected(const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent)
{
	if ( WidgetObject && !WidgetObject->IsDesignTime() )
	{
		return WidgetObject->OnMotionDetected(MyGeometry, InMotionEvent).NativeReply;
	}

	return FReply::Unhandled();
}
