// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "Widgets/SWidget.h"
#include "Input/Events.h"
#include "ActiveTimerHandle.h"
#include "SlateStats.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Widgets"), STAT_SlateTotalWidgets, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Painted Widgets"), STAT_SlateNumPaintedWidgets, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Ticked Widgets"), STAT_SlateNumTickedWidgets, STATGROUP_Slate);

SLATE_DECLARE_CYCLE_COUNTER(GSlateWidgetTick, "SWidget Tick");
SLATE_DECLARE_CYCLE_COUNTER(GSlateOnPaint, "OnPaint");
SLATE_DECLARE_CYCLE_COUNTER(GSlatePrepass, "SlatePrepass");
SLATE_DECLARE_CYCLE_COUNTER(GSlateArrangeChildren, "ArrangeChildren");
SLATE_DECLARE_CYCLE_COUNTER(GSlateGetVisibility, "GetVisibility");

TAutoConsoleVariable<int32> TickInvisibleWidgets(TEXT("Slate.TickInvisibleWidgets"), 0, TEXT("Controls whether invisible widgets are ticked."));

SLATECORE_API int32 bFoldTick = 1;
FAutoConsoleVariableRef FoldTick(TEXT("Slate.FoldTick"), bFoldTick, TEXT("When folding, call Tick as part of the paint pass instead of a separate tick pass."));

SWidget::SWidget()
	: Cursor( TOptional<EMouseCursor::Type>() )
	, EnabledState( true )
	, Visibility( EVisibility::Visible )
	, RenderTransform( )
	, RenderTransformPivot( FVector2D::ZeroVector )
	, DesiredSize(FVector2D::ZeroVector)
	, ToolTip()
	, LayoutCache(nullptr)
	, bIsHovered(false)
	, bCanTick(true)
	, bCanSupportFocus(true)
	, bCanHaveChildren(true)
	, bToolTipForceFieldEnabled(false)
	, bForceVolatile(false)
	, bCachedVolatile(false)
	, bInheritedVolatility(false)
{
	if (GIsRunning)
	{
		INC_DWORD_STAT(STAT_SlateTotalWidgets);
	}
}

SWidget::~SWidget()
{
	// Unregister all ActiveTimers so they aren't left stranded in the Application's list.
	if ( FSlateApplicationBase::IsInitialized() )
	{
		for ( const auto& ActiveTimerHandle : ActiveTimers )
		{
			FSlateApplicationBase::Get().UnRegisterActiveTimer(ActiveTimerHandle);
		}
	}

	DEC_DWORD_STAT(STAT_SlateTotalWidgets);
}

void SWidget::Construct(
	const TAttribute<FText> & InToolTipText ,
	const TSharedPtr<IToolTip> & InToolTip ,
	const TAttribute< TOptional<EMouseCursor::Type> > & InCursor ,
	const TAttribute<bool> & InEnabledState ,
	const TAttribute<EVisibility> & InVisibility,
	const TAttribute<TOptional<FSlateRenderTransform>>& InTransform,
	const TAttribute<FVector2D>& InTransformPivot,
	const FName& InTag,
	const bool InForceVolatile,
	const TArray<TSharedRef<ISlateMetaData>>& InMetaData
)
{
	if ( InToolTip.IsValid() )
	{
		// If someone specified a fancy widget tooltip, use it.
		ToolTip = InToolTip;
	}
	else if ( InToolTipText.IsSet() )
	{
		// If someone specified a text binding, make a tooltip out of it
		ToolTip = FSlateApplicationBase::Get().MakeToolTip(InToolTipText);
	}
	else if( !ToolTip.IsValid() || (ToolTip.IsValid() && ToolTip->IsEmpty()) )
	{	
		// We don't have a tooltip.
		ToolTip.Reset();
	}

	Cursor = InCursor;
	EnabledState = InEnabledState;
	Visibility = InVisibility;
	RenderTransform = InTransform;
	RenderTransformPivot = InTransformPivot;
	Tag = InTag;
	bForceVolatile = InForceVolatile;
	MetaData = InMetaData;
}

