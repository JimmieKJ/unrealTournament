// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "Widgets/SWidget.h"
#include "Input/Events.h"
#include "ActiveTimerHandle.h"
#include "SlateStats.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Painted Widgets"), STAT_SlateNumPaintedWidgets, STATGROUP_Slate);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Ticked Widgets"), STAT_SlateNumTickedWidgets, STATGROUP_Slate);

SLATE_DECLARE_CYCLE_COUNTER(GSlateWidgetTick, "SWidget Tick");
SLATE_DECLARE_CYCLE_COUNTER(GSlateOnPaint, "OnPaint");
SLATE_DECLARE_CYCLE_COUNTER(GSlatePrepass, "SlatePrepass");
SLATE_DECLARE_CYCLE_COUNTER(GSlateArrangeChildren, "ArrangeChildren");
SLATE_DECLARE_CYCLE_COUNTER(GSlateGetVisibility, "GetVisibility");

TAutoConsoleVariable<int32> TickInvisibleWidgets(TEXT("Slate.TickInvisibleWidgets"), 0, TEXT("Controls whether invisible widgets are ticked."));

SWidget::SWidget()
	: Cursor( TOptional<EMouseCursor::Type>() )
	, EnabledState( true )
	, Visibility( EVisibility::Visible )
	, RenderTransform( )
	, RenderTransformPivot( FVector2D::ZeroVector )
	, bIsHovered(false)
	, DesiredSize(FVector2D::ZeroVector)
	, ToolTip()
	, bToolTipForceFieldEnabled( false )
{

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
		// It's the left stick return a navigation request of the correct direction
		if (InKeyEvent.GetKey() == EKeys::Right || InKeyEvent.GetKey() == EKeys::Gamepad_DPad_Right || InKeyEvent.GetKey() == EKeys::Gamepad_LeftStick_Right)
		{
			return FReply::Handled().SetNavigation(EUINavigation::Right);
		}
		else if (InKeyEvent.GetKey() == EKeys::Left || InKeyEvent.GetKey() == EKeys::Gamepad_DPad_Left || InKeyEvent.GetKey() == EKeys::Gamepad_LeftStick_Left)
		{
			return FReply::Handled().SetNavigation(EUINavigation::Left);
		}
		else if (InKeyEvent.GetKey() == EKeys::Up || InKeyEvent.GetKey() == EKeys::Gamepad_DPad_Up || InKeyEvent.GetKey() == EKeys::Gamepad_LeftStick_Up)
		{
			return FReply::Handled().SetNavigation(EUINavigation::Up);
		}
		else if (InKeyEvent.GetKey() == EKeys::Down || InKeyEvent.GetKey() == EKeys::Gamepad_DPad_Down || InKeyEvent.GetKey() == EKeys::Gamepad_LeftStick_Down)
		{
			return FReply::Handled().SetNavigation(EUINavigation::Down);
		}
		// If the key was Tab, interpret as an attempt to move focus.
		else if (InKeyEvent.GetKey() == EKeys::Tab)
		{
			//@TODO: Really these uses of input should be at a lower priority, only occurring if nothing else handled them
			// For now this code prevents consuming them when some modifiers are held down, allowing some limited binding
			const bool bAllowEatingKeyEvents = !InKeyEvent.IsControlDown() && !InKeyEvent.IsAltDown() && !InKeyEvent.IsCommandDown();

			if (bAllowEatingKeyEvents)
			{
				EUINavigation MoveDirection = (InKeyEvent.IsShiftDown())
					? EUINavigation::Previous
					: EUINavigation::Next;
				return FReply::Handled().SetNavigation(MoveDirection);
			}
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


FReply SWidget::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}


FReply SWidget::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}


FReply SWidget::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Unhandled();
}


FReply SWidget::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
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


FReply SWidget::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return FReply::Unhandled();
}