FReply SWidget::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Unhandled();
}

void SWidget::OnFocusLost(const FFocusEvent& InFocusEvent)
{
}

void SWidget::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath)
{
}

void SWidget::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnFocusChanging(PreviousFocusPath, NewWidgetPath);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FReply SWidget::OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnPreviewKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if (SupportsKeyboardFocus())
	{
		EUINavigation Direction = FSlateApplicationBase::Get().GetNavigationDirectionFromKey( InKeyEvent );
		// It's the left stick return a navigation request of the correct direction
		if ( Direction != EUINavigation::Invalid )
		{
			return FReply::Handled().SetNavigation( Direction );
		}
	}
	return FReply::Unhandled();
}

FReply SWidget::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseButtonDownHandler.IsBound())
	{
		// If a handler is assigned, call it.
		return MouseButtonDownHandler.Execute(MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply SWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseButtonUpHandler.IsBound())
	{
		// If a handler is assigned, call it.
		return MouseButtonUpHandler.Execute(MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply SWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseMoveHandler.IsBound())
	{
		// A valid handler is assigned for mouse move; let it handle the event.
		return MouseMoveHandler.Execute(MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

FReply SWidget::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseDoubleClickHandler.IsBound())
	{
		// A valid handler is assigned; let it handle the event.
		return MouseDoubleClickHandler.Execute(MyGeometry, MouseEvent);
	}
	return FReply::Unhandled();
}

void SWidget::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bIsHovered = true;
}

void SWidget::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	bIsHovered = false;
}

FReply SWidget::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}

FCursorReply SWidget::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	TOptional<EMouseCursor::Type> TheCursor = Cursor.Get();
	return ( TheCursor.IsSet() )
		? FCursorReply::Cursor( TheCursor.GetValue() )
		: FCursorReply::Unhandled();
}

TOptional<TSharedRef<SWidget>> SWidget::OnMapCursor(const FCursorReply& CursorReply) const
{
	return TOptional<TSharedRef<SWidget>>();
}

bool SWidget::OnVisualizeTooltip( const TSharedPtr<SWidget>& TooltipContent )
{
	return false;
}

TSharedPtr<FPopupLayer> SWidget::OnVisualizePopup(const TSharedRef<SWidget>& PopupContent)
{
	return TSharedPtr<FPopupLayer>();
}

FReply SWidget::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}

void SWidget::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
}

void SWidget::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
}

FReply SWidget::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnControllerButtonPressed( const FGeometry& MyGeometry, const FControllerEvent& ControllerEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnControllerButtonReleased( const FGeometry& MyGeometry, const FControllerEvent& ControllerEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnControllerAnalogValueChanged( const FGeometry& MyGeometry, const FControllerEvent& ControllerEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& GestureEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	return FReply::Unhandled();
}

FReply SWidget::OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent )
{
	return FReply::Unhandled();
}

TOptional<bool> SWidget::OnQueryShowFocus(const EFocusCause InFocusCause) const
{
	return TOptional<bool>();
}

FPopupMethodReply SWidget::OnQueryPopupMethod() const
{
	return FPopupMethodReply::Unhandled();
}

TSharedPtr<struct FVirtualPointerPosition> SWidget::TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& MyGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const
{
	return nullptr;
}

void SWidget::OnFinishedPointerInput()
{

}

void SWidget::OnFinishedKeyInput()
{

}

FNavigationReply SWidget::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	EUINavigation Type = InNavigationEvent.GetNavigationType();
	TSharedPtr<FNavigationMetaData> NavigationMetaData = GetMetaData<FNavigationMetaData>();
	if (NavigationMetaData.IsValid())
	{
		TSharedPtr<SWidget> Widget = NavigationMetaData->GetFocusRecipient(Type).Pin();
		return FNavigationReply(NavigationMetaData->GetBoundaryRule(Type), Widget, NavigationMetaData->GetFocusDelegate(Type));
	}
	return FNavigationReply::Escape();
}

EWindowZone::Type SWidget::GetWindowZoneOverride() const
{
	// No special behavior.  Override this in derived widgets, if needed.
	return EWindowZone::Unspecified;
}

void SWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
}

void SWidget::TickWidgetsRecursively( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	INC_DWORD_STAT(STAT_SlateNumTickedWidgets);

	// Execute any pending active timers for this widget, followed by the passive tick
	ExecuteActiveTimers( InCurrentTime, InDeltaTime );
	{
		SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_MED, GSlateWidgetTick, GetType());
		Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	}

	// Gather all children, whether they're visible or not.  We need to allow invisible widgets to
	// consider whether they should still be invisible in their tick functions, as well as maintain
	// other state when hidden,
	FArrangedChildren ArrangedChildren(TickInvisibleWidgets.GetValueOnGameThread() ? EVisibility::All : EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	// Recur!
	for(int32 ChildIndex=0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& SomeChild = ArrangedChildren[ChildIndex];
		SomeChild.Widget->TickWidgetsRecursively( SomeChild.Geometry, InCurrentTime, InDeltaTime );
	}
}

void SWidget::SlatePrepass()
{
	SlatePrepass( FSlateApplicationBase::Get().GetApplicationScale() );
}

void SWidget::SlatePrepass(float LayoutScaleMultiplier)
{
	//SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_MED, GSlatePrepass, GetType());

	// TODO Figure out a better way than to just reset the pointer.  This causes problems when we prepass
	// volatile widgets, who still need to know about their invalidation panel incase they vanish themselves.

	// Reset the layout cache object each pre-pass to ensure we never access a stale layout cache object 
	// as this widget could have been moved in and out of a panel that was invalidated between frames.
	//LayoutCache = nullptr;

	if ( bCanHaveChildren )
	{
		// Cache child desired sizes first. This widget's desired size is
		// a function of its children's sizes.
		FChildren* MyChildren = this->GetChildren();
		int32 NumChildren = MyChildren->Num();
		for ( int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const TSharedRef<SWidget>& Child = MyChildren->GetChildAt(ChildIndex);

			if ( Child->Visibility.Get() != EVisibility::Collapsed )
			{
				const float ChildLayoutScaleMultiplier = GetRelativeLayoutScale(MyChildren->GetSlotAt(ChildIndex));
				// Recur: Descend down the widget tree.
				Child->SlatePrepass(LayoutScaleMultiplier*ChildLayoutScaleMultiplier);
			}
		}
	}

	// Cache this widget's desired size.
	CacheDesiredSize(LayoutScaleMultiplier);
}

void SWidget::CacheDesiredSize(float LayoutScaleMultiplier)
{
	// Cache this widget's desired size.
	this->Advanced_SetDesiredSize(this->ComputeDesiredSize(LayoutScaleMultiplier));
}

void SWidget::CachePrepass(const TWeakPtr<ILayoutCache>& InLayoutCache)
{
	if ( bCanHaveChildren )
	{
		FChildren* MyChildren = this->GetChildren();
		int32 NumChildren = MyChildren->Num();
		for ( int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const TSharedRef<SWidget>& Child = MyChildren->GetChildAt(ChildIndex);
			if ( Child->GetVisibility().IsVisible() == false )
			{
				Child->LayoutCache = InLayoutCache;
			}
			else
			{
				Child->CachePrepass(InLayoutCache);
			}
		}
	}
}

bool SWidget::SupportsKeyboardFocus() const
{
	return false;
}

bool SWidget::HasKeyboardFocus() const
{
	return (FSlateApplicationBase::Get().GetKeyboardFocusedWidget().Get() == this);
}

TOptional<EFocusCause> SWidget::HasUserFocus(int32 UserIndex) const
{
	return FSlateApplicationBase::Get().HasUserFocus(SharedThis(this), UserIndex);
}