bool SWidget::OnVisualizeTooltip( const TSharedPtr<SWidget>& TooltipContent )
{
	return false;
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

TOptional<EPopupMethod> SWidget::OnQueryPopupMethod() const
{
	return TOptional<EPopupMethod>();
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
	SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_MED, GSlatePrepass, GetType());
	// Cache child desired sizes first. This widget's desired size is
	// a function of its children's sizes.
	FChildren* MyChildren = this->GetChildren();
	int32 NumChildren = MyChildren->Num();
	for(int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex)
	{
		const TSharedRef<SWidget>& Child = MyChildren->GetChildAt(ChildIndex);
		if ( Child->Visibility.Get() != EVisibility::Collapsed )
		{
			const float ChildLayoutScaleMultiplier = GetRelativeLayoutScale( MyChildren->GetSlotAt(ChildIndex) );
			// Recur: Descend down the widget tree.
			Child->SlatePrepass(LayoutScaleMultiplier*ChildLayoutScaleMultiplier);
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


const FVector2D& SWidget::GetDesiredSize() const
{
	return DesiredSize;
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

bool SWidget::HasFocusedDescendants() const
{
	return FSlateApplicationBase::Get().HasFocusedDescendants(SharedThis(this));
}

const FSlateBrush* SWidget::GetFocusBrush() const
{
	return FCoreStyle::Get().GetBrush("FocusRectangle");
}

bool SWidget::HasMouseCapture() const
{
	return FSlateApplicationBase::Get().HasMouseCapture(SharedThis(this));
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
	return this->TypeOfWidget;
}


FString SWidget::GetReadableLocation() const
{
	return FString::Printf(TEXT("%s(%d)"), *FPaths::GetCleanFilename(this->CreatedInLocation.GetPlainNameString()), this->CreatedInLocation.GetNumber());
}


FName SWidget::GetCreatedInLocation() const
{
	return this->CreatedInLocation;
}

FName SWidget::GetTag() const
{
	return this->Tag;
}


FSlateColor SWidget::GetForegroundColor() const
{
	static FSlateColor NoColor = FSlateColor::UseForeground();
	return NoColor;
}


void SWidget::SetToolTipText( const TAttribute<FString>& ToolTipString )
{
	ToolTip = FSlateApplicationBase::Get().MakeToolTip(ToolTipString);
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


void SWidget::SetCursor( const TAttribute< TOptional<EMouseCursor::Type> >& InCursor )
{
	Cursor = InCursor;
}


void SWidget::SetDebugInfo( const ANSICHAR* InType, const ANSICHAR* InFile, int32 OnLine )
{
	this->TypeOfWidget = InType;
	this->
		CreatedInLocation = FName( InFile );
	this->CreatedInLocation.SetNumber(OnLine);
}

SLATECORE_API int32 bFoldTick = 1;
FAutoConsoleVariableRef FoldTick(TEXT("Slate.FoldTick"), bFoldTick, TEXT("When folding, call Tick as part of the paint pass instead of a separate tick pass."));

int32 SWidget::Paint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	INC_DWORD_STAT(STAT_SlateNumPaintedWidgets);
	SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_MED, GSlateOnPaint, GetType());

	if ( bFoldTick )
	{
		FGeometry TickGeometry = AllottedGeometry;
		TickGeometry.AppendTransform( FSlateLayoutTransform(Args.GetWindowToDesktopTransform()) );

		SWidget* MutableThis = const_cast<SWidget*>(this);
		MutableThis->ExecuteActiveTimers( Args.GetCurrentTime(), Args.GetDeltaTime() );
		MutableThis->Tick( TickGeometry, Args.GetCurrentTime(), Args.GetDeltaTime() );
	}

	const FPaintArgs UpdatedArgs = Args.RecordHittestGeometry( this, AllottedGeometry, MyClippingRect );
	int32 NewLayerID = OnPaint(UpdatedArgs, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (SupportsKeyboardFocus())
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
					FColor(255, 255, 255, 128)
					);
			}
		}
	}

	return NewLayerID;
}

float SWidget::GetRelativeLayoutScale(const FSlotBase& Child) const
{
	return 1.0f;
}

void SWidget::ArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_MED, GSlateArrangeChildren, GetType());
	OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

EVisibility SWidget::GetVisibility() const
{
	SLATE_CYCLE_COUNTER_SCOPE_CUSTOM_DETAILED(SLATE_STATS_DETAIL_LEVEL_HI,GSlateGetVisibility, GetType());
	return Visibility.Get();
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

SWidget::~SWidget()
{
	// Unregister all ActiveTimers so they aren't left stranded in the Application's list.
	if (FSlateApplicationBase::IsInitialized())
	{
		for (const auto& ActiveTimerHandle : ActiveTimers)
		{
			FSlateApplicationBase::Get().UnRegisterActiveTimer(ActiveTimerHandle);
		}
	}
}