TOptional<EFocusCause> SWidget::HasAnyUserFocus() const
{
	return FSlateApplicationBase::Get().HasAnyUserFocus(SharedThis(this));
}

bool SWidget::HasUserFocusedDescendants(int32 UserIndex) const
{
	return FSlateApplicationBase::Get().HasUserFocusedDescendants(SharedThis(this), UserIndex);
}

bool SWidget::HasFocusedDescendants() const
{
	return FSlateApplicationBase::Get().HasFocusedDescendants(SharedThis(this));
}

bool SWidget::HasAnyUserFocusOrFocusedDescendants() const
{
	return HasAnyUserFocus().IsSet() || HasFocusedDescendants();
}

const FSlateBrush* SWidget::GetFocusBrush() const
{
	return FCoreStyle::Get().GetBrush("FocusRectangle");
}

bool SWidget::HasMouseCapture() const
{
	return FSlateApplicationBase::Get().DoesWidgetHaveMouseCapture(SharedThis(this));
}

bool SWidget::HasMouseCaptureByUser(int32 UserIndex, TOptional<int32> PointerIndex) const
{
	return FSlateApplicationBase::Get().DoesWidgetHaveMouseCaptureByUser(SharedThis(this), UserIndex, PointerIndex);
}

void SWidget::OnMouseCaptureLost()
{
	
}

bool SWidget::FindChildGeometries( const FGeometry& MyGeometry, const TSet< TSharedRef<SWidget> >& WidgetsToFind, TMap<TSharedRef<SWidget>, FArrangedWidget>& OutResult ) const
{
	FindChildGeometries_Helper(MyGeometry, WidgetsToFind, OutResult);
	return OutResult.Num() == WidgetsToFind.Num();
}


void SWidget::FindChildGeometries_Helper( const FGeometry& MyGeometry, const TSet< TSharedRef<SWidget> >& WidgetsToFind, TMap<TSharedRef<SWidget>, FArrangedWidget>& OutResult ) const
{
	// Perform a breadth first search!

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	this->ArrangeChildren( MyGeometry, ArrangedChildren );
	const int32 NumChildren = ArrangedChildren.Num();

	// See if we found any of the widgets on this level.
	for(int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
	{
		const FArrangedWidget& CurChild = ArrangedChildren[ ChildIndex ];
		
		if ( WidgetsToFind.Contains(CurChild.Widget) )
		{
			// We found one of the widgets for which we need geometry!
			OutResult.Add( CurChild.Widget, CurChild );
		}
	}

	// If we have not found all the widgets that we were looking for, descend.
	if ( OutResult.Num() != WidgetsToFind.Num() )
	{
		// Look for widgets among the children.
		for( int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
		{
			const FArrangedWidget& CurChild = ArrangedChildren[ ChildIndex ];
			CurChild.Widget->FindChildGeometries_Helper( CurChild.Geometry, WidgetsToFind, OutResult );
		}	
	}	
}

FGeometry SWidget::FindChildGeometry( const FGeometry& MyGeometry, TSharedRef<SWidget> WidgetToFind ) const
{
	// We just need to find the one WidgetToFind among our descendants.
	TSet< TSharedRef<SWidget> > WidgetsToFind;
	{
		WidgetsToFind.Add( WidgetToFind );
	}
	TMap<TSharedRef<SWidget>, FArrangedWidget> Result;

	FindChildGeometries( MyGeometry, WidgetsToFind, Result );

	return Result.FindChecked( WidgetToFind ).Geometry;
}

int32 SWidget::FindChildUnderMouse( const FArrangedChildren& Children, const FPointerEvent& MouseEvent )
{
	const FVector2D& AbsoluteCursorLocation = MouseEvent.GetScreenSpacePosition();
	return SWidget::FindChildUnderPosition( Children, AbsoluteCursorLocation );
}

int32 SWidget::FindChildUnderPosition( const FArrangedChildren& Children, const FVector2D& ArrangedSpacePosition )
{
	const int32 NumChildren = Children.Num();
	for( int32 ChildIndex=NumChildren-1; ChildIndex >= 0; --ChildIndex )
	{
		const FArrangedWidget& Candidate = Children[ChildIndex];
		const bool bCandidateUnderCursor = 
			// Candidate is physically under the cursor
			Candidate.Geometry.IsUnderLocation( ArrangedSpacePosition );

		if (bCandidateUnderCursor)
		{
			return ChildIndex;
		}
	}

	return INDEX_NONE;
}

FString SWidget::ToString() const
{
	return FString::Printf(TEXT("%s [%s]"), *this->TypeOfWidget.ToString(), *this->GetReadableLocation() );
}

FString SWidget::GetTypeAsString() const
{
	return this->TypeOfWidget.ToString();
}

FName SWidget::GetType() const
{
	return TypeOfWidget;
}

FString SWidget::GetReadableLocation() const
{
	return FString::Printf(TEXT("%s(%d)"), *FPaths::GetCleanFilename(this->CreatedInLocation.GetPlainNameString()), this->CreatedInLocation.GetNumber());
}

FName SWidget::GetCreatedInLocation() const
{
	return CreatedInLocation;
}

FName SWidget::GetTag() const
{
	return Tag;
}

FSlateColor SWidget::GetForegroundColor() const
{
	static FSlateColor NoColor = FSlateColor::UseForeground();
	return NoColor;
}

void SWidget::SetToolTipText(const TAttribute<FText>& ToolTipText)
{
	ToolTip = FSlateApplicationBase::Get().MakeToolTip(ToolTipText);
}

void SWidget::SetToolTipText( const FText& ToolTipText )
{
	ToolTip = FSlateApplicationBase::Get().MakeToolTip(ToolTipText);
}

void SWidget::SetToolTip( const TSharedPtr<IToolTip> & InToolTip )
{
	ToolTip = InToolTip;
}

TSharedPtr<IToolTip> SWidget::GetToolTip()
{
	return ToolTip;
}

void SWidget::OnToolTipClosing()
{
}

void SWidget::EnableToolTipForceField( const bool bEnableForceField )
{
	bToolTipForceFieldEnabled = bEnableForceField;
}

bool SWidget::IsDirectlyHovered() const
{
	return FSlateApplicationBase::Get().IsWidgetDirectlyHovered(SharedThis(this));
}

void SWidget::SetCursor( const TAttribute< TOptional<EMouseCursor::Type> >& InCursor )
{
	Cursor = InCursor;
}

void SWidget::SetDebugInfo( const ANSICHAR* InType, const ANSICHAR* InFile, int32 OnLine )
{
	TypeOfWidget = InType;
	CreatedInLocation = FName( InFile );
	CreatedInLocation.SetNumber(OnLine);
}

int32 SWidget::Paint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	INC_DWORD_STAT(STAT_SlateNumPaintedWidgets);
	SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_MED, GSlateOnPaint, GetType());

	// Save the current layout cache we're associated with (if any)
	LayoutCache = Args.GetLayoutCache();

	// Record if we're part of a volatility pass, this is critical for ensuring we don't report a child
	// of a volatile widget as non-volatile, causing the invalidation panel to do work that's not required.
	bInheritedVolatility = Args.IsVolatilityPass();

	// If this paint pass is to cache off our geometry, but we're a volatile widget,
	// record this widget as volatile in the draw elements so that we get our own tick/paint 
	// pass later when the layout cache draws.
	if ( Args.IsCaching() && IsVolatile() )
	{
		const int32 VolatileLayerId = LayerId + 1;
		OutDrawElements.QueueVolatilePainting(
			FSlateWindowElementList::FVolatilePaint(SharedThis(this), Args, AllottedGeometry, MyClippingRect, VolatileLayerId, InWidgetStyle, bParentEnabled));

		return VolatileLayerId;
	}

	if ( bFoldTick && bCanTick )
	{
		FGeometry TickGeometry = AllottedGeometry;
		TickGeometry.AppendTransform( FSlateLayoutTransform(Args.GetWindowToDesktopTransform()) );

		SWidget* MutableThis = const_cast<SWidget*>(this);
		MutableThis->ExecuteActiveTimers( Args.GetCurrentTime(), Args.GetDeltaTime() );
		MutableThis->Tick( TickGeometry, Args.GetCurrentTime(), Args.GetDeltaTime() );
	}

	// Record hit test geometry, but only if we're not caching.
	const FPaintArgs UpdatedArgs = Args.RecordHittestGeometry(this, AllottedGeometry, LayerId, MyClippingRect);

	// Paint the geometry of this widget.
	int32 NewLayerID = OnPaint(UpdatedArgs, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// Check if we need to show the keyboard focus ring, this is only necessary if the widget could be focused.
	if ( bCanSupportFocus && SupportsKeyboardFocus() )
	{
		bool bShowUserFocus = FSlateApplicationBase::Get().ShowUserFocus(SharedThis(this));
		if (bShowUserFocus)
		{
			const FSlateBrush* BrushResource = GetFocusBrush();

			if (BrushResource != nullptr)
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					NewLayerID,
					AllottedGeometry.ToPaintGeometry(),
					BrushResource,
					MyClippingRect,
					ESlateDrawEffect::None,
					BrushResource->GetTint(InWidgetStyle)
					);
			}
		}
	}

	if ( OutDrawElements.ShouldResolveDeferred() )
	{
		NewLayerID = OutDrawElements.PaintDeferred(NewLayerID);
	}

	return NewLayerID;
}

float SWidget::GetRelativeLayoutScale(const FSlotBase& Child) const
{
	return 1.0f;
}

TSharedRef<FActiveTimerHandle> SWidget::RegisterActiveTimer(float TickPeriod, FWidgetActiveTimerDelegate TickFunction)
{
	TSharedRef<FActiveTimerHandle> ActiveTimerHandle = MakeShareable(new FActiveTimerHandle(TickPeriod, TickFunction, FSlateApplicationBase::Get().GetCurrentTime() + TickPeriod));
	FSlateApplicationBase::Get().RegisterActiveTimer(ActiveTimerHandle);
	ActiveTimers.Add(ActiveTimerHandle);
	return ActiveTimerHandle;
}

void SWidget::UnRegisterActiveTimer(const TSharedRef<FActiveTimerHandle>& ActiveTimerHandle)
{
	if (FSlateApplicationBase::IsInitialized())
	{
		FSlateApplicationBase::Get().UnRegisterActiveTimer(ActiveTimerHandle);
		ActiveTimers.Remove(ActiveTimerHandle);
	}
}

void SWidget::ExecuteActiveTimers(double CurrentTime, float DeltaTime)
{
	// loop over the registered tick handles and execute them, removing them if necessary.
	for (int32 i = 0; i < ActiveTimers.Num();)
	{
		EActiveTimerReturnType Result = ActiveTimers[i]->ExecuteIfPending( CurrentTime, DeltaTime );
		if (Result == EActiveTimerReturnType::Continue)
		{
			++i;
		}
		else
		{
			if (FSlateApplicationBase::IsInitialized())
			{
				FSlateApplicationBase::Get().UnRegisterActiveTimer(ActiveTimers[i]);
			}
			
			ActiveTimers.RemoveAt(i);
		}
	}
}

void SWidget::SetOnMouseButtonDown(FPointerEventHandler EventHandler)
{
	MouseButtonDownHandler = EventHandler;
}

void SWidget::SetOnMouseButtonUp(FPointerEventHandler EventHandler)
{
	MouseButtonUpHandler = EventHandler;
}

void SWidget::SetOnMouseMove(FPointerEventHandler EventHandler)
{
	MouseMoveHandler = EventHandler;
}

void SWidget::SetOnMouseDoubleClick(FPointerEventHandler EventHandler)
{
	MouseDoubleClickHandler = EventHandler;
}
