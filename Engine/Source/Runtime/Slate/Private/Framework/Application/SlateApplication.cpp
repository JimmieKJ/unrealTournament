// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "SlatePrivatePCH.h"
#include "SWindowTitleBar.h"
#include "HittestGrid.h"
#include "SlateStats.h"

#include "IWidgetReflector.h"
#include "GenericCommands.h"
#include "NotificationManager.h"
#include "IInputProcessor.h"
#include "ToolboxModule.h"
#include "TabCommands.h"

extern SLATECORE_API TOptional<FShortRect> GSlateScissorRect;

class FEventRouter
{

// @todo slate : making too many event copies when translating events( i.e. Translate<EventType>::PointerEvent ).



// @todo slate : Widget Reflector should log: (1) Every process reply (2) Every time the event is handled and by who.

// @todo slate : Remove remaining [&]-style mass captures.

// @todo slate : Eliminate all ad-hoc uses of SetEventPath()

// @todo slate : Remove CALL_WIDGET_FUNCTION

public:

	class FDirectPolicy
	{
	public:
		FDirectPolicy( const FWidgetAndPointer& InTarget, const FWidgetPath& InRoutingPath )
		: bEventSent(false)
		, RoutingPath(InRoutingPath)
		, Target(InTarget)
		{
		}

		bool ShouldKeepGoing() const
		{
			return !bEventSent;
		}

		void Next()
		{
			bEventSent = true;
		}

		FWidgetAndPointer GetWidget() const
		{
			return Target;
		}

		const FWidgetPath& GetRoutingPath() const
		{
			return RoutingPath;
		}

	private:
		bool bEventSent;
		const FWidgetPath& RoutingPath;
		const FWidgetAndPointer& Target;
	};

	class FToLeafmostPolicy
	{
	public:
		FToLeafmostPolicy( const FWidgetPath& InRoutingPath )
		: bEventSent(false)
		, RoutingPath(InRoutingPath)
		{
		}

		bool ShouldKeepGoing() const
		{
			return !bEventSent && RoutingPath.Widgets.Num() > 0;
		}

		void Next()
		{
			bEventSent = true;
		}

		FWidgetAndPointer GetWidget() const
		{
			const int32 WidgetIndex = RoutingPath.Widgets.Num()-1;
			return FWidgetAndPointer(RoutingPath.Widgets[WidgetIndex], RoutingPath.VirtualPointerPositions[WidgetIndex]);
		}

		const FWidgetPath& GetRoutingPath() const
		{
			return RoutingPath;
		}

	private:
		bool bEventSent;
		const FWidgetPath& RoutingPath;
	};

	class FTunnelPolicy
	{
	public:
		FTunnelPolicy( const FWidgetPath& InRoutingPath )
		: WidgetIndex(0)
		, RoutingPath(InRoutingPath)
		{
		}

		bool ShouldKeepGoing() const
		{
			return WidgetIndex < RoutingPath.Widgets.Num();
		}

		void Next()
		{
			++WidgetIndex;
		}

		FWidgetAndPointer GetWidget() const
		{
			return FWidgetAndPointer(RoutingPath.Widgets[WidgetIndex], RoutingPath.VirtualPointerPositions[WidgetIndex]);
		}
		
		const FWidgetPath& GetRoutingPath() const
		{
			return RoutingPath;
		}


	private:
		int32 WidgetIndex;
		const FWidgetPath& RoutingPath;
	};

	class FBubblePolicy
	{
	public:
		FBubblePolicy( const FWidgetPath& InRoutingPath )
		: WidgetIndex( InRoutingPath.Widgets.Num()-1 )
		, RoutingPath (InRoutingPath)
		{
		}

		bool ShouldKeepGoing() const
		{
			return WidgetIndex >= 0;
		}

		void Next()
		{
			--WidgetIndex;
		}

		FWidgetAndPointer GetWidget() const
		{
			return FWidgetAndPointer(RoutingPath.Widgets[WidgetIndex], RoutingPath.VirtualPointerPositions[WidgetIndex]);
		}

		const FWidgetPath& GetRoutingPath() const
		{
			return RoutingPath;
		}
	
	private:
		int32 WidgetIndex;
		const FWidgetPath& RoutingPath;
	};

	static void LogEvent( FSlateApplication* ThisApplication, const FInputEvent& Event, const FReplyBase& Reply )
	{
		TSharedPtr<IWidgetReflector> Reflector = ThisApplication->WidgetReflectorPtr.Pin();
		if (Reflector.IsValid() && Reply.IsEventHandled())
		{
			Reflector->OnEventProcessed( Event, Reply );
		}
	}

	/**
	 * Route an event along a focus path (as opposed to PointerPath)
	 *
	 * Focus paths are used focus devices.(e.g. Keyboard or Game Pads)
	 * Focus paths change when the user navigates focus (e.g. Tab or
	 * Shift Tab, clicks on a focusable widget, or navigation with keyboard/game pad.)
	 */
	template< typename RoutingPolicyType, typename FuncType, typename EventType >
	static FReply RouteAlongFocusPath( FSlateApplication* ThisApplication, RoutingPolicyType RoutingPolicy, EventType KeyEventCopy, const FuncType& Lambda )
	{
		return Route<FReply>(ThisApplication, RoutingPolicy, KeyEventCopy, Lambda);
	}

	/**
	 * Route an event based on the Routing Policy.
	 */
	template< typename ReplyType, typename RoutingPolicyType, typename EventType, typename FuncType >
	static ReplyType Route( FSlateApplication* ThisApplication, RoutingPolicyType RoutingPolicy, EventType EventCopy, const FuncType& Lambda )
	{
		ReplyType Reply = ReplyType::Unhandled();
		const FWidgetPath& RoutingPath = RoutingPolicy.GetRoutingPath();
		
		EventCopy.SetEventPath( RoutingPath );

		for ( ; !Reply.IsEventHandled() && RoutingPolicy.ShouldKeepGoing(); RoutingPolicy.Next() )
		{
			const FWidgetAndPointer& ArrangedWidget = RoutingPolicy.GetWidget();
			const EventType TranslatedEvent = Translate<EventType>::PointerEvent( ArrangedWidget.PointerPosition, EventCopy );
			Reply = Lambda( ArrangedWidget, TranslatedEvent ).SetHandler( ArrangedWidget.Widget );
			ProcessReply(ThisApplication, RoutingPath, Reply, &RoutingPath, &TranslatedEvent);
		}

		LogEvent(ThisApplication, EventCopy, Reply);

		return Reply;
	}

	static void ProcessReply( FSlateApplication* Application, const FWidgetPath& RoutingPath, const FNoReply& Reply, const FWidgetPath* WidgetsUnderCursor, const FInputEvent* )
	{
	}

	static void ProcessReply( FSlateApplication* Application, const FWidgetPath& RoutingPath, const FCursorReply& Reply, const FWidgetPath* WidgetsUnderCursor, const FInputEvent* )
	{
	}

	static void ProcessReply( FSlateApplication* Application, const FWidgetPath& RoutingPath, const FReply& Reply, const FWidgetPath* WidgetsUnderCursor, const FInputEvent* PointerEvent )
	{
		Application->ProcessReply(RoutingPath, Reply, WidgetsUnderCursor, nullptr);
	}

	static void ProcessReply( FSlateApplication* Application, const FWidgetPath& RoutingPath, const FReply& Reply, const FWidgetPath* WidgetsUnderCursor, const FPointerEvent* PointerEvent )
	{
		Application->ProcessReply(RoutingPath, Reply, WidgetsUnderCursor, PointerEvent);
	}

	template<typename EventType>
	struct Translate
	{
		static EventType PointerEvent( const TSharedPtr<FVirtualPointerPosition>& InPosition, const EventType& InEvent )
		{
			// Most events do not do any coordinate translation.
			return InEvent;
		}
	};

};


template<>
struct FEventRouter::Translate<FPointerEvent>
{
	static  FPointerEvent PointerEvent( const TSharedPtr<FVirtualPointerPosition>& InPosition, const FPointerEvent& InEvent )
	{
		// Pointer events are translated into the virtual window space. For 3D Widget Components this means
		if ( !InPosition.IsValid() )
		{
			return InEvent;
		}
		else
		{
			return FPointerEvent::MakeTranslatedEvent<FPointerEvent>( InEvent, *InPosition );
		}
	}
};


DECLARE_CYCLE_STAT( TEXT("Message Tick Time"), STAT_SlateMessageTick, STATGROUP_Slate );
DECLARE_CYCLE_STAT( TEXT("Update Tooltip Time"), STAT_SlateUpdateTooltip, STATGROUP_Slate );
DECLARE_CYCLE_STAT( TEXT("Tick Window And Children Time"), STAT_SlateTickWindowAndChildren, STATGROUP_Slate );
DECLARE_CYCLE_STAT( TEXT("Total Slate Tick Time"), STAT_SlateTickTime, STATGROUP_Slate );
DECLARE_CYCLE_STAT( TEXT("SlatePrepass"), STAT_SlatePrepass, STATGROUP_Slate );
DECLARE_CYCLE_STAT( TEXT("DrawWindows"), STAT_SlateDrawWindowTime, STATGROUP_Slate );
DECLARE_CYCLE_STAT( TEXT("TickWidgets"), STAT_SlateTickWidgets, STATGROUP_Slate );
DECLARE_CYCLE_STAT( TEXT("TickRegisteredWidgets"), STAT_SlateTickRegisteredWidgets, STATGROUP_Slate );

SLATE_DECLARE_CYCLE_COUNTER(GSlateTotalTickTime, "Total Slate Tick Time");
SLATE_DECLARE_CYCLE_COUNTER(GMessageTickTime, "Message Tick Time");
SLATE_DECLARE_CYCLE_COUNTER(GUpdateTooltipTime, "Update Tooltip Time");
SLATE_DECLARE_CYCLE_COUNTER(GSlateSynthesizeMouseMove, "Synthesize Mouse Move");
SLATE_DECLARE_CYCLE_COUNTER(GTickWindowAndChildrenTime, "Tick Window And Children Time");
SLATE_DECLARE_CYCLE_COUNTER(GTickWidgets, "TickWidgets");
SLATE_DECLARE_CYCLE_COUNTER(GSlateTickNotificationManager, "NotificationManager Tick");
SLATE_DECLARE_CYCLE_COUNTER(GSlateDrawWindows, "DrawWindows");
SLATE_DECLARE_CYCLE_COUNTER(GSlateDrawWindowAndChildren, "Draw Window And Children");
SLATE_DECLARE_CYCLE_COUNTER(GSlateRendererDrawWindows, "Renderer DrawWindows");
SLATE_DECLARE_CYCLE_COUNTER(GSlateDrawPrepass, "DrawPrepass");
SLATE_DECLARE_CYCLE_COUNTER(GSlatePrepassWindowAndChildren, "Prepass Window And Children");

// Slate Event Logging is enabled to allow crash log dumping
#define LOG_SLATE_EVENTS 0


#if LOG_SLATE_EVENTS
	#define LOG_EVENT_CONTENT( EventType, AdditionalContent, WidgetOrReply ) LogSlateEvent(EventLogger, EventType, AdditionalContent, WidgetOrReply);
	
	#define LOG_EVENT( EventType, WidgetOrReply ) LOG_EVENT_CONTENT( EventType, FString(), WidgetOrReply )
	static void LogSlateEvent( const TSharedPtr<IEventLogger>& EventLogger, EEventLog::Type Event, const FString& AdditionalContent, const TSharedPtr<SWidget>& HandlerWidget )
	{
		if (EventLogger.IsValid())
		{
			EventLogger->Log( Event, AdditionalContent, HandlerWidget );
		}
	}

	static void LogSlateEvent( const TSharedPtr<IEventLogger>& EventLogger, EEventLog::Type Event, const FString& AdditionalContent, const FReply& InReply )
	{
		if ( EventLogger.IsValid() && InReply.IsEventHandled() )
		{
			EventLogger->Log( Event, AdditionalContent, InReply.GetHandler() );
		}
	}
#else
	#define LOG_EVENT_CONTENT( EventType, AdditionalContent, WidgetOrReply )
	
	#define LOG_EVENT( Event, WidgetOrReply ) CheckReplyCorrectness(WidgetOrReply);
	static void CheckReplyCorrectness(const TSharedPtr<SWidget>& HandlerWidget)
	{
	}
	static void CheckReplyCorrectness(const FReply& InReply)
	{
		check( !InReply.IsEventHandled() || InReply.GetHandler().IsValid() );
	}
#endif


namespace SlateDefs
{
	// How far tool tips should be offset from the mouse cursor position, in pixels
	static const FVector2D ToolTipOffsetFromMouse( 12.0f, 8.0f );

	// How far tool tips should be pushed out from a force field border, in pixels
	static const FVector2D ToolTipOffsetFromForceField( 4.0f, 3.0f );

	// When true we will clear keyboard focus from widgets within the game view port when the window loses focus
	static bool ClearGameViewportFocusWhenWindowLosesFocus = false;
}


/** True if we should allow throttling based on mouse movement activity.  int32 instead of bool only for console variable system. */
TAutoConsoleVariable<int32> ThrottleWhenMouseIsMoving( 
	TEXT( "Slate.ThrottleWhenMouseIsMoving" ),
	false,
	TEXT( "Whether to attempt to increase UI responsiveness based on mouse cursor movement." ) );

/** Minimum sustained average frame rate required before we consider the editor to be "responsive" for a smooth UI experience */
TAutoConsoleVariable<int32> TargetFrameRateForResponsiveness(
	TEXT( "Slate.TargetFrameRateForResponsiveness" ),
	35,	// Frames per second
	TEXT( "Minimum sustained average frame rate required before we consider the editor to be \"responsive\" for a smooth UI experience" ) );

/** Whether to skip the second Slate PrePass call (the one right before rendering). */
TAutoConsoleVariable<int32> SkipSecondPrepass(
	TEXT("Slate.SkipSecondPrepass"),
	0,
	TEXT("Whether to skip the second Slate PrePass call (the one right before rendering)."));

/** Whether Slate should go to sleep when there are no active timers and the user is idle */
TAutoConsoleVariable<int32> AllowSlateToSleep(
	TEXT("Slate.AllowSlateToSleep"),
	true,
	TEXT("Whether Slate should go to sleep when there are no active timers and the user is idle"));

/** The amount of time that must pass without any user action before Slate is put to sleep (provided that there are no active timers). */
TAutoConsoleVariable<float> SleepBufferPostInput(
	TEXT("Slate.SleepBufferPostInput"),
	0.0f,
	TEXT("The amount of time that must pass without any user action before Slate is put to sleep (provided that there are no active timers)."));

//////////////////////////////////////////////////////////////////////////
bool FSlateApplication::MouseCaptorHelper::HasCapture() const
{
	for (auto PointerPathPair : PointerIndexToMouseCaptorWeakPathMap)
	{
		if (PointerPathPair.Value.IsValid())
		{
			return true;
		}
	}
	return false;
}

bool FSlateApplication::MouseCaptorHelper::HasCaptureForPointerIndex(uint32 PointerIndex) const
{
	const FWeakWidgetPath* MouseCaptorWeakPath = PointerIndexToMouseCaptorWeakPathMap.Find(PointerIndex);
	return MouseCaptorWeakPath && MouseCaptorWeakPath->IsValid();
}

TSharedPtr< SWidget > FSlateApplication::MouseCaptorHelper::ToSharedWidget(uint32 PointerIndex) const
{
	// If the path is valid then get the last widget, this is the current mouse captor
	TSharedPtr< SWidget > SharedWidgetPtr;
	const FWeakWidgetPath* MouseCaptorWeakPath = PointerIndexToMouseCaptorWeakPathMap.Find(PointerIndex);
	if (MouseCaptorWeakPath && MouseCaptorWeakPath->IsValid() )
	{
		TWeakPtr< SWidget > WeakWidgetPtr = MouseCaptorWeakPath->GetLastWidget();
		SharedWidgetPtr = WeakWidgetPtr.Pin();
	}

	return SharedWidgetPtr;
}

TArray<TSharedRef<SWidget>> FSlateApplication::MouseCaptorHelper::ToSharedWidgets() const
{
	TArray<TSharedRef<SWidget>> Widgets;
	for (auto IndexPathPair : PointerIndexToMouseCaptorWeakPathMap)
	{
		TSharedPtr<SWidget> LastWidget = IndexPathPair.Value.GetLastWidget().Pin();
		if (LastWidget.IsValid())
		{
			Widgets.Add(LastWidget.ToSharedRef());
		}
	}
	return Widgets;
}

TSharedPtr< SWidget > FSlateApplication::MouseCaptorHelper::ToSharedWindow(uint32 PointerIndex)
{
	// if the path is valid then we can get the window the current mouse captor belongs to
	FWidgetPath MouseCaptorPath = ToWidgetPath(PointerIndex);
	if ( MouseCaptorPath.IsValid() )
	{
		return MouseCaptorPath.GetWindow();
	}

	return TSharedPtr< SWidget >();
}

void FSlateApplication::MouseCaptorHelper::SetMouseCaptor(uint32 PointerIndex, const FWidgetPath& EventPath, TSharedPtr< SWidget > Widget)
{
	// Caller is trying to set a new mouse captor, so invalidate the current one - when the function finishes
	// it still may not have a valid captor widget, this is ok
	InvalidateCaptureForPointer(PointerIndex);

	if ( Widget.IsValid() )
	{
		TSharedRef< SWidget > WidgetRef = Widget.ToSharedRef();
		FWidgetPath NewMouseCaptorPath = EventPath.GetPathDownTo( WidgetRef );

		const auto IsPathToCaptorFound = []( const FWidgetPath& PathToTest, const TSharedRef<SWidget>& WidgetToFind )
		{
			return PathToTest.Widgets.Num() > 0 && PathToTest.Widgets.Last().Widget == WidgetToFind;
		};

		FWeakWidgetPath MouseCaptorWeakPath;
		if ( IsPathToCaptorFound( NewMouseCaptorPath, WidgetRef ) )
		{
			MouseCaptorWeakPath = NewMouseCaptorPath;
		}
		else if (EventPath.Widgets.Num() > 0)
		{
			// If the target widget wasn't found on the event path then start the search from the root
			NewMouseCaptorPath = EventPath.GetPathDownTo( EventPath.Widgets[0].Widget );
			NewMouseCaptorPath.ExtendPathTo( FWidgetMatcher( WidgetRef ) );
			
			MouseCaptorWeakPath = IsPathToCaptorFound( NewMouseCaptorPath, WidgetRef )
				? NewMouseCaptorPath
				: FWeakWidgetPath();
		}
		else
		{
			ensureMsgf(EventPath.Widgets.Num() > 0, TEXT("An unknown widget is attempting to set capture to %s"), *Widget->ToString() );
		}

		if (MouseCaptorWeakPath.IsValid())
		{
			PointerIndexToMouseCaptorWeakPathMap.Add(PointerIndex, MouseCaptorWeakPath);
		}
	}
}

void FSlateApplication::MouseCaptorHelper::InvalidateCaptureForAllPointers()
{
	TArray<uint32> PointerIndices;
	PointerIndexToMouseCaptorWeakPathMap.GenerateKeyArray(PointerIndices);
	for (auto Index : PointerIndices)
	{
		InvalidateCaptureForPointer(Index);
	}
}

FWidgetPath FSlateApplication::MouseCaptorHelper::ToWidgetPath( FWeakWidgetPath::EInterruptedPathHandling::Type InterruptedPathHandling, const FPointerEvent* PointerEvent )
{
	FWidgetPath WidgetPath;
	const FWeakWidgetPath* MouseCaptorWeakPath = PointerIndexToMouseCaptorWeakPathMap.Find(PointerEvent->GetPointerIndex());
	if ( MouseCaptorWeakPath->IsValid() )
	{
		if ( MouseCaptorWeakPath->ToWidgetPath( WidgetPath, InterruptedPathHandling, PointerEvent ) == FWeakWidgetPath::EPathResolutionResult::Truncated )
		{
			// If the path was truncated then it means this widget is no longer part of the active set,
			// so we make sure to invalidate its capture
			InvalidateCaptureForPointer(PointerEvent->GetPointerIndex());
		}
	}

	return WidgetPath;
}

FWidgetPath FSlateApplication::MouseCaptorHelper::ToWidgetPath(uint32 PointerIndex, FWeakWidgetPath::EInterruptedPathHandling::Type InterruptedPathHandling)
{
	FWidgetPath WidgetPath;
	const FWeakWidgetPath* MouseCaptorWeakPath = PointerIndexToMouseCaptorWeakPathMap.Find(PointerIndex);
	if (MouseCaptorWeakPath && MouseCaptorWeakPath->IsValid() )
	{
		if ( MouseCaptorWeakPath->ToWidgetPath( WidgetPath, InterruptedPathHandling ) == FWeakWidgetPath::EPathResolutionResult::Truncated )
		{
			// If the path was truncated then it means this widget is no longer part of the active set,
			// so we make sure to invalidate its capture
			InvalidateCaptureForPointer(PointerIndex);
		}
	}

	return WidgetPath;
}

void FSlateApplication::MouseCaptorHelper::InvalidateCaptureForPointer(uint32 PointerIndex)
{
	InformCurrentCaptorOfCaptureLoss(PointerIndex);
	PointerIndexToMouseCaptorWeakPathMap.Remove(PointerIndex);
}

TArray<FWidgetPath> FSlateApplication::MouseCaptorHelper::ToWidgetPaths()
{
	TArray<FWidgetPath> WidgetPaths;
	TArray<uint32> PointerIndices;
	PointerIndexToMouseCaptorWeakPathMap.GenerateKeyArray(PointerIndices);
	for (auto Index : PointerIndices)
	{
		WidgetPaths.Add(ToWidgetPath(Index));
	}
	return WidgetPaths;
}

FWeakWidgetPath FSlateApplication::MouseCaptorHelper::ToWeakPath(uint32 PointerIndex) const
{
	const FWeakWidgetPath* MouseCaptorWeakPath = PointerIndexToMouseCaptorWeakPathMap.Find(PointerIndex);
	if (MouseCaptorWeakPath)
	{
		return *MouseCaptorWeakPath;
	}
	return FWeakWidgetPath();
}

void FSlateApplication::MouseCaptorHelper::InformCurrentCaptorOfCaptureLoss(uint32 PointerIndex) const
{
	// if we have a path to a widget then it is the current mouse captor and needs to know it has lost capture
	const FWeakWidgetPath* MouseCaptorWeakPath = PointerIndexToMouseCaptorWeakPathMap.Find(PointerIndex);
	if (MouseCaptorWeakPath && MouseCaptorWeakPath->IsValid() )
	{
		TWeakPtr< SWidget > WeakWidgetPtr = MouseCaptorWeakPath->GetLastWidget();
		TSharedPtr< SWidget > SharedWidgetPtr = WeakWidgetPtr.Pin();
		if ( SharedWidgetPtr.IsValid() )
		{
			SharedWidgetPtr->OnMouseCaptureLost();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
FDelegateHandle FPopupSupport::RegisterClickNotification( const TSharedRef<SWidget>& NotifyWhenClickedOutsideMe, const FOnClickedOutside& InNotification )
{
	// If the subscriber or a zone object is destroyed, the subscription is
	// no longer active. Clean it up here so that consumers of this API have an
	// easy time with resource management.
	struct { void operator()( TArray<FClickSubscriber>& Notifications ) {
		for ( int32 SubscriberIndex=0; SubscriberIndex < Notifications.Num(); )
		{
			if ( !Notifications[SubscriberIndex].ShouldKeep() )
			{
				Notifications.RemoveAtSwap(SubscriberIndex);
			}
			else
			{
				SubscriberIndex++;
			}		
		}
	}} ClearOutStaleNotifications;
	
	ClearOutStaleNotifications( ClickZoneNotifications );

	// Add a new notification.
	ClickZoneNotifications.Add( FClickSubscriber( NotifyWhenClickedOutsideMe, InNotification ) );

	return ClickZoneNotifications.Last().Notification.GetHandle();
}

void FPopupSupport::UnregisterClickNotification( const FOnClickedOutside& InNotification )
{
	for (int32 SubscriptionIndex=0; SubscriptionIndex < ClickZoneNotifications.Num();)
	{
		if (ClickZoneNotifications[SubscriptionIndex].Notification.DEPRECATED_Compare(InNotification))
		{
			ClickZoneNotifications.RemoveAtSwap(SubscriptionIndex);
		}
		else
		{
			SubscriptionIndex++;
		}
	}	
}

void FPopupSupport::UnregisterClickNotification( FDelegateHandle Handle )
{
	for (int32 SubscriptionIndex=0; SubscriptionIndex < ClickZoneNotifications.Num();)
	{
		if (ClickZoneNotifications[SubscriptionIndex].Notification.GetHandle() == Handle)
		{
			ClickZoneNotifications.RemoveAtSwap(SubscriptionIndex);
		}
		else
		{
			SubscriptionIndex++;
		}
	}	
}

void FPopupSupport::SendNotifications( const FWidgetPath& WidgetsUnderCursor )
{
	struct FArrangedWidgetMatcher
	{
		FArrangedWidgetMatcher( const TSharedRef<SWidget>& InWidgetToMatch )
		: WidgetToMatch( InWidgetToMatch )
		{}

		bool operator()(const FArrangedWidget& Candidate) const
		{
			return WidgetToMatch == Candidate.Widget;
		}

		const TSharedRef<SWidget>& WidgetToMatch;
	};

	// For each subscription, if the widget in question is not being clicked, send the notification.
	// i.e. Notifications are saying "some widget outside you was clicked".
	for (int32 SubscriberIndex=0; SubscriberIndex < ClickZoneNotifications.Num(); ++SubscriberIndex)
	{
		FClickSubscriber& Subscriber = ClickZoneNotifications[SubscriberIndex];
		if (Subscriber.DetectClicksOutsideMe.IsValid())
		{
			// Did we click outside the region in this subscription? If so send the notification.
			FArrangedWidgetMatcher Matcher(Subscriber.DetectClicksOutsideMe.Pin().ToSharedRef());
			const bool bClickedOutsideOfWidget = WidgetsUnderCursor.Widgets.GetInternalArray().IndexOfByPredicate(Matcher) == INDEX_NONE;
			if ( bClickedOutsideOfWidget )
			{
				Subscriber.Notification.ExecuteIfBound();
			}
		}
	}
}

void FSlateApplication::Create()
{
	Create(MakeShareable(FPlatformMisc::CreateApplication()));
}


TSharedRef<FSlateApplication> FSlateApplication::Create(const TSharedRef<class GenericApplication>& InPlatformApplication)
{
	EKeys::Initialize();

	FCoreStyle::ResetToDefault();

	CurrentApplication = MakeShareable( new FSlateApplication() );
	CurrentBaseApplication = CurrentApplication;

	PlatformApplication = InPlatformApplication;
	PlatformApplication->SetMessageHandler( CurrentApplication.ToSharedRef() );

	// The grid needs to know the size and coordinate system of the desktop.
	// Some monitor setups have a primary monitor on the right and below the
	// left one, so the leftmost upper right monitor can be something like (-1280, -200)
	{
		// Get an initial value for the VirtualDesktop geometry
		CurrentApplication->VirtualDesktopRect = []()
		{
			FDisplayMetrics DisplayMetrics;
			FSlateApplicationBase::Get().GetDisplayMetrics(DisplayMetrics);
			const FPlatformRect& VirtualDisplayRect = DisplayMetrics.VirtualDisplayRect;
			return FSlateRect(VirtualDisplayRect.Left, VirtualDisplayRect.Top, VirtualDisplayRect.Right, VirtualDisplayRect.Bottom);
		}();

		// Sign up for updates from the OS. Polling this every frame is too expensive on at least some OSs.
		PlatformApplication->OnDisplayMetricsChanged().AddSP(CurrentApplication.ToSharedRef(), &FSlateApplication::OnVirtualDesktopSizeChanged);
	}

	return CurrentApplication.ToSharedRef();
}

void FSlateApplication::Shutdown()
{
	if (FSlateApplication::IsInitialized())
	{
		CurrentApplication->OnShutdown();
		CurrentApplication->DestroyRenderer();
		CurrentApplication->Renderer.Reset();
		PlatformApplication->DestroyApplication();
		PlatformApplication.Reset();
		CurrentApplication.Reset();
		CurrentBaseApplication.Reset();
	}
}

TSharedPtr<FSlateApplication> FSlateApplication::CurrentApplication = nullptr;

FSlateApplication::FSlateApplication()
	: SynthesizeMouseMovePending(0)
	, bAppIsActive(true)
	, bSlateWindowActive(true)
	, Scale( 1.0f )
	, DragTriggerDistance( 5.0f )
	, CursorRadius( 0.0f )
	, LastUserInteractionTime( 0.0 )
	, LastUserInteractionTimeForThrottling( 0.0 )
	, LastMouseMoveTime( 0.0 )
	, SlateSoundDevice( MakeShareable(new FNullSlateSoundDevice()) )
	, CurrentTime( FPlatformTime::Seconds() )
	, LastTickTime( 0.0 )
	, AverageDeltaTime( 1.0f / 30.0f )	// Prime the running average with a typical frame rate so it doesn't have to spin up from zero
	, AverageDeltaTimeForResponsiveness( 1.0f / 30.0f )
	, OnExitRequested()
	, EventLogger( TSharedPtr<IEventLogger>() )
	, NumExternalModalWindowsActive( 0 )
	, bAllowToolTips( true )
	, ToolTipDelay( 0.15f )
	, ToolTipFadeInDuration( 0.1f )
	, ToolTipSummonTime( 0.0 )
	, DesiredToolTipLocation( FVector2D::ZeroVector )
	, ToolTipOffsetDirection( EToolTipOffsetDirection::Undetermined )
	, bRequestLeaveDebugMode( false )
	, bLeaveDebugForSingleStep( false )
	, CVarAllowToolTips(
		TEXT( "Slate.AllowToolTips" ),
		bAllowToolTips,
		TEXT( "Whether to allow tool-tips to spawn at all." ) )
	, CVarToolTipDelay(
		TEXT( "Slate.ToolTipDelay" ),
		ToolTipDelay,
		TEXT( "Delay in seconds before a tool-tip is displayed near the mouse cursor when hovering over widgets that supply tool-tip data." ) )
	, CVarToolTipFadeInDuration(
		TEXT( "Slate.ToolTipFadeInDuration" ),
		ToolTipFadeInDuration,
		TEXT( "How long it takes for a tool-tip to fade in, in seconds." ) )
	, bIsExternalUIOpened( false )
	, SlateTextField( nullptr )
	, bIsFakingTouch(FParse::Param(FCommandLine::Get(), TEXT("simmobile")) || FParse::Param(FCommandLine::Get(), TEXT("faketouches")))
	, bIsGameFakingTouch( false )
	, bIsFakingTouched( false )
	, bTouchFallbackToMouse( true )
	, bSoftwareCursorAvailable( false )	
	, bMenuAnimationsEnabled( true )
	, AppIcon( FCoreStyle::Get().GetBrush("DefaultAppIcon") )
	, VirtualDesktopRect( 0,0,0,0 )
{
#if WITH_UNREAL_DEVELOPER_TOOLS
	FModuleManager::Get().LoadModule(TEXT("Settings"));
#endif	

	if (GConfig)
	{	
		GConfig->GetBool(TEXT("MobileSlateUI"),TEXT("bTouchFallbackToMouse"),bTouchFallbackToMouse,GEngineIni);
		GConfig->GetBool(TEXT("CursorControl"), TEXT("bAllowSoftwareCursor"), bSoftwareCursorAvailable, GEngineIni);
	}


	// causes InputCore to initialize, even if statically linked
	FInputCoreModule& InputCore = FModuleManager::LoadModuleChecked<FInputCoreModule>(TEXT("InputCore"));

	FGenericCommands::Register();
	FTabCommands::Register();

	NormalExecutionGetter.BindRaw( this, &FSlateApplication::IsNormalExecution );
	PointerIndexLastPositionMap.Add(CursorPointerIndex, FVector2D::ZeroVector);
}

FSlateApplication::~FSlateApplication()
{
	FTabCommands::Unregister();
	FGenericCommands::Unregister();
	
	if (SlateTextField != nullptr)
	{
		delete SlateTextField;
		SlateTextField = nullptr;
	}
}

const FStyleNode* FSlateApplication::GetRootStyle() const
{
	return RootStyleNode;
}

void FSlateApplication::InitializeRenderer( TSharedRef<FSlateRenderer> InRenderer )
{
	Renderer = InRenderer;
	Renderer->Initialize();
}

void FSlateApplication::InitializeSound( const TSharedRef<ISlateSoundDevice>& InSlateSoundDevice )
{
	SlateSoundDevice = InSlateSoundDevice;
}

void FSlateApplication::DestroyRenderer()
{
	if( Renderer.IsValid() )
	{
		Renderer->Destroy();
	}
}

/**
 * Called when the user closes the outermost frame (ie quitting the app). Uses standard UE4 global variable
 * so normal UE4 applications work as expected
 */
static void OnRequestExit()
{
	GIsRequestingExit = true;
}

void FSlateApplication::PlaySound( const FSlateSound& SoundToPlay, int32 UserIndex ) const
{
	SlateSoundDevice->PlaySound(SoundToPlay, UserIndex);
}

float FSlateApplication::GetSoundDuration(const FSlateSound& Sound) const
	{
	return SlateSoundDevice->GetSoundDuration(Sound);
}

FVector2D FSlateApplication::GetCursorPos() const
{
	if ( PlatformApplication->Cursor.IsValid() )
	{
		return PlatformApplication->Cursor->GetPosition();
	}

	return FVector2D( 0, 0 );
}

FVector2D FSlateApplication::GetLastCursorPos() const
{
	return PointerIndexLastPositionMap[CursorPointerIndex];
}

void FSlateApplication::SetCursorPos( const FVector2D& MouseCoordinate )
{
	if ( PlatformApplication->Cursor.IsValid() )
	{
		return PlatformApplication->Cursor->SetPosition( MouseCoordinate.X, MouseCoordinate.Y );
	}
}

FWidgetPath FSlateApplication::LocateWindowUnderMouse( FVector2D ScreenspaceMouseCoordinate, const TArray< TSharedRef< SWindow > >& Windows, bool bIgnoreEnabledStatus )
{
	bool bPrevWindowWasModal = false;
	FArrangedChildren OutWidgetPath(EVisibility::Visible);

	for (int32 WindowIndex = Windows.Num() - 1; WindowIndex >= 0 && OutWidgetPath.Num() == 0; --WindowIndex)
	{ 
		const TSharedRef<SWindow>& Window = Windows[WindowIndex];

		// Hittest the window's children first.
		FWidgetPath ResultingPath = LocateWindowUnderMouse(ScreenspaceMouseCoordinate, Window->GetChildWindows(), bIgnoreEnabledStatus);
		if (ResultingPath.IsValid())
		{
			return ResultingPath;
		}

		// If none of the children were hit, hittest the parent.

		// Only accept input if the current window accepts input and the current window is not under a modal window or an interactive tooltip

		if (!bPrevWindowWasModal)
		{
			FWidgetPath PathToLocatedWidget = LocateWidgetInWindow(ScreenspaceMouseCoordinate, Window, bIgnoreEnabledStatus);
			if (PathToLocatedWidget.IsValid())
			{
				return PathToLocatedWidget;
			}
		}
	}
	
	return FWidgetPath();
}

bool FSlateApplication::IsWindowHousingInteractiveTooltip(const TSharedRef<const SWindow>& WindowToTest) const
{
	const TSharedPtr<IToolTip> ActiveToolTipPtr = ActiveToolTip.Pin();
	const TSharedPtr<SWindow> ToolTipWindowPtr = ToolTipWindow.Pin();
	const bool bIsHousingInteractiveTooltip =
		WindowToTest == ToolTipWindowPtr &&
		ActiveToolTipPtr.IsValid() &&
		ActiveToolTipPtr->IsInteractive();

	return bIsHousingInteractiveTooltip;
}

void FSlateApplication::TickWindowAndChildren( TSharedRef<SWindow> WindowToTick )
{
	SLATE_CYCLE_COUNTER_SCOPE_CUSTOM(GTickWindowAndChildrenTime, WindowToTick->GetCreatedInLocation());
	if ( WindowToTick->IsVisible() && !WindowToTick->IsWindowMinimized() )
	{
		// Switch to the appropriate world for ticking
		FScopedSwitchWorldHack SwitchWorld( WindowToTick );

		// Measure all the widgets before we tick, and update their DesiredSize.  This is
		// needed so that Tick() can call ArrangeChildren(), then pass valid widget metrics into
		// the Tick() function.
		
		{
			SCOPE_CYCLE_COUNTER( STAT_SlatePrepass );
			WindowToTick->SlatePrepass(GetApplicationScale());
		}

		if (WindowToTick->IsAutosized())
		{
			WindowToTick->Resize(WindowToTick->GetDesiredSize());
		}

		{
			SCOPE_CYCLE_COUNTER( STAT_SlateTickWidgets );
			SLATE_CYCLE_COUNTER_SCOPE(GTickWidgets);
			// Tick this window and all of the widgets in this window
			WindowToTick->TickWidgetsRecursively( WindowToTick->GetWindowGeometryInScreen(), GetCurrentTime(), GetDeltaTime() );
		}

		// Tick all of this window's child windows.
		const TArray< TSharedRef<SWindow> >& WindowChildren = WindowToTick->GetChildWindows();
		for ( int32 ChildIndex=0; ChildIndex < WindowChildren.Num(); ++ChildIndex )
		{
			TickWindowAndChildren( WindowChildren[ChildIndex] );
		}
	}
}

void FSlateApplication::DrawWindows()
{
	SLATE_CYCLE_COUNTER_SCOPE(GSlateDrawWindows);
	PrivateDrawWindows();
}

struct FDrawWindowArgs
{
	FDrawWindowArgs( FSlateDrawBuffer& InDrawBuffer, const FWidgetPath& InWidgetsUnderCursor )
		: OutDrawBuffer( InDrawBuffer )
		, WidgetsUnderCursor( InWidgetsUnderCursor )
	{}

	FSlateDrawBuffer& OutDrawBuffer;
	const FWidgetPath& WidgetsUnderCursor;
};


void FSlateApplication::DrawWindowAndChildren( const TSharedRef<SWindow>& WindowToDraw, FDrawWindowArgs& DrawWindowArgs )
{
	SLATE_CYCLE_COUNTER_SCOPE_CUSTOM(GSlateDrawWindowAndChildren, WindowToDraw->GetCreatedInLocation());
	// Only draw visible windows
	if( WindowToDraw->IsVisible() && !WindowToDraw->IsWindowMinimized() )
	{
		// Switch to the appropriate world for drawing
		FScopedSwitchWorldHack SwitchWorld( WindowToDraw );

		FSlateWindowElementList& WindowElementList = DrawWindowArgs.OutDrawBuffer.AddWindowElementList( WindowToDraw );

		// Drawing is done in window space, so null out the positions and keep the size.
		FGeometry WindowGeometry = WindowToDraw->GetWindowGeometryInWindow();
		int32 MaxLayerId = 0;
		{
			WindowToDraw->GetHittestGrid()->ClearGridForNewFrame( VirtualDesktopRect );
			MaxLayerId = WindowToDraw->PaintWindow(
				FPaintArgs(WindowToDraw, *WindowToDraw->GetHittestGrid(), WindowToDraw->GetPositionInScreen(), GetCurrentTime(), GetDeltaTime()),
				WindowGeometry, WindowToDraw->GetClippingRectangleInWindow(),
				WindowElementList,
				0,
				FWidgetStyle(),
				WindowToDraw->IsEnabled() );

			// Draw drag drop operation if it's windowless.
			if ( IsDragDropping() && DragDropContent->IsWindowlessOperation() )
			{
				TSharedPtr<SWindow> DragDropWindow = DragDropWindowPtr.Pin();
				if ( DragDropWindow.IsValid() && DragDropWindow == WindowToDraw )
				{
					TSharedPtr<SWidget> DecoratorWidget = DragDropContent->GetDefaultDecorator();
					if ( DecoratorWidget.IsValid() && DecoratorWidget->GetVisibility().IsVisible() )
					{
						DecoratorWidget->SetVisibility(EVisibility::HitTestInvisible);
						DecoratorWidget->SlatePrepass();

						FVector2D DragDropContentInWindowSpace = WindowToDraw->GetWindowGeometryInScreen().AbsoluteToLocal(DragDropContent->GetDecoratorPosition());
						const FGeometry DragDropContentGeometry = FGeometry::MakeRoot(DecoratorWidget->GetDesiredSize(), FSlateLayoutTransform(DragDropContentInWindowSpace));

						DecoratorWidget->Paint(
							FPaintArgs(WindowToDraw, *WindowToDraw->GetHittestGrid(), WindowToDraw->GetPositionInScreen(), GetCurrentTime(), GetDeltaTime()),
							DragDropContentGeometry, WindowToDraw->GetClippingRectangleInWindow(),
							WindowElementList,
							++MaxLayerId,
							FWidgetStyle(),
							WindowToDraw->IsEnabled());
					}
				}
			}

			// Draw Software Cursor
			TSharedPtr<SWindow> CursorWindow = CursorWindowPtr.Pin();
			if (CursorWindow.IsValid() && WindowToDraw == CursorWindow)
			{
				TSharedPtr<SWidget> CursorWidget = CursorWidgetPtr.Pin();
				
				if (CursorWidget.IsValid())
				{
					CursorWidget->SlatePrepass(GetApplicationScale());

					FVector2D CursorPosInWindowSpace = WindowToDraw->GetWindowGeometryInScreen().AbsoluteToLocal(GetCursorPos());
					CursorPosInWindowSpace += (CursorWidget->GetDesiredSize() * -0.5);
					const FGeometry CursorGeometry = FGeometry::MakeRoot(CursorWidget->GetDesiredSize(), FSlateLayoutTransform(CursorPosInWindowSpace));

					CursorWidget->Paint(
						FPaintArgs(WindowToDraw, *WindowToDraw->GetHittestGrid(), WindowToDraw->GetPositionInScreen(), GetCurrentTime(), GetDeltaTime()),
						CursorGeometry, WindowToDraw->GetClippingRectangleInWindow(),
						WindowElementList,
						++MaxLayerId,
						FWidgetStyle(),
						WindowToDraw->IsEnabled());
				}
			}
		}

		// The widget reflector may want to paint some additional stuff as part of the Widget introspection that it performs.
		// For example: it may draw layout rectangles for hovered widgets.
		const bool bVisualizeLayoutUnderCursor = DrawWindowArgs.WidgetsUnderCursor.IsValid();
		const bool bCapturingFromThisWindow = bVisualizeLayoutUnderCursor && DrawWindowArgs.WidgetsUnderCursor.TopLevelWindow == WindowToDraw;
		TSharedPtr<IWidgetReflector> WidgetReflector = WidgetReflectorPtr.Pin();
		if ( bCapturingFromThisWindow || (WidgetReflector.IsValid() && WidgetReflector->ReflectorNeedsToDrawIn(WindowToDraw)) )
		{
			MaxLayerId = WidgetReflector->Visualize( DrawWindowArgs.WidgetsUnderCursor, WindowElementList, MaxLayerId );
		}

		// Visualize pointer presses and pressed keys for demo-recording purposes.
		const bool bVisualiseMouseClicks = WidgetReflector.IsValid() && PlatformApplication->Cursor.IsValid() && PlatformApplication->Cursor->GetType() != EMouseCursor::None;
		if (bVisualiseMouseClicks )
		{
			MaxLayerId = WidgetReflector->VisualizeCursorAndKeys( WindowElementList, MaxLayerId );
		}

		// Draw the child windows
		const TArray< TSharedRef<SWindow> >& WindowChildren = WindowToDraw->GetChildWindows();
		for (int32 ChildIndex=0; ChildIndex < WindowChildren.Num(); ++ChildIndex)
		{
			DrawWindowAndChildren( WindowChildren[ChildIndex], DrawWindowArgs );
		}
	}
}

static void PrepassWindowAndChildren( TSharedRef<SWindow> WindowToPrepass )
{
	SLATE_CYCLE_COUNTER_SCOPE_CUSTOM(GSlatePrepassWindowAndChildren, WindowToPrepass->GetCreatedInLocation());
	FScopedSwitchWorldHack SwitchWorld( WindowToPrepass );

	{
		SCOPE_CYCLE_COUNTER(STAT_SlatePrepass);
		WindowToPrepass->SlatePrepass(FSlateApplication::Get().GetApplicationScale());
	}

	if (WindowToPrepass->IsAutosized())
	{
		WindowToPrepass->Resize(WindowToPrepass->GetDesiredSize());
	}

	for (const TSharedRef<SWindow>& ChildWindow : WindowToPrepass->GetChildWindows() )
	{
		PrepassWindowAndChildren(ChildWindow);
	}
}

void FSlateApplication::DrawPrepass( TSharedPtr<SWindow> DrawOnlyThisWindow )
{
	SLATE_CYCLE_COUNTER_SCOPE(GSlateDrawPrepass);
	TSharedPtr<SWindow> ActiveModalWindow = GetActiveModalWindow();

	if (ActiveModalWindow.IsValid())
	{
		PrepassWindowAndChildren( ActiveModalWindow.ToSharedRef() );

		for (TArray< TSharedRef<SWindow> >::TConstIterator CurrentWindowIt(SlateWindows); CurrentWindowIt; ++CurrentWindowIt)
		{
			const TSharedRef<SWindow>& CurrentWindow = *CurrentWindowIt;
			if (CurrentWindow->IsTopmostWindow())
			{
				PrepassWindowAndChildren( CurrentWindow );
			}
		}

		TArray< TSharedRef<SWindow> > NotificationWindows;
		FSlateNotificationManager::Get().GetWindows(NotificationWindows);
		for (auto CurrentWindowIt(NotificationWindows.CreateIterator()); CurrentWindowIt; ++CurrentWindowIt)
		{
			PrepassWindowAndChildren(*CurrentWindowIt );
		}
	}
	else if (DrawOnlyThisWindow.IsValid())
	{
		PrepassWindowAndChildren( DrawOnlyThisWindow.ToSharedRef() );
	}
	else
	{
		// Draw all windows
		for (const TSharedRef<SWindow>& CurrentWindow : SlateWindows)
		{
			if (CurrentWindow->IsVisible() && !CurrentWindow->IsWindowMinimized())
			{
				PrepassWindowAndChildren(CurrentWindow );
			}
		}
	}
}


TArray<TSharedRef<SWindow>> GatherAllDescendants(const TArray< TSharedRef<SWindow> >& InWindowList)
{
	TArray<TSharedRef<SWindow>> GatheredDescendants(InWindowList);

	for (const TSharedRef<SWindow>& SomeWindow : InWindowList)
	{
		GatheredDescendants.Append( GatherAllDescendants( SomeWindow->GetChildWindows() ) );
	}
	
	return GatheredDescendants;
}

void FSlateApplication::PrivateDrawWindows( TSharedPtr<SWindow> DrawOnlyThisWindow )
{
	check(Renderer.IsValid());

	// Is user expecting visual feedback from the Widget Reflector?
	const bool bVisualizeLayoutUnderCursor = WidgetReflectorPtr.IsValid() && WidgetReflectorPtr.Pin()->IsVisualizingLayoutUnderCursor();

	FWidgetPath WidgetsUnderCursor = bVisualizeLayoutUnderCursor
		? WidgetsUnderCursorLastEvent.ToWidgetPath()
		: FWidgetPath();

	if ( !SkipSecondPrepass.GetValueOnGameThread() )
	{
		DrawPrepass( DrawOnlyThisWindow );
	}

	FDrawWindowArgs DrawWindowArgs( Renderer->GetDrawBuffer(), WidgetsUnderCursor );

	{
		SCOPE_CYCLE_COUNTER( STAT_SlateDrawWindowTime );

		TSharedPtr<SWindow> ActiveModalWindow = GetActiveModalWindow(); 

		if (ActiveModalWindow.IsValid())
		{
			DrawWindowAndChildren( ActiveModalWindow.ToSharedRef(), DrawWindowArgs );

			for( TArray< TSharedRef<SWindow> >::TConstIterator CurrentWindowIt( SlateWindows ); CurrentWindowIt; ++CurrentWindowIt )
			{
				const TSharedRef<SWindow>& CurrentWindow = *CurrentWindowIt;
				if ( CurrentWindow->IsTopmostWindow() )
				{
					DrawWindowAndChildren(CurrentWindow, DrawWindowArgs);
				}
			}

			TArray< TSharedRef<SWindow> > NotificationWindows;
			FSlateNotificationManager::Get().GetWindows(NotificationWindows);
			for( auto CurrentWindowIt( NotificationWindows.CreateIterator() ); CurrentWindowIt; ++CurrentWindowIt )
			{
				DrawWindowAndChildren(*CurrentWindowIt, DrawWindowArgs);
			}	
		}
		else if( DrawOnlyThisWindow.IsValid() )
		{
			DrawWindowAndChildren( DrawOnlyThisWindow.ToSharedRef(), DrawWindowArgs );
		}
		else
		{
			// Draw all windows
			for( TArray< TSharedRef<SWindow> >::TConstIterator CurrentWindowIt( SlateWindows ); CurrentWindowIt; ++CurrentWindowIt )
			{
				TSharedRef<SWindow> CurrentWindow = *CurrentWindowIt;
				if ( CurrentWindow->IsVisible() )
				{
					DrawWindowAndChildren( CurrentWindow, DrawWindowArgs );
				}
			}
		}
	}


	// Some windows may have been destroyed/removed.
	// Do not attempt to draw any windows that have been removed.
	TArray<TSharedRef<SWindow>> AllWindows = GatherAllDescendants(SlateWindows);
	DrawWindowArgs.OutDrawBuffer.GetWindowElementLists().RemoveAll([&]( FSlateWindowElementList& Candidate )
	{
		TSharedPtr<SWindow> CandidateWindow = Candidate.GetWindow();
		return !CandidateWindow.IsValid() || !AllWindows.Contains(CandidateWindow.ToSharedRef());
	});

	{	
		SLATE_CYCLE_COUNTER_SCOPE(GSlateRendererDrawWindows);
		Renderer->DrawWindows( DrawWindowArgs.OutDrawBuffer );
	}
}

void FSlateApplication::PollGameDeviceState()
{
	if( ActiveModalWindows.Num() == 0 && !GIntraFrameDebuggingGameThread )
	{
		// Don't poll when a modal window open or intra frame debugging is happening
		PlatformApplication->PollGameDeviceState( GetDeltaTime() );
	}
}

void FSlateApplication::FinishedInputThisFrame()
{
	const float DeltaTime = GetDeltaTime();

	if (InputPreProcessor.IsValid() && PlatformApplication->Cursor.IsValid())
	{
		InputPreProcessor->Tick(DeltaTime, *this, PlatformApplication->Cursor.ToSharedRef());
	}

	// All the input events have been processed.

	// Any widgets that may have received pointer input events
	// are given a chance to process accumulated values.
	if (MouseCaptor.HasCapture())
	{
		TArray<TSharedRef<SWidget>> Captors = MouseCaptor.ToSharedWidgets();
		for (const auto & Captor : Captors )
		{
			Captor->OnFinishedPointerInput();
		}
	}
	else
	{
		for ( const TWeakPtr<SWidget>& WidgetPtr : WidgetsUnderCursorLastEvent.Widgets )
		{
			const TSharedPtr<SWidget>& Widget = WidgetPtr.Pin();
			if (Widget.IsValid())
			{
				Widget->OnFinishedPointerInput();
			}
			else
			{
				break;
			}
		}
	}

	// Any widgets that may have recieved key events
	// are given a chance to process accumulated values.
	for (int32 SlateUserIndex = 0; SlateUserIndex < SlateApplicationDefs::MaxUsers; ++SlateUserIndex)
	{
		const FUserFocusEntry& UserFocusEntry = UserFocusEntries[SlateUserIndex];
		for (const TWeakPtr<SWidget>& WidgetPtr : UserFocusEntry.WidgetPath.Widgets)
		{
			const TSharedPtr<SWidget>& Widget = WidgetPtr.Pin();
			if (Widget.IsValid())
			{
				Widget->OnFinishedKeyInput();
			}
			else
			{
				break;
			}
		}
	}
}


extern SLATECORE_API int32 bFoldTick;


void FSlateApplication::Tick()
{
	{
	SCOPE_CYCLE_COUNTER( STAT_SlateTickTime );
	SLATE_CYCLE_COUNTER_SCOPE(GSlateTotalTickTime);
	
	{
		const float DeltaTime = GetDeltaTime();

		SCOPE_CYCLE_COUNTER( STAT_SlateMessageTick );

		// We need to pump messages here so that slate can receive input.  
		if( (ActiveModalWindows.Num() > 0) || GIntraFrameDebuggingGameThread )
		{
			// We only need to pump messages for slate when a modal window or blocking mode is active is up because normally message pumping is handled in FEngineLoop::Tick
			PlatformApplication->PumpMessages( DeltaTime );

			if (FCoreDelegates::StarvedGameLoop.IsBound())
			{
				FCoreDelegates::StarvedGameLoop.Execute();
			}
		}

		PlatformApplication->Tick( DeltaTime );

		PlatformApplication->ProcessDeferredEvents( DeltaTime );
	}

	// The widget locking the cursor to its bounds may have been reshaped.
	// Check if the widget was reshaped and update the cursor lock
	// bounds if needed.
	UpdateCursorLockRegion();

	// When Slate captures the mouse, it is up to us to set the cursor 
	// because the OS assumes that we own the mouse.
	if (MouseCaptor.HasCapture())
	{
		QueryCursor();
	}

	{
		SCOPE_CYCLE_COUNTER( STAT_SlateUpdateTooltip );
		SLATE_CYCLE_COUNTER_SCOPE(GUpdateTooltipTime);

		// Update tool tip, if we have one
		const bool AllowSpawningOfToolTips = false;
		UpdateToolTip( AllowSpawningOfToolTips );
	}


	// Advance time
	LastTickTime = CurrentTime;
	CurrentTime = FPlatformTime::Seconds();

	// Update average time between ticks.  This is used to monitor how responsive the application "feels".
	// Note that we calculate this before we apply the max quantum clamping below, because we want to store
	// the actual frame rate, even if it is very low.
	{
		// Scalar percent of new delta time that contributes to running average.  Use a lower value to add more smoothing
		// to the average frame rate.  A value of 1.0 will disable smoothing.
		const float RunningAverageScale = 0.1f;

		AverageDeltaTime = AverageDeltaTime * ( 1.0f - RunningAverageScale ) + GetDeltaTime() * RunningAverageScale;

		// Don't update average delta time if we're in an exceptional situation, such as when throttling mode
		// is active, because the measured tick time will not be representative of the application's performance.
		// In these cases, the cached average delta time from before the throttle activated will be used until
		// throttling has finished.
		if( FSlateThrottleManager::Get().IsAllowingExpensiveTasks() )
		{
			// Clamp to avoid including huge hitchy frames in our average
			const float ClampedDeltaTime = FMath::Clamp( GetDeltaTime(), 0.0f, 1.0f );
			AverageDeltaTimeForResponsiveness = AverageDeltaTimeForResponsiveness * ( 1.0f - RunningAverageScale ) + ClampedDeltaTime * RunningAverageScale;
		}
	}

	// Handle large quantums
	const double MaxQuantumBeforeClamp = 1.0 / 8.0;		// 8 FPS
	if( GetDeltaTime() > MaxQuantumBeforeClamp )
	{
		LastTickTime = CurrentTime - MaxQuantumBeforeClamp;
	}

	const bool bNeedsSyntheticMouseMouse = SynthesizeMouseMovePending > 0;
	if (bNeedsSyntheticMouseMouse)
	{
		// Force a mouse move event to make sure all widgets know whether there is a mouse cursor hovering over them
		SynthesizeMouseMove();
		--SynthesizeMouseMovePending;
	}

	// Update auto-throttling based on elapsed time since user interaction
	ThrottleApplicationBasedOnMouseMovement();

	TSharedPtr<SWindow> ActiveModalWindow = GetActiveModalWindow();

	const float SleepThreshold = SleepBufferPostInput.GetValueOnGameThread();
	const double TimeSinceInput = LastTickTime - LastUserInteractionTime;
	const double TimeSinceMouseMove = LastTickTime - LastMouseMoveTime;
	
	const bool bIsUserIdle = (TimeSinceInput > SleepThreshold) && (TimeSinceMouseMove > SleepThreshold);
	const bool bAnyActiveTimersPending = AnyActiveTimersArePending();
	if (bAnyActiveTimersPending)
	{
		// Some UI might slide under the cursor. To a widget, this is
		// as if the cursor moved over it.
		QueueSynthesizedMouseMove();
	}

	// skip tick/draw if we are idle and there are no active timers registered that we need to drive slate for.
	// This effectively means the slate application is totally idle and we don't need to update the UI.
	// This relies on Widgets properly registering for Active timer when they need something to happen even
	// when the user is not providing any input (ie, animations, viewport rendering, async polling, etc).
	if (!AllowSlateToSleep.GetValueOnGameThread() || bAnyActiveTimersPending || !bIsUserIdle || bNeedsSyntheticMouseMouse)
	{
		if (!bFoldTick)
		{
			SCOPE_CYCLE_COUNTER(STAT_SlateTickWindowAndChildren);
			SLATE_CYCLE_COUNTER_SCOPE(GTickWindowAndChildrenTime);

			if ( ActiveModalWindow.IsValid() )
			{
				// There is a modal window, and we just need to tick it.
				TickWindowAndChildren(ActiveModalWindow.ToSharedRef());
				// And also tick any topmost windows (like tooltips, etc)
				for ( TArray< TSharedRef<SWindow> >::TIterator CurrentWindowIt(SlateWindows); CurrentWindowIt; ++CurrentWindowIt )
				{
					TSharedRef<SWindow>& CurrentWindow = *CurrentWindowIt;
					if ( CurrentWindow->IsTopmostWindow() )
					{
						TickWindowAndChildren(CurrentWindow);
					}
				}
				// also tick the notification manager's windows
				TArray< TSharedRef<SWindow> > NotificationWindows;
				FSlateNotificationManager::Get().GetWindows(NotificationWindows);
				for ( auto CurrentWindowIt(NotificationWindows.CreateIterator()); CurrentWindowIt; ++CurrentWindowIt )
				{
					TickWindowAndChildren(*CurrentWindowIt);
				}
			}
			else
			{
				// No modal window; tick all slate windows.
				for ( TArray< TSharedRef<SWindow> >::TIterator CurrentWindowIt(SlateWindows); CurrentWindowIt; ++CurrentWindowIt )
				{
					TSharedRef<SWindow>& CurrentWindow = *CurrentWindowIt;
					TickWindowAndChildren(CurrentWindow);
				}
			}
		}

		// Update any notifications - this needs to be done after windows have updated themselves 
		// (so they know their size)
		{
			SLATE_CYCLE_COUNTER_SCOPE(GSlateTickNotificationManager);
			FSlateNotificationManager::Get().Tick();
		}

		// Draw all windows
		DrawWindows();
	}
	}

	// Update Slate Stats
	SLATE_STATS_END_FRAME(GetCurrentTime());
}


void FSlateApplication::PumpMessages()
{
	PlatformApplication->PumpMessages( GetDeltaTime() );
}


void FSlateApplication::ThrottleApplicationBasedOnMouseMovement()
{
	bool bShouldThrottle = false;
	if( ThrottleWhenMouseIsMoving.GetValueOnGameThread() )	// Interpreted as bool here
	{
		// We only want to engage the throttle for a short amount of time after the mouse stops moving
		const float TimeToThrottleAfterMouseStops = 0.1f;

		// After a key or mouse button is pressed, we'll leave the throttle disengaged for awhile so the
		// user can use the keys to navigate in a viewport, for example.
		const float MinTimeSinceButtonPressToThrottle = 1.0f;

		// Use a small movement threshold to avoid engaging the throttle when the user bumps the mouse
		const float MinMouseMovePixelsBeforeThrottle = 2.0f;

		const FVector2D& CursorPos = GetCursorPos();
		static FVector2D LastCursorPos = GetCursorPos();
		//static double LastMouseMoveTime = FPlatformTime::Seconds();
		static bool bIsMouseMoving = false;
		if( CursorPos != LastCursorPos )
		{
			// Did the cursor move far enough that we care?
			if( bIsMouseMoving || ( CursorPos - LastCursorPos ).SizeSquared() >= MinMouseMovePixelsBeforeThrottle * MinMouseMovePixelsBeforeThrottle )
			{
				bIsMouseMoving = true;
				LastMouseMoveTime = this->GetCurrentTime();
				LastCursorPos = CursorPos;
			}
		}

		const float TimeSinceLastUserInteraction = CurrentTime - LastUserInteractionTimeForThrottling;
		const float TimeSinceLastMouseMove = CurrentTime - LastMouseMoveTime;
		if( TimeSinceLastMouseMove < TimeToThrottleAfterMouseStops )
		{
			// Only throttle if a Slate window is currently active.  If a Wx window (such as Matinee) is
			// being used, we don't want to throttle
			if( this->GetActiveTopLevelWindow().IsValid() )
			{
				// Only throttle if the user hasn't pressed a button in awhile
				if( TimeSinceLastUserInteraction > MinTimeSinceButtonPressToThrottle )
				{
					// If a widget has the mouse captured, then we won't bother throttling
					if( !MouseCaptor.HasCapture() )
					{
						// If there is no Slate window under the mouse, then we won't engage throttling
						if( LocateWindowUnderMouse( GetCursorPos(), GetInteractiveTopLevelWindows() ).IsValid() )
						{
							bShouldThrottle = true;
						}
					}
				}
			}
		}
		else
		{
			// Mouse hasn't moved in a bit, so reset our movement state
			bIsMouseMoving = false;
			LastCursorPos = CursorPos;
		}
	}

	if( bShouldThrottle )
	{
		if( !UserInteractionResponsivnessThrottle.IsValid() )
		{
			// Engage throttling
			UserInteractionResponsivnessThrottle = FSlateThrottleManager::Get().EnterResponsiveMode();
		}
	}
	else
	{
		if( UserInteractionResponsivnessThrottle.IsValid() )
		{
			// Disengage throttling
			FSlateThrottleManager::Get().LeaveResponsiveMode( UserInteractionResponsivnessThrottle );
		}
	}
}

FWidgetPath FSlateApplication::LocateWidgetInWindow(FVector2D ScreenspaceMouseCoordinate, const TSharedRef<SWindow>& Window, bool bIgnoreEnabledStatus) const
{
	const bool bAcceptsInput = Window->IsVisible() && (Window->AcceptsInput() || IsWindowHousingInteractiveTooltip(Window));
	if (bAcceptsInput && Window->IsScreenspaceMouseWithin(ScreenspaceMouseCoordinate))
	{
		const TArray<FWidgetAndPointer> WidgetsAndCursors = Window->GetHittestGrid()->GetBubblePath(ScreenspaceMouseCoordinate, GetCursorRadius(), bIgnoreEnabledStatus);
		return FWidgetPath(WidgetsAndCursors);
	}
	else
	{
		return FWidgetPath();
	}
}


TSharedRef<SWindow> FSlateApplication::AddWindow( TSharedRef<SWindow> InSlateWindow, const bool bShowImmediately )
{
	// Add the Slate window to the Slate application's top-level window array.  Note that neither the Slate window
	// or the native window are ready to be used yet, however we need to make sure they're in the Slate window
	// array so that we can properly respond to OS window messages as soon as they're sent.  For example, a window
	// activation message may be sent by the OS as soon as the window is shown (in the Init function), and if we
	// don't add the Slate window to our window list, we wouldn't be able to route that message to the window.

	FSlateWindowHelper::ArrangeWindowToFront(SlateWindows, InSlateWindow);
	TSharedRef<FGenericWindow> NewWindow = MakeWindow( InSlateWindow, bShowImmediately );

	if( bShowImmediately )
	{
		InSlateWindow->ShowWindow();

		//@todo Slate: Potentially dangerous and annoying if all slate windows that are created steal focus.
		if( InSlateWindow->SupportsKeyboardFocus() && InSlateWindow->IsFocusedInitially() )
		{
			InSlateWindow->GetNativeWindow()->SetWindowFocus();
		}
	}

	return InSlateWindow;
}

TSharedRef< FGenericWindow > FSlateApplication::MakeWindow( TSharedRef<SWindow> InSlateWindow, const bool bShowImmediately )
{
	TSharedPtr<FGenericWindow> NativeParent = nullptr;
	TSharedPtr<SWindow> ParentWindow = InSlateWindow->GetParentWindow();
	if ( ParentWindow.IsValid() )
	{
		NativeParent = ParentWindow->GetNativeWindow();
	}

	TSharedRef< FGenericWindowDefinition > Definition = MakeShareable( new FGenericWindowDefinition() );

	const FVector2D Size = InSlateWindow->GetInitialDesiredSizeInScreen();
	Definition->WidthDesiredOnScreen = Size.X;
	Definition->HeightDesiredOnScreen = Size.Y;

	const FVector2D Position = InSlateWindow->GetInitialDesiredPositionInScreen();
	Definition->XDesiredPositionOnScreen = Position.X;
	Definition->YDesiredPositionOnScreen = Position.Y;

	Definition->HasOSWindowBorder = InSlateWindow->HasOSWindowBorder();
	Definition->TransparencySupport = InSlateWindow->GetTransparencySupport();
	Definition->AppearsInTaskbar = InSlateWindow->AppearsInTaskbar();
	Definition->IsTopmostWindow = InSlateWindow->IsTopmostWindow();
	Definition->AcceptsInput = InSlateWindow->AcceptsInput();
	Definition->ActivateWhenFirstShown = InSlateWindow->ActivateWhenFirstShown();

	Definition->HasCloseButton = InSlateWindow->HasCloseBox();
	Definition->SupportsMinimize = InSlateWindow->HasMinimizeBox();
	Definition->SupportsMaximize = InSlateWindow->HasMaximizeBox();

	Definition->IsModalWindow = InSlateWindow->IsModalWindow();
	Definition->IsRegularWindow = InSlateWindow->IsRegularWindow();
	Definition->HasSizingFrame = InSlateWindow->HasSizingFrame();
	Definition->SizeWillChangeOften = InSlateWindow->SizeWillChangeOften();
	Definition->ExpectedMaxWidth = InSlateWindow->GetExpectedMaxWidth();
	Definition->ExpectedMaxHeight = InSlateWindow->GetExpectedMaxHeight();

	Definition->Title = InSlateWindow->GetTitle().ToString();
	Definition->Opacity = InSlateWindow->GetOpacity();
	Definition->CornerRadius = InSlateWindow->GetCornerRadius();

	Definition->SizeLimits = InSlateWindow->GetSizeLimits();

	TSharedRef< FGenericWindow > NewWindow = PlatformApplication->MakeWindow();

	InSlateWindow->SetNativeWindow( NewWindow );

	InSlateWindow->SetCachedScreenPosition( Position );
	InSlateWindow->SetCachedSize( Size );

	PlatformApplication->InitializeWindow( NewWindow, Definition, NativeParent, bShowImmediately );

	ITextInputMethodSystem* const TextInputMethodSystem = PlatformApplication->GetTextInputMethodSystem();
	if ( TextInputMethodSystem )
	{
		TextInputMethodSystem->ApplyDefaults( NewWindow );
	}

	return NewWindow;
}

bool FSlateApplication::CanAddModalWindow() const
{
	// A modal window cannot be opened until the renderer has been created.
	return CanDisplayWindows();
}

bool FSlateApplication::CanDisplayWindows() const
{
	// The renderer must be created and global shaders be available
	return Renderer.IsValid() && Renderer->AreShadersInitialized();
}


void FSlateApplication::AddModalWindow( TSharedRef<SWindow> InSlateWindow, const TSharedPtr<const SWidget> InParentWidget, bool bSlowTaskWindow )
{
	if( !CanAddModalWindow() )
	{
		// Bail out.  The incoming window will never be added, and no native window will be created.
		return;
	}



	// Push the active modal window onto the stack.  
	ActiveModalWindows.AddUnique( InSlateWindow );

	// Close the open tooltip when a new window is open.  Tooltips from non-modal windows can be dangerous and cause rentrancy into code that shouldnt execute in a modal state.
	CloseToolTip();

	// Set the modal flag on the window
	InSlateWindow->SetAsModalWindow();
	
	// Make sure we aren't in the middle of using a slate draw buffer
	Renderer->FlushCommands();

	// In slow task windows, depending on the frequency with which the window is updated, it could be quite some time 
	// before the window is ticked (and drawn) so we hide the window by default and the slow task window will show it when needed
	const bool bShowWindow = !bSlowTaskWindow;

	// Create the new window
	// Note: generally a modal window should not be added without a parent but 
	// due to this being called from wxWidget editors, this is not always possible
	if( InParentWidget.IsValid() )
	{
		// Find the window of the parent widget
		FWidgetPath WidgetPath;
		GeneratePathToWidgetChecked( InParentWidget.ToSharedRef(), WidgetPath );
		AddWindowAsNativeChild( InSlateWindow, WidgetPath.GetWindow(), bShowWindow );
	}
	else
	{
		AddWindow( InSlateWindow, bShowWindow );
	}

	if ( ActiveModalWindows.Num() == 1 )
	{
		// Signal that a slate modal window has opened so external windows may be disabled as well
		ModalWindowStackStartedDelegate.ExecuteIfBound();
	}

	// Release mouse capture here in case the new modal window has been added in one of the mouse button
	// event callbacks. Otherwise it will be unresponsive until the next mouse up event.
	ReleaseMouseCapture();

	// Clear the cached pressed mouse buttons, in case a new modal window has been added between the mouse down and mouse up of another window.
	PressedMouseButtons.Empty();

	// Also force the platform capture off as the call to ReleaseMouseCapture() above still relies on mouse up messages to clear the capture
	PlatformApplication->SetCapture( nullptr );

	// Disable high precision mouse mode when a modal window is added.  On some OS'es even when a window is diabled, raw input is sent to it.
	PlatformApplication->SetHighPrecisionMouseMode( false, nullptr );

	// Block on all modal windows unless its a slow task.  In that case the game thread is allowed to run.
	if( !bSlowTaskWindow )
	{
		// Show the cursor if it was previously hidden so users can interact with the window
		if ( PlatformApplication->Cursor.IsValid() )
		{
			PlatformApplication->Cursor->Show( true );
		}

		// Tick slate from here in the event that we should not return until the modal window is closed.
		while( InSlateWindow == GetActiveModalWindow() )
		{
			// Tick and render Slate
			Tick();

			// Synchronize the game thread and the render thread so that the render thread doesn't get too far behind.
			Renderer->Sync();
		}
	}
}

void FSlateApplication::SetModalWindowStackStartedDelegate(FModalWindowStackStarted StackStartedDelegate)
{
	ModalWindowStackStartedDelegate = StackStartedDelegate;
}

void FSlateApplication::SetModalWindowStackEndedDelegate(FModalWindowStackEnded StackEndedDelegate)
{
	ModalWindowStackEndedDelegate = StackEndedDelegate;
}

TSharedRef<SWindow> FSlateApplication::AddWindowAsNativeChild( TSharedRef<SWindow> InSlateWindow, TSharedRef<SWindow> InParentWindow, const bool bShowImmediately )
{
	// Parent window must already have been added
	checkSlow(FSlateWindowHelper::ContainsWindow(SlateWindows, InParentWindow));

	// Add the Slate window to the Slate application's top-level window array.  Note that neither the Slate window
	// or the native window are ready to be used yet, however we need to make sure they're in the Slate window
	// array so that we can properly respond to OS window messages as soon as they're sent.  For example, a window
	// activation message may be sent by the OS as soon as the window is shown (in the Init function), and if we
	// don't add the Slate window to our window list, we wouldn't be able to route that message to the window.
	InParentWindow->AddChildWindow( InSlateWindow );
	TSharedRef<FGenericWindow> NewWindow = MakeWindow( InSlateWindow, bShowImmediately );

	if( bShowImmediately )
	{
		InSlateWindow->ShowWindow();

		//@todo Slate: Potentially dangerous and annoying if all slate windows that are created steal focus.
		if( InSlateWindow->SupportsKeyboardFocus() && InSlateWindow->IsFocusedInitially() )
		{
			InSlateWindow->GetNativeWindow()->SetWindowFocus();
		}
	}

	return InSlateWindow;
}


TSharedRef<SWindow> FSlateApplication::PushMenu( const TSharedRef<SWidget>& InParentContent, const TSharedRef<SWidget>& InContent, const FVector2D& SummonLocation, const FPopupTransitionEffect& TransitionEffect, const bool bFocusImmediately, const bool bShouldAutoSize, const FVector2D& WindowSize, const FVector2D& SummonLocationSize )
{
	FWidgetPath WidgetPath;
	GeneratePathToWidgetChecked( InParentContent, WidgetPath );

#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
	// The would-be parent of the new menu being pushed is about to be destroyed.  Any children added to an about to be destroyed window will also be destroyed
	if (IsWindowInDestroyQueue( WidgetPath.GetWindow() ))
	{
		UE_LOG(LogSlate, Warning, TEXT("FSlateApplication::PushMenu() called when parent window queued for destroy. New menu will be destroyed."));
	}
#endif

	return MenuStack.PushMenu( WidgetPath.GetWindow(), InContent, SummonLocation, TransitionEffect, bFocusImmediately, bShouldAutoSize, WindowSize, SummonLocationSize );
}

bool FSlateApplication::HasOpenSubMenus( TSharedRef<SWindow> Window ) const
{
	return MenuStack.HasOpenSubMenus(Window);
}

bool FSlateApplication::AnyMenusVisible() const
{
	return MenuStack.GetNumStackLevels() > 0;
}

void FSlateApplication::DismissAllMenus()
{
	MenuStack.Dismiss();
}

void FSlateApplication::DismissMenu( TSharedRef<SWindow> MenuWindowToDismiss )
{
	int32 Location = MenuStack.FindLocationInStack( MenuWindowToDismiss );
	// Dismiss everything starting at the window to dismiss
	MenuStack.Dismiss( Location );
}

int32 FSlateApplication::GetLocationInMenuStack( TSharedRef<SWindow> WindowToFind ) const
{
	return MenuStack.FindLocationInStack( WindowToFind );
}


void FSlateApplication::RequestDestroyWindow( TSharedRef<SWindow> InWindowToDestroy )
{
	struct local
	{
		static void Helper( const TSharedRef<SWindow> WindowToDestroy, TArray< TSharedRef<SWindow> >& OutWindowDestroyQueue)
		{
			/** @return the list of this window's child windows */
			TArray< TSharedRef<SWindow> >& ChildWindows = WindowToDestroy->GetChildWindows();

			// Children need to be destroyed first. 
			if( ChildWindows.Num() > 0 )
			{
				for( int32 ChildIndex = 0; ChildIndex < ChildWindows.Num(); ++ChildIndex )
				{	
					// Recursively request that the window is destroyed which will also queue any children of children etc...
					Helper( ChildWindows[ ChildIndex ], OutWindowDestroyQueue );
				}
			}

			OutWindowDestroyQueue.AddUnique( WindowToDestroy );
		}
	};

	local::Helper( InWindowToDestroy, WindowDestroyQueue );

	DestroyWindowsImmediately();
}

void FSlateApplication::DestroyWindowImmediately( TSharedRef<SWindow> WindowToDestroy ) 
{
	// Request that the window and its children are destroyed
	RequestDestroyWindow( WindowToDestroy );

	DestroyWindowsImmediately();
}


void FSlateApplication::ExternalModalStart()
{
	if( NumExternalModalWindowsActive++ == 0 )
	{
		// Close all open menus.
		DismissAllMenus();

		// Close tool-tips
		CloseToolTip();

		// Tick and render Slate so that it can destroy any menu windows if necessary before we disable.
		Tick();
		Renderer->Sync();

		if( ActiveModalWindows.Num() > 0 )
		{
			// There are still modal windows so only enable the new active modal window.
			GetActiveModalWindow()->EnableWindow( false );
		}
		else
		{
			// We are creating a modal window so all other windows need to be disabled.
			for( TArray< TSharedRef<SWindow> >::TIterator CurrentWindowIt( SlateWindows ); CurrentWindowIt; ++CurrentWindowIt )
			{
				TSharedRef<SWindow> CurrentWindow = ( *CurrentWindowIt );
				CurrentWindow->EnableWindow( false );
			}
		}
	}
}


void FSlateApplication::ExternalModalStop()
{
	check(NumExternalModalWindowsActive > 0);
	if( --NumExternalModalWindowsActive == 0 )
	{
		if( ActiveModalWindows.Num() > 0 )
		{
			// There are still modal windows so only enable the new active modal window.
			GetActiveModalWindow()->EnableWindow( true );
		}
		else
		{
			// We are creating a modal window so all other windows need to be disabled.
			for( TArray< TSharedRef<SWindow> >::TIterator CurrentWindowIt( SlateWindows ); CurrentWindowIt; ++CurrentWindowIt )
			{
				TSharedRef<SWindow> CurrentWindow = ( *CurrentWindowIt );
				CurrentWindow->EnableWindow( true );
			}
		}
	}
}

void FSlateApplication::InvalidateAllViewports()
{
	Renderer->InvalidateAllViewports();
}


void FSlateApplication::RegisterGameViewport( TSharedRef<SViewport> InViewport )
{
	InViewport->SetActive(true);
	GameViewportWidget = InViewport;
	
	FWidgetPath PathToViewport;
	// If we cannot find the window it could have been destroyed.
	if (FSlateWindowHelper::FindPathToWidget(SlateWindows, InViewport, PathToViewport, EVisibility::All))
	{
		FReply Reply = FReply::Handled().SetUserFocus(InViewport, EFocusCause::SetDirectly, true);
	
		// Set keyboard focus on the actual OS window for the top level Slate window in the viewport path
		// This is needed because some OS messages are only sent to the window with keyboard focus
		// Slate will translate the message and send it to the actual widget with focus.
		// Without this we don't get WM_KEYDOWN or WM_CHAR messages in play in viewport sessions.
		PathToViewport.GetWindow()->GetNativeWindow()->SetWindowFocus();

		ProcessReply( PathToViewport, Reply, nullptr, nullptr );
	}
}


void FSlateApplication::UnregisterGameViewport()
{
	ResetToDefaultInputSettings();

	if (GameViewportWidget.IsValid())
	{
		GameViewportWidget.Pin()->SetActive(false);
	}
	GameViewportWidget.Reset();
}

TSharedPtr<SViewport> FSlateApplication::GetGameViewport() const
{
	return GameViewportWidget.Pin();
}

int32 FSlateApplication::GetUserIndexForKeyboard() const
{
	//@Todo Slate: Fix this to actual be a map and add API for the user to edit the mapping.
	// HACK! Just directly mapping the keyboard to User Index 0.
	return 0;
}
 
int32 FSlateApplication::GetUserIndexForController(int32 ControllerId) const
{
	//@Todo Slate: Fix this to actual be a map and add API for the user to edit the mapping.
	// HACK! Just directly mapping a controller to a User Index.
	return ControllerId;
}

void FSlateApplication::SetUserFocusToGameViewport(uint32 UserIndex, EFocusCause ReasonFocusIsChanging /* = EFocusCause::SetDirectly*/)
{
	TSharedPtr<SViewport> CurrentGameViewportWidget = GameViewportWidget.Pin();
	if (CurrentGameViewportWidget.IsValid())
	{
		SetUserFocus(UserIndex, CurrentGameViewportWidget, ReasonFocusIsChanging);
	}
}

void FSlateApplication::SetFocusToGameViewport()
{
	SetUserFocusToGameViewport(GetUserIndexForKeyboard(), EFocusCause::SetDirectly);
}

void FSlateApplication::SetAllUserFocusToGameViewport(EFocusCause ReasonFocusIsChanging /* = EFocusCause::SetDirectly*/)
{
	TSharedPtr< SViewport > CurrentGameViewportWidget = GameViewportWidget.Pin();

	if (CurrentGameViewportWidget.IsValid())
	{
		FWidgetPath PathToWidget;
		FSlateWindowHelper::FindPathToWidget(SlateWindows, CurrentGameViewportWidget.ToSharedRef(), /*OUT*/ PathToWidget);

		for (int32 SlateUserIndex = 0; SlateUserIndex < SlateApplicationDefs::MaxUsers; ++SlateUserIndex)
		{
			SetUserFocus(SlateUserIndex, PathToWidget, ReasonFocusIsChanging);
		}
	}
}

void FSlateApplication::SetJoystickCaptorToGameViewport()
{
	SetAllUserFocusToGameViewport();
}

void FSlateApplication::SetUserFocus(uint32 UserIndex, const TSharedPtr<SWidget>& WidgetToFocus, EFocusCause ReasonFocusIsChanging /* = EFocusCause::SetDirectly*/)
{
	const bool bValidWidget = WidgetToFocus.IsValid();
	ensureMsgf(bValidWidget, TEXT("Attempting to focus an invalid widget. If your intent is to clear focus use ClearUserFocus()"));
	if (bValidWidget)
	{
		FWidgetPath PathToWidget;
		const bool bFound = FSlateWindowHelper::FindPathToWidget(SlateWindows, WidgetToFocus.ToSharedRef(), /*OUT*/ PathToWidget);
		if (bFound)
		{
			SetUserFocus(UserIndex, PathToWidget, ReasonFocusIsChanging);
		}
		else
		{
			//ensureMsgf(bFound, TEXT("Attempting to focus a widget that isn't in the tree and visible: %s. If your intent is to clear focus use ClearUserFocus()"), WidgetToFocus->ToString());
		}
	}
}

TSharedPtr<SWidget> FSlateApplication::GetUserFocusedWidget(uint32 UserIndex) const
{
	const FUserFocusEntry& UserFocusEntry = UserFocusEntries[UserIndex];
	return UserFocusEntry.WidgetPath.IsValid() ? UserFocusEntry.WidgetPath.GetLastWidget().Pin() : TSharedPtr<SWidget>();
}

TSharedPtr<SWidget> FSlateApplication::GetJoystickCaptor(uint32 UserIndex) const
{
	return GetUserFocusedWidget(UserIndex);
}

void FSlateApplication::ClearUserFocus(uint32 UserIndex, EFocusCause ReasonFocusIsChanging /* = EFocusCause::SetDirectly*/)
{
	SetUserFocus(UserIndex, FWidgetPath(), ReasonFocusIsChanging);
}

void FSlateApplication::ReleaseJoystickCapture(uint32 UserIndex)
{
	ClearUserFocus(UserIndex);
}

void FSlateApplication::SetKeyboardFocus(const TSharedPtr< SWidget >& OptionalWidgetToFocus, EFocusCause ReasonFocusIsChanging /* = EFocusCause::SetDirectly*/)
{
	SetUserFocus(GetUserIndexForKeyboard(), OptionalWidgetToFocus, ReasonFocusIsChanging);
}

void FSlateApplication::ClearKeyboardFocus(const EFocusCause ReasonFocusIsChanging)
{
	SetUserFocus(GetUserIndexForKeyboard(), FWidgetPath(), ReasonFocusIsChanging);
}

void FSlateApplication::ResetToDefaultInputSettings()
{
	ProcessReply(FWidgetPath(), FReply::Handled().ClearUserFocus(true), nullptr, nullptr);
	ResetToDefaultPointerInputSettings();
}

void FSlateApplication::ResetToDefaultPointerInputSettings()
{
	for (auto MouseCaptorPath : MouseCaptor.ToWidgetPaths())
	{
		ProcessReply(MouseCaptorPath, FReply::Handled().ReleaseMouseCapture(), nullptr, nullptr);
	}

	ProcessReply(FWidgetPath(), FReply::Handled().ReleaseMouseLock(), nullptr, nullptr);

	if ( PlatformApplication->Cursor.IsValid() )
	{
		PlatformApplication->Cursor->SetType(EMouseCursor::Default);
	}
}

void* FSlateApplication::GetMouseCaptureWindow( void ) const
{
	return PlatformApplication->GetCapture();
}


void FSlateApplication::ReleaseMouseCapture()
{
	MouseCaptor.InvalidateCaptureForAllPointers();
}

TSharedPtr<SWindow> FSlateApplication::GetActiveTopLevelWindow() const
{
	return ActiveTopLevelWindow.Pin();
}


TSharedPtr<SWindow> FSlateApplication::GetActiveModalWindow() const
{
	return (ActiveModalWindows.Num() > 0) ? ActiveModalWindows.Last() : nullptr;
}

bool FSlateApplication::SetKeyboardFocus(const FWidgetPath& InFocusPath, const EFocusCause InCause /*= EFocusCause::SetDirectly*/)
{
	return SetUserFocus(GetUserIndexForKeyboard(), InFocusPath, InCause);
}

bool FSlateApplication::SetUserFocus(const uint32 InUserIndex, const FWidgetPath& InFocusPath, const EFocusCause InCause)
{
	check(InUserIndex >= 0 && InUserIndex < SlateApplicationDefs::MaxUsers);

	FUserFocusEntry& UserFocusEntry = UserFocusEntries[InUserIndex];

	TSharedPtr<IWidgetReflector> WidgetReflector = WidgetReflectorPtr.Pin();
	const bool bReflectorShowingFocus = WidgetReflector.IsValid() && WidgetReflector->IsShowingFocus();

	// Get the old Widget information
	const FWeakWidgetPath OldFocusedWidgetPath = UserFocusEntry.WidgetPath;
	TSharedPtr<SWidget> OldFocusedWidget = OldFocusedWidgetPath.IsValid() ? OldFocusedWidgetPath.GetLastWidget().Pin() : TSharedPtr< SWidget >();

	// Get the new widget information by finding the first widget in the path that supports focus
	FWidgetPath NewFocusedWidgetPath;
	TSharedPtr<SWidget> NewFocusedWidget;

	if (InFocusPath.IsValid())
	{
		for (int32 WidgetIndex = InFocusPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
		{
			const FArrangedWidget& WidgetToFocus = InFocusPath.Widgets[WidgetIndex];

			// Does this widget support keyboard focus?  If so, then we'll go ahead and set it!
			if (WidgetToFocus.Widget->SupportsKeyboardFocus())
			{
				// Is we aren't changing focus then simply return
				if (WidgetToFocus.Widget == OldFocusedWidget)
				{
					return false;
				}
				NewFocusedWidget = WidgetToFocus.Widget;
				NewFocusedWidgetPath = InFocusPath.GetPathDownTo(NewFocusedWidget.ToSharedRef());
				break;
			}
		}
	}

	// Notify widgets in the old focus path that focus is changing
	if (OldFocusedWidgetPath.IsValid())
	{
		FScopedSwitchWorldHack SwitchWorld(OldFocusedWidgetPath.Window.Pin());

		for (int32 ChildIndex = 0; ChildIndex < OldFocusedWidgetPath.Widgets.Num(); ++ChildIndex)
		{
			TSharedPtr<SWidget> SomeWidget = OldFocusedWidgetPath.Widgets[ChildIndex].Pin();
			if (SomeWidget.IsValid())
			{
				SomeWidget->OnFocusChanging(OldFocusedWidgetPath, NewFocusedWidgetPath);
			}
		}
	}

	// Notify widgets in the new focus path that focus is changing
	if (NewFocusedWidgetPath.IsValid())
	{
		FScopedSwitchWorldHack SwitchWorld(NewFocusedWidgetPath.GetWindow());

		for (int32 ChildIndex = 0; ChildIndex < NewFocusedWidgetPath.Widgets.Num(); ++ChildIndex)
		{
			TSharedPtr<SWidget> SomeWidget = NewFocusedWidgetPath.Widgets[ChildIndex].Widget;
			if (SomeWidget.IsValid())
			{
				SomeWidget->OnFocusChanging(OldFocusedWidgetPath, NewFocusedWidgetPath);
			}
		}
	}

	//UE_LOG(LogSlate, Warning, TEXT("Focus for user %i set to %s."), InUserIndex, NewFocusedWidget.IsValid() ? *NewFocusedWidget->ToString() : TEXT("Invalid"));

	// Store a weak widget path to the widget that's taking focus
	UserFocusEntry.WidgetPath = FWeakWidgetPath(NewFocusedWidgetPath);

	// Store the cause of the focus
	UserFocusEntry.FocusCause = InCause;

	// Figure out if we should show focus for this focus entry
	UserFocusEntry.ShowFocus = false;
	if (NewFocusedWidgetPath.IsValid())
	{
		UserFocusEntry.ShowFocus = InCause == EFocusCause::Navigation;
		for (int32 WidgetIndex = NewFocusedWidgetPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
		{
			TOptional<bool> ShowFocus = NewFocusedWidgetPath.Widgets[WidgetIndex].Widget->OnQueryShowFocus(InCause);
			if (ShowFocus.IsSet())
			{
				UserFocusEntry.ShowFocus = ShowFocus.GetValue();
				break;
			}
		}
	}

	// Let the old widget know that it lost keyboard focus
	if(OldFocusedWidget.IsValid())
	{
		// Switch worlds for widgets in the old path
		FScopedSwitchWorldHack SwitchWorld(OldFocusedWidgetPath.Window.Pin());

		// Let previously-focused widget know that it's losing focus
		OldFocusedWidget->OnFocusLost(FFocusEvent(InCause, InUserIndex));
	}

	if (bReflectorShowingFocus)
	{
		WidgetReflector->SetWidgetsToVisualize(NewFocusedWidgetPath);
	}

	// Let the new widget know that it's received keyboard focus
	if (NewFocusedWidget.IsValid())
	{
		TSharedPtr<SWindow> FocusedWindow = NewFocusedWidgetPath.GetWindow();
		
		// Set the windows restore state - we do this before calling OnFocusReceived to ensure that if
		// OnFocusReceived returns a new UserFocus, the WidgetToFocusOnActivate will get set correctly
		//@Todo Slate: We need to store the full focus state
		//@Todo Slate: Why are we checking FocusedWindow != NewFocusedWidget?
		if (FocusedWindow.IsValid() && FocusedWindow != NewFocusedWidget)
		{
			FocusedWindow->SetWidgetToFocusOnActivate(NewFocusedWidget);
		}

		// Switch worlds for widgets in the new path
		FScopedSwitchWorldHack SwitchWorld(FocusedWindow);
		
		const FArrangedWidget& WidgetToFocus = NewFocusedWidgetPath.Widgets.Last();

		FReply Reply = NewFocusedWidget->OnFocusReceived(WidgetToFocus.Geometry, FFocusEvent(InCause, InUserIndex));
		if (Reply.IsEventHandled())
		{
			ProcessReply(InFocusPath, Reply, nullptr, nullptr);
		}
	}

	return true;
}


FModifierKeysState FSlateApplication::GetModifierKeys() const
{
	return PlatformApplication->GetModifierKeys();
}


void FSlateApplication::OnShutdown()
{
	CloseAllWindowsImmediately();
}

void FSlateApplication::CloseAllWindowsImmediately()
{
	// Clean up our tooltip window
	TSharedPtr< SWindow > PinnedToolTipWindow(ToolTipWindow.Pin());
	if (PinnedToolTipWindow.IsValid())
	{
		PinnedToolTipWindow->RequestDestroyWindow();
		ToolTipWindow.Reset();
	}

	for (int32 WindowIndex = 0; WindowIndex < SlateWindows.Num(); ++WindowIndex)
	{
		// Destroy all top level windows.  This will also request that all children of each window be destroyed
		RequestDestroyWindow(SlateWindows[WindowIndex]);
	}

	DestroyWindowsImmediately();
}

void FSlateApplication::DestroyWindowsImmediately()
{
	// Destroy any windows that were queued for deletion.

	// Thomas.Sarkanen: I've changed this from a for() to a while() loop so that it is now valid to call RequestDestroyWindow()
	// in the callstack of another call to RequestDestroyWindow(). Previously this would cause a stack overflow, as the
	// WindowDestroyQueue would be continually added to each time the for() loop ran.
	while ( WindowDestroyQueue.Num() > 0 )
	{
		TSharedRef<SWindow> CurrentWindow = WindowDestroyQueue[0];
		WindowDestroyQueue.Remove(CurrentWindow);
		if( ActiveModalWindows.Num() > 0 && ActiveModalWindows.Contains( CurrentWindow ) )
		{
			ActiveModalWindows.Remove( CurrentWindow );

			if( ActiveModalWindows.Num() > 0 )
			{
				// There are still modal windows so only enable the new active modal window.
				GetActiveModalWindow()->EnableWindow( true );
			}
			else
			{
				//  There are no modal windows so renable all slate windows
				for ( TArray< TSharedRef<SWindow> >::TConstIterator SlateWindowIter( SlateWindows ); SlateWindowIter; ++SlateWindowIter )
				{
					// All other windows need to be re-enabled BEFORE a modal window is destroyed or focus will not be set correctly
					(*SlateWindowIter)->EnableWindow( true );
				}

				// Signal that all slate modal windows are closed
				ModalWindowStackEndedDelegate.ExecuteIfBound();
			}
		}

		// Any window being destroyed should be removed from the menu stack if its in it
		MenuStack.RemoveWindow( CurrentWindow );

		// Perform actual cleanup of the window
		PrivateDestroyWindow( CurrentWindow );
	}

	WindowDestroyQueue.Empty();
}


void FSlateApplication::SetExitRequestedHandler( const FSimpleDelegate& OnExitRequestedHandler )
{
	OnExitRequested = OnExitRequestedHandler;
}


bool FSlateApplication::GeneratePathToWidgetUnchecked( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter ) const
{
	return FSlateWindowHelper::FindPathToWidget(SlateWindows, InWidget, OutWidgetPath, VisibilityFilter);
}


void FSlateApplication::GeneratePathToWidgetChecked( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath, EVisibility VisibilityFilter ) const
{
	const bool bWasFound = FSlateWindowHelper::FindPathToWidget(SlateWindows, InWidget, OutWidgetPath, VisibilityFilter);
	check( bWasFound );
}


TSharedPtr<SWindow> FSlateApplication::FindWidgetWindow( TSharedRef< const SWidget > InWidget ) const
{
	FWidgetPath WidgetPath;
	return FindWidgetWindow( InWidget, WidgetPath );
}


TSharedPtr<SWindow> FSlateApplication::FindWidgetWindow( TSharedRef< const SWidget > InWidget, FWidgetPath& OutWidgetPath ) const
{
	// If the user wants a widget path back populate it instead
	const bool bWasFound = FSlateWindowHelper::FindPathToWidget(SlateWindows, InWidget, OutWidgetPath, EVisibility::All);
	if( bWasFound )
	{
		return OutWidgetPath.TopLevelWindow;
	}
	return nullptr;
}


void FSlateApplication::ProcessReply( const FWidgetPath& CurrentEventPath, const FReply TheReply, const FWidgetPath* WidgetsUnderMouse, const FPointerEvent* InMouseEvent, uint32 UserIndex )
{
	const TSharedPtr<FDragDropOperation> ReplyDragDropContent = TheReply.GetDragDropContent();
	const bool bStartingDragDrop = ReplyDragDropContent.IsValid();

	// Release mouse capture if requested or if we are starting a drag and drop.
	// Make sure to only clobber WidgetsUnderCursor if we actually had a mouse capture.
	uint32 PointerIndex = InMouseEvent != nullptr ? InMouseEvent->GetPointerIndex() : CursorPointerIndex;
	if (MouseCaptor.HasCaptureForPointerIndex(PointerIndex) && (TheReply.ShouldReleaseMouse() || bStartingDragDrop) )
	{
		WidgetsUnderCursorLastEvent = MouseCaptor.ToWeakPath(PointerIndex);
		MouseCaptor.InvalidateCaptureForPointer(PointerIndex);
	}

	// Clear focus is requested.
	if (TheReply.ShouldReleaseUserFocus())
	{
		if (TheReply.AffectsAllUsers())
		{
			for (int32 SlateUserIndex = 0; SlateUserIndex < SlateApplicationDefs::MaxUsers; ++SlateUserIndex)
			{
				SetUserFocus(SlateUserIndex, FWidgetPath(), TheReply.GetFocusCause());
			}
		}
		else
		{
			SetUserFocus(UserIndex, FWidgetPath(), TheReply.GetFocusCause());
		}
	}

	if (TheReply.ShouldEndDragDrop())
	{
		CancelDragDrop();
	}

	if ( bStartingDragDrop )
	{
		checkf( !this->DragDropContent.IsValid(), TEXT("Drag and Drop already in progress!") );
		check( true == TheReply.IsEventHandled() );
		check( WidgetsUnderMouse != nullptr );
		check( InMouseEvent != nullptr );
		DragDropContent = ReplyDragDropContent;

		// We have entered drag and drop mode.
		// Pretend that the mouse left all the previously hovered widgets, and a drag entered them.
		FEventRouter::Route<FNoReply>(this, FEventRouter::FBubblePolicy(*WidgetsUnderMouse), *InMouseEvent, [](const FArrangedWidget& SomeWidget, const FPointerEvent& PointerEvent)
		{
			SomeWidget.Widget->OnMouseLeave( PointerEvent );
			return FNoReply();
		});

		FEventRouter::Route<FNoReply>(this, FEventRouter::FBubblePolicy(*WidgetsUnderMouse), FDragDropEvent( *InMouseEvent, ReplyDragDropContent ), [](const FArrangedWidget& SomeWidget, const FDragDropEvent& DragDropEvent )
		{
			SomeWidget.Widget->OnDragEnter( SomeWidget.Geometry, DragDropEvent );
			return FNoReply();
		});
	}
	
	TSharedPtr<SWidget> RequestedMouseCaptor = TheReply.GetMouseCaptor();
	// Do not capture the mouse if we are also starting a drag and drop.
	if( RequestedMouseCaptor.IsValid() && !bStartingDragDrop )
	{
		MouseCaptor.SetMouseCaptor(PointerIndex, CurrentEventPath, RequestedMouseCaptor );
	}
	
	if( CurrentEventPath.IsValid() && ( TheReply.ShouldReleaseMouse() || RequestedMouseCaptor.IsValid() ) )
	{
		// If the mouse is being captured or released, toggle high precision raw input if requested by the reply.
		// Raw input is only used with mouse capture
		const TSharedRef< SWindow> Window = CurrentEventPath.GetWindow();

		if ( TheReply.ShouldUseHighPrecisionMouse() )
		{
			PlatformApplication->SetCapture( Window->GetNativeWindow() );
			PlatformApplication->SetHighPrecisionMouseMode( true, Window->GetNativeWindow() );
		}
		else if ( PlatformApplication->IsUsingHighPrecisionMouseMode() )
		{
			PlatformApplication->SetHighPrecisionMouseMode( false, nullptr );
			PlatformApplication->SetCapture( nullptr );
		}
	}

	TOptional<FIntPoint> RequestedMousePos = TheReply.GetRequestedMousePos();
	if( RequestedMousePos.IsSet() )
	{
		const FVector2D Position = RequestedMousePos.GetValue();
		PointerIndexLastPositionMap.Add(CursorPointerIndex, Position);
		SetCursorPos( Position );
	}

	if( TheReply.GetMouseLockWidget().IsValid() )
	{
		// The reply requested mouse lock so tell the native application to lock the mouse to the widget receiving the event
		LockCursor( TheReply.GetMouseLockWidget() );
	}
	else if( TheReply.ShouldReleaseMouseLock() )
	{
		// Unlock the mouse
		LockCursor( nullptr );
	}
	
	// If we have a valid Navigation request and enough time has passed since the last navigation then try to navigate.
	if (TheReply.GetNavigationType() != EUINavigation::Invalid)
	{
		TSharedPtr<SWidget> FocusedWidget = GetUserFocusedWidget(UserIndex);
		if (FocusedWidget.IsValid())
		{
			FNavigationEvent NavigationEvent(UserIndex, TheReply.GetNavigationType());

			const FUserFocusEntry& UserFocusEntry = UserFocusEntries[UserIndex];
			FWidgetPath EventPath = UserFocusEntry.WidgetPath.ToWidgetPath();

			FNavigationReply NavigationReply = FNavigationReply::Escape();
			for (int32 WidgetIndex = EventPath.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
			{
				FArrangedWidget& SomeWidgetGettingEvent = EventPath.Widgets[WidgetIndex];
				if (SomeWidgetGettingEvent.Widget->IsEnabled())
				{
					NavigationReply = SomeWidgetGettingEvent.Widget->OnNavigation(SomeWidgetGettingEvent.Geometry, NavigationEvent).SetHandler(SomeWidgetGettingEvent.Widget);
					if (NavigationReply.GetBoundaryRule() != EUINavigationRule::Escape || WidgetIndex == 0)
					{
						AttemptNavigation(NavigationEvent, NavigationReply, SomeWidgetGettingEvent);
						break;
					}
				}
			}
		}
	}

	if ( TheReply.GetDetectDragRequest().IsValid() )
	{
		DragDetector.DetectDragForWidget = WidgetsUnderMouse->GetPathDownTo( TheReply.GetDetectDragRequest().ToSharedRef() );
		DragDetector.DetectDragButton = TheReply.GetDetectDragRequestButton();
		DragDetector.DetectDragStartLocation = InMouseEvent->GetScreenSpacePosition();
	}

	// Set focus if requested.
	TSharedPtr<SWidget> RequestedFocusRecepient = TheReply.GetUserFocusRecepient();
	if (TheReply.ShouldSetUserFocus() || RequestedFocusRecepient.IsValid())
	{
		if (TheReply.AffectsAllUsers())
		{
			for (int32 SlateUserIndex = 0; SlateUserIndex < SlateApplicationDefs::MaxUsers; ++SlateUserIndex)
			{
				SetUserFocus(SlateUserIndex, RequestedFocusRecepient, TheReply.GetFocusCause());
			}
		}
		else
		{
			SetUserFocus(UserIndex, RequestedFocusRecepient, TheReply.GetFocusCause());
		}
	}
}

void FSlateApplication::LockCursor(const TSharedPtr<SWidget>& Widget)
{
	if (PlatformApplication->Cursor.IsValid())
	{
		if (Widget.IsValid())
		{
			// Get a path to this widget so we know the position and size of its geometry
			FWidgetPath WidgetPath;
			const bool bFoundWidget = GeneratePathToWidgetUnchecked(Widget.ToSharedRef(), WidgetPath);
			if ( ensureMsgf(bFoundWidget, TEXT("Attempting to LockCursor() to widget but could not find widget %s"), *Widget->ToString()) )
			{
				LockCursorToPath(WidgetPath);
			}
		}
		else
		{
			UnlockCursor();
		}
	}
}

void FSlateApplication::LockCursorToPath(const FWidgetPath& WidgetPath)
{
	// The last widget in the path should be the widget we are locking the cursor to
	const FArrangedWidget& WidgetGeom = WidgetPath.Widgets[WidgetPath.Widgets.Num() - 1];

	TSharedRef<SWindow> Window = WidgetPath.GetWindow();
	// Do not attempt to lock the cursor to the window if its not in the foreground.  It would cause annoying side effects
	if (Window->GetNativeWindow()->IsForegroundWindow())
	{
		const FSlateRect SlateClipRect = WidgetGeom.Geometry.GetClippingRect();
		CursorLock.LastComputedBounds = SlateClipRect;
		CursorLock.PathToLockingWidget = WidgetPath;
			
		// Generate a screen space clip rect based on the widgets geometry

		// Note: We round the upper left coordinate of the clip rect so we guarantee the rect is inside the geometry of the widget.  If we truncated when there is a half pixel we would cause the clip
		// rect to be half a pixel larger than the geometry and cause the mouse to go outside of the geometry.
		RECT ClipRect;
		ClipRect.left = FMath::RoundToInt(SlateClipRect.Left);
		ClipRect.top = FMath::RoundToInt(SlateClipRect.Top);
		ClipRect.right = FMath::TruncToInt(SlateClipRect.Right);
		ClipRect.bottom = FMath::TruncToInt(SlateClipRect.Bottom);

		// Lock the mouse to the widget
		PlatformApplication->Cursor->Lock(&ClipRect);
	}
}

void FSlateApplication::UnlockCursor()
{
	// Unlock the mouse
	PlatformApplication->Cursor->Lock(nullptr);
	CursorLock.PathToLockingWidget = FWeakWidgetPath();
}

void FSlateApplication::UpdateCursorLockRegion()
{
	const FWidgetPath PathToWidget = CursorLock.PathToLockingWidget.ToWidgetPath(FWeakWidgetPath::EInterruptedPathHandling::ReturnInvalid);
	if (PathToWidget.IsValid())
	{
		const FSlateRect ComputedClipRect = PathToWidget.Widgets.Last().Geometry.GetClippingRect();
		if (ComputedClipRect != CursorLock.LastComputedBounds)
		{
			LockCursorToPath(PathToWidget);
		}
	}
}

void FSlateApplication::QueryCursor()
{
	if ( PlatformApplication->Cursor.IsValid() )
	{
		// drag-drop overrides cursor
		FCursorReply CursorReply = FCursorReply::Unhandled();
		if(IsDragDropping())
		{
			CursorReply = DragDropContent->OnCursorQuery();
		}
		
		if (!CursorReply.IsEventHandled())
		{
			FWidgetPath WidgetsToQueryForCursor;
			const TSharedPtr<SWindow> ActiveModalWindow = GetActiveModalWindow();

			const FVector2D CurrentCursorPosition = GetCursorPos();
			const FVector2D LastCursorPosition = GetLastCursorPos();
			const FPointerEvent CursorEvent(
				CursorPointerIndex,
				CurrentCursorPosition,
				LastCursorPosition,
				CurrentCursorPosition - LastCursorPosition,
				PressedMouseButtons,
				PlatformApplication->GetModifierKeys()
				);

			// Query widgets with mouse capture for the cursor
			if (MouseCaptor.HasCaptureForPointerIndex(CursorPointerIndex))
			{
				FWidgetPath MouseCaptorPath = MouseCaptor.ToWidgetPath( FWeakWidgetPath::EInterruptedPathHandling::Truncate, &CursorEvent);
				if ( MouseCaptorPath.IsValid() )
				{
					TSharedRef< SWindow > CaptureWindow = MouseCaptorPath.GetWindow();

					// Never query the mouse captor path if it is outside an active modal window
					if ( !ActiveModalWindow.IsValid() || ( CaptureWindow == ActiveModalWindow || CaptureWindow->IsDescendantOf(ActiveModalWindow) ) )
					{
						WidgetsToQueryForCursor = MouseCaptorPath;
					}
				}
			}
			else
			{
				WidgetsToQueryForCursor = LocateWindowUnderMouse( GetCursorPos(), GetInteractiveTopLevelWindows() );
			}

			if (WidgetsToQueryForCursor.IsValid())
			{
				// Switch worlds for widgets in the current path
				FScopedSwitchWorldHack SwitchWorld( WidgetsToQueryForCursor );

				for (int32 WidgetIndex = WidgetsToQueryForCursor.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
				{
					const FArrangedWidget& ArrangedWidget = WidgetsToQueryForCursor.Widgets[WidgetIndex];
					CursorReply = ArrangedWidget.Widget->OnCursorQuery(ArrangedWidget.Geometry, CursorEvent);
					if (CursorReply.IsEventHandled())
					{
						if (!CursorReply.GetCursorWidget().IsValid())
						{
							for (; WidgetIndex >= 0; --WidgetIndex)
							{
								TOptional<TSharedRef<SWidget>> CursorWidget = WidgetsToQueryForCursor.Widgets[WidgetIndex].Widget->OnMapCursor(CursorReply);
								if (CursorWidget.IsSet())
								{
									CursorReply.SetCursorWidget(WidgetsToQueryForCursor.GetWindow(), CursorWidget.GetValue());
									break;
								}
							}
						}
						break;
					}
				}

				if (!CursorReply.IsEventHandled() && WidgetsToQueryForCursor.IsValid())
				{
					// Query was NOT handled, and we are still over a slate window.
					CursorReply = FCursorReply::Cursor(EMouseCursor::Default);
				}
			}
			else
			{
				// Set the default cursor when there isn't an active window under the cursor and the mouse isn't captured
				CursorReply = FCursorReply::Cursor(EMouseCursor::Default);
			}
		}
		ProcessCursorReply(CursorReply);
	}
}

void FSlateApplication::ProcessCursorReply(const FCursorReply& CursorReply)
{
	if (CursorReply.IsEventHandled())
	{
		CursorWidgetPtr = CursorReply.GetCursorWidget();
		if (CursorReply.GetCursorWidget().IsValid())
		{
			CursorReply.GetCursorWidget()->SetVisibility(EVisibility::HitTestInvisible);
			CursorWindowPtr = CursorReply.GetCursorWindow();
			PlatformApplication->Cursor->SetType(EMouseCursor::None);
		}
		else
		{
			CursorWindowPtr.Reset();
			PlatformApplication->Cursor->SetType(CursorReply.GetCursorType());
		}
	}
	else
	{
		CursorWindowPtr.Reset();
		CursorWidgetPtr.Reset();
	}
}

void FSlateApplication::SpawnToolTip( const TSharedRef<IToolTip>& InToolTip, const FVector2D& InSpawnLocation )
{
	// Close existing tool tip, if we have one
	CloseToolTip();

	// Spawn the new tool tip
	{
		TSharedPtr< SWindow > NewToolTipWindow( ToolTipWindow.Pin() );
		if( !NewToolTipWindow.IsValid() )
		{
			// Create the tool tip window
			NewToolTipWindow = SWindow::MakeToolTipWindow();

			// Don't show the window yet.  We'll set it up with some content first!
			const bool bShowImmediately = false;
			AddWindow( NewToolTipWindow.ToSharedRef(), bShowImmediately );
		}

		NewToolTipWindow->SetContent
		(
			SNew(SWeakWidget)
			.PossiblyNullContent(InToolTip->AsWidget())
		);

		// Move the window again to recalculate popup window position if necessary (tool tip may spawn outside of the monitors work area)
		// and in that case we need to adjust it
		DesiredToolTipLocation = InSpawnLocation;
		{
			// Make sure the desired size is valid
			NewToolTipWindow->SlatePrepass(FSlateApplication::Get().GetApplicationScale());

			FSlateRect Anchor(DesiredToolTipLocation.X, DesiredToolTipLocation.Y, DesiredToolTipLocation.X, DesiredToolTipLocation.Y);
			DesiredToolTipLocation = CalculatePopupWindowPosition( Anchor, NewToolTipWindow->GetDesiredSize() );

			// MoveWindowTo will adjust the window's position, if needed
			NewToolTipWindow->MoveWindowTo( DesiredToolTipLocation );
		}


		// Show the window
		NewToolTipWindow->ShowWindow();

		// Keep a weak reference to the tool tip window
		ToolTipWindow = NewToolTipWindow;

		// Keep track of when this tool tip was spawned
		ToolTipSummonTime = FPlatformTime::Seconds();
	}
}

void FSlateApplication::CloseToolTip()
{
	TSharedPtr< SWindow > PinnedToolTipWindow( ToolTipWindow.Pin() );
	if( PinnedToolTipWindow.IsValid() && PinnedToolTipWindow->IsVisible() )
	{
		// Notify the source widget that its tooltip is closing
		if (SWidget* SourceWidget = ActiveToolTipWidgetSource.Pin().Get())
		{
			SourceWidget->OnToolTipClosing();
		}

		TSharedPtr<IToolTip> StableActiveToolTip = ActiveToolTip.Pin();
		if ( StableActiveToolTip.IsValid() )
		{
			StableActiveToolTip->OnClosed();
		}

		// Hide the tool tip window.  We don't destroy the window, because we want to reuse it for future tool tips.
		PinnedToolTipWindow->HideWindow();

		ActiveToolTip.Reset();
		ActiveToolTipWidgetSource.Reset();
	}
	ToolTipOffsetDirection = EToolTipOffsetDirection::Undetermined;
}

void FSlateApplication::UpdateToolTip( bool AllowSpawningOfNewToolTips )
{
	const bool bCheckForToolTipChanges =
		bAllowToolTips &&					// Tool-tips must be enabled
		!IsUsingHighPrecisionMouseMovment() && // If we are using HighPrecision movement then we can't rely on the OS cursor to be accurate
		!IsDragDropping();					// We must not currently be in the middle of a drag-drop action
	
	// We still want to show tooltips for widgets that are disabled
	const bool bIgnoreEnabledStatus = true;

	FWidgetPath WidgetsToQueryForToolTip;
	// We don't show any tooltips when drag and dropping or when another app is active
	if (bCheckForToolTipChanges)
	{
		// Ask each widget under the Mouse if they have a tool tip to show.
		FWidgetPath WidgetsUnderMouse = LocateWindowUnderMouse( GetCursorPos(), GetInteractiveTopLevelWindows(), bIgnoreEnabledStatus );
		// Don't attempt to show tooltips inside an existing tooltip
		if (!WidgetsUnderMouse.IsValid() || WidgetsUnderMouse.GetWindow() != ToolTipWindow.Pin())
		{
			WidgetsToQueryForToolTip = WidgetsUnderMouse;
		}
	}

	bool bHaveForceFieldRect = false;
	FSlateRect ForceFieldRect;

	TSharedPtr<IToolTip> NewToolTip;
	TSharedPtr<SWidget> WidgetProvidingNewToolTip;
	for ( int32 WidgetIndex=WidgetsToQueryForToolTip.Widgets.Num()-1; WidgetIndex >= 0; --WidgetIndex )
	{
		FArrangedWidget* CurWidgetGeometry = &WidgetsToQueryForToolTip.Widgets[WidgetIndex];
		const TSharedRef<SWidget>& CurWidget = CurWidgetGeometry->Widget;

		if( !NewToolTip.IsValid() )
		{
			TSharedPtr< IToolTip > WidgetToolTip = CurWidget->GetToolTip();

			// Make sure the tool-tip currently is displaying something before spawning it.
			if( WidgetToolTip.IsValid() && !WidgetToolTip->IsEmpty() )
			{
				WidgetProvidingNewToolTip = CurWidget;
				NewToolTip = WidgetToolTip;
			}
		}

		// Keep track of the root most widget with a tool-tip force field enabled
		if( CurWidget->HasToolTipForceField() )
		{
			if( !bHaveForceFieldRect )
			{
				bHaveForceFieldRect = true;
				ForceFieldRect = CurWidgetGeometry->Geometry.GetClippingRect();				
			}
			else
			{
				// Grow the rect to encompass this geometry.  Usually, the parent's rect should always be inclusive
				// of it's child though.  Just is kind of just being paranoid.
				ForceFieldRect = ForceFieldRect.Expand( CurWidgetGeometry->Geometry.GetClippingRect() );
			}
		}
	}

	// Did the tool tip change from last time?
	const bool bToolTipChanged = (NewToolTip != ActiveToolTip.Pin());
	
	// Any widgets that wish to handle visualizing the tooltip get a chance here.
	TSharedPtr<SWidget> NewTooltipVisualizer;
	if (bToolTipChanged)
	{
		// Remove existing tooltip if there is one.
		if (TooltipVisualizerPtr.IsValid())
		{
			TooltipVisualizerPtr.Pin()->OnVisualizeTooltip( nullptr );
		}

		// Notify the new tooltip that it's about to be opened.
		if ( NewToolTip.IsValid() )
		{
			NewToolTip->OnOpening();
		}

		TSharedPtr<SWidget> NewToolTipWidget = NewToolTip.IsValid() ? NewToolTip->AsWidget() : TSharedPtr<SWidget>();

		bool bOnVisualizeTooltipHandled = false;
		// Some widgets might want to provide an alternative Tooltip Handler.
		for ( int32 WidgetIndex=WidgetsToQueryForToolTip.Widgets.Num()-1; !bOnVisualizeTooltipHandled && WidgetIndex >= 0; --WidgetIndex )
		{
			const FArrangedWidget& CurWidgetGeometry = WidgetsToQueryForToolTip.Widgets[WidgetIndex];
			bOnVisualizeTooltipHandled = CurWidgetGeometry.Widget->OnVisualizeTooltip( NewToolTipWidget );
			if (bOnVisualizeTooltipHandled)
			{
				// Someone is taking care of visualizing this tooltip
				NewTooltipVisualizer = CurWidgetGeometry.Widget;
			}
		}
	}


	// If a widget under the cursor has a tool-tip forcefield active, then go through any menus
	// in the menu stack that are above that widget's window, and make sure those windows also
	// prevent the tool-tip from encroaching.  This prevents tool-tips from drawing over sub-menus
	// spawned from menu items in a different window, for example.
	if( bHaveForceFieldRect && WidgetsToQueryForToolTip.IsValid() )
	{
		const int32 MenuStackLevel = MenuStack.FindLocationInStack( WidgetsToQueryForToolTip.GetWindow() );

		// Also check widgets in pop-up menus owned by this window
		for( int32 CurStackLevel = MenuStackLevel + 1; CurStackLevel < MenuStack.GetNumStackLevels(); ++CurStackLevel )
		{
			FMenuWindowList& Windows = MenuStack.GetWindowsAtStackLevel( CurStackLevel );

			for( FMenuWindowList::TConstIterator WindowIt( Windows ); WindowIt; ++WindowIt )
			{
				TSharedPtr< SWindow > CurWindow = *WindowIt;
				if( CurWindow.IsValid() )
				{
					const FGeometry& WindowGeometry = CurWindow->GetWindowGeometryInScreen();
					ForceFieldRect = ForceFieldRect.Expand( WindowGeometry.GetClippingRect() );
				}
			}
		}
	}

	{
		TSharedPtr<IToolTip> ActiveToolTipPtr = ActiveToolTip.Pin();
		if ( ( ActiveToolTipPtr.IsValid() && !ActiveToolTipPtr->IsInteractive() ) || ( NewToolTip.IsValid() && NewToolTip != ActiveToolTip.Pin() ) )
		{
			// Keep track of where we want tool tips to be positioned
			DesiredToolTipLocation = GetLastCursorPos() + SlateDefs::ToolTipOffsetFromMouse;
		}
	}

	TSharedPtr< SWindow > ToolTipWindowPtr = ToolTipWindow.Pin();
	if ( ToolTipWindowPtr.IsValid() )
	{
		FSlateRect Anchor(DesiredToolTipLocation.X, DesiredToolTipLocation.Y, DesiredToolTipLocation.X, DesiredToolTipLocation.Y);
		DesiredToolTipLocation = CalculatePopupWindowPosition( Anchor, ToolTipWindowPtr->GetDesiredSize() );
	}

	// Repel tool-tip from a force field, if necessary
	if( bHaveForceFieldRect )
	{
		FVector2D ToolTipShift;
		ToolTipShift.X = ( ForceFieldRect.Right + SlateDefs::ToolTipOffsetFromForceField.X ) - DesiredToolTipLocation.X;
		ToolTipShift.Y = ( ForceFieldRect.Bottom + SlateDefs::ToolTipOffsetFromForceField.Y ) - DesiredToolTipLocation.Y;

		// Make sure the tool-tip needs to be offset
		if( ToolTipShift.X > 0.0f && ToolTipShift.Y > 0.0f )
		{
			// Find the best edge to move the tool-tip towards
			if( ToolTipOffsetDirection == EToolTipOffsetDirection::Right ||
				( ToolTipOffsetDirection == EToolTipOffsetDirection::Undetermined && ToolTipShift.X < ToolTipShift.Y ) )
			{
				// Move right
				DesiredToolTipLocation.X += ToolTipShift.X;
				ToolTipOffsetDirection = EToolTipOffsetDirection::Right;
			}
			else
			{
				// Move down
				DesiredToolTipLocation.Y += ToolTipShift.Y;
				ToolTipOffsetDirection = EToolTipOffsetDirection::Down;
			}
		}
	}

	// The tool tip changed...
	if ( bToolTipChanged )
	{
		// Close any existing tooltips; Unless the current tooltip is interactive and we don't have a valid tooltip to replace it
		TSharedPtr<IToolTip> ActiveToolTipPtr = ActiveToolTip.Pin();
		if ( NewToolTip.IsValid() || ( ActiveToolTipPtr.IsValid() && !ActiveToolTipPtr->IsInteractive() ) )
		{
			CloseToolTip();
			
			if (NewTooltipVisualizer.IsValid())
			{
				TooltipVisualizerPtr = NewTooltipVisualizer;
			}
			else if( bAllowToolTips && AllowSpawningOfNewToolTips )
			{
				// Spawn a new one if we have it
				if( NewToolTip.IsValid() )
				{
					SpawnToolTip( NewToolTip.ToSharedRef(), DesiredToolTipLocation );
				}
			}
			else
			{
				NewToolTip = nullptr;
			}

			ActiveToolTip = NewToolTip;
			ActiveToolTipWidgetSource = WidgetProvidingNewToolTip;
		}
	}

	// Do we have a tool tip window?
	if( ToolTipWindow.IsValid() )
	{
		// Only enable tool-tip transitions if we're running at a decent frame rate
		const bool bAllowInstantToolTips = false;
		const bool bAllowAnimations = !bAllowInstantToolTips && FSlateApplication::Get().IsRunningAtTargetFrameRate();

		// How long since the tool tip was summoned?
		const float TimeSinceSummon = FPlatformTime::Seconds() - ToolTipDelay - ToolTipSummonTime;
		const float ToolTipOpacity = bAllowInstantToolTips ? 1.0f : FMath::Clamp< float >( TimeSinceSummon / ToolTipFadeInDuration, 0.0f, 1.0f );

		// Update window opacity
		TSharedRef< SWindow > PinnedToolTipWindow( ToolTipWindow.Pin().ToSharedRef() );
		PinnedToolTipWindow->SetOpacity( ToolTipOpacity );

		// How far tool tips should slide
		const FVector2D SlideDistance( 30.0f, 5.0f );

		// Apply steep inbound curve to the movement, so it looks like it quickly decelerating
		const float SlideProgress = bAllowAnimations ? FMath::Pow( 1.0f - ToolTipOpacity, 3.0f ) : 0.0f;

		FVector2D WindowLocation = DesiredToolTipLocation + SlideProgress * SlideDistance;
		if( WindowLocation != PinnedToolTipWindow->GetPositionInScreen() )
		{
			// Avoid the edges of the desktop
			FSlateRect Anchor(WindowLocation.X, WindowLocation.Y, WindowLocation.X, WindowLocation.Y);
			WindowLocation = CalculatePopupWindowPosition( Anchor, PinnedToolTipWindow->GetDesiredSize() );

			// Update the tool tip window positioning
			// SetCachedScreenPosition is a hack (issue tracked as TTP #347070) which is needed because code in TickWindowAndChildren()/DrawPrepass()
			// assumes GetPositionInScreen() to correspond to the new window location in the same tick. This is true on Windows, but other
			// OSes (Linux in particular) may not update cached screen position until next time events are polled.
			PinnedToolTipWindow->SetCachedScreenPosition( WindowLocation );
			PinnedToolTipWindow->MoveWindowTo( WindowLocation );
		}
	}
}

TArray< TSharedRef<SWindow> > FSlateApplication::GetInteractiveTopLevelWindows()
{
	if (ActiveModalWindows.Num() > 0)
	{
		// If we have modal windows, only the topmost modal window and its children are interactive.
		TArray< TSharedRef<SWindow>, TInlineAllocator<1> > OutWindows;
		OutWindows.Add( ActiveModalWindows.Last().ToSharedRef() );
		return TArray< TSharedRef<SWindow> >(OutWindows);
	}
	else
	{
		// No modal windows? All windows are interactive.
		return SlateWindows;
	}
}

void FSlateApplication::GetAllVisibleWindowsOrdered(TArray< TSharedRef<SWindow> >& OutWindows)
{
	for( TArray< TSharedRef<SWindow> >::TConstIterator CurrentWindowIt( SlateWindows ); CurrentWindowIt; ++CurrentWindowIt )
	{
		TSharedRef<SWindow> CurrentWindow = *CurrentWindowIt;
		if ( CurrentWindow->IsVisible() )
		{
			GetAllVisibleChildWindows(OutWindows, CurrentWindow);
		}
	}
}

void FSlateApplication::GetAllVisibleChildWindows(TArray< TSharedRef<SWindow> >& OutWindows, TSharedRef<SWindow> CurrentWindow)
{
	if ( CurrentWindow->IsVisible() )
	{
		OutWindows.Add(CurrentWindow);

		const TArray< TSharedRef<SWindow> >& WindowChildren = CurrentWindow->GetChildWindows();
		for (int32 ChildIndex=0; ChildIndex < WindowChildren.Num(); ++ChildIndex)
		{
			GetAllVisibleChildWindows( OutWindows, WindowChildren[ChildIndex] );
		}
	}
}

bool FSlateApplication::IsDragDropping() const
{
	return DragDropContent.IsValid();
}

TSharedPtr<FDragDropOperation> FSlateApplication::GetDragDroppingContent() const
{
	return DragDropContent;
}

void FSlateApplication::CancelDragDrop()
{
	FWidgetPath WidgetsToDragLeave = WidgetsUnderCursorLastEvent.ToWidgetPath(FWeakWidgetPath::EInterruptedPathHandling::Truncate);
	if (WidgetsToDragLeave.IsValid())
	{
		const FDragDropEvent DragDropEvent(FPointerEvent(), DragDropContent);
		for (int32 WidgetIndex = WidgetsToDragLeave.Widgets.Num() - 1; WidgetIndex >= 0; --WidgetIndex)
		{
			WidgetsToDragLeave.Widgets[WidgetIndex].Widget->OnDragLeave(DragDropEvent);
		}
	}

	WidgetsUnderCursorLastEvent = FWeakWidgetPath();
	DragDropContent.Reset();
}

void FSlateApplication::EnterDebuggingMode()
{
	bRequestLeaveDebugMode = false;

	// Note it is ok to hold a reference here as the game viewport should not be destroyed while in debugging mode
	TSharedPtr<SViewport> PreviousGameViewport;

	// Disable any game viewports while we are in debug mode so that mouse capture is released and the cursor is visible
	if (GameViewportWidget.IsValid())
	{
		PreviousGameViewport = GameViewportWidget.Pin();
		UnregisterGameViewport();
	}

	Renderer->FlushCommands();
	
	// We are about to start an in stack tick. Make sure the rendering thread isn't already behind
	Renderer->Sync();

#if WITH_EDITORONLY_DATA
	// Flag that we're about to enter the first frame of intra-frame debugging.
	GFirstFrameIntraFrameDebugging = true;
#endif	//WITH_EDITORONLY_DATA


	// The scissor rect stack must be reset when re-entering the tick loop to avoid graphical artifacts with existing clip rects applied new widgets
	TOptional<FShortRect> PreviousScissorRect = GSlateScissorRect;
	GSlateScissorRect.Reset();

	// Tick slate from here in the event that we should not return until the modal window is closed.
	while (!bRequestLeaveDebugMode)
	{
		// Tick and render Slate
		Tick();

		// Synchronize the game thread and the render thread so that the render thread doesn't get too far behind.
		Renderer->Sync();

#if WITH_EDITORONLY_DATA
		// We are done with the first frame
		GFirstFrameIntraFrameDebugging = false;

		// If we are requesting leaving debugging mode, leave it now.
		GIntraFrameDebuggingGameThread = !bRequestLeaveDebugMode;
#endif	//WITH_EDITORONLY_DATA
	}

	bRequestLeaveDebugMode = false;
	
	if ( PreviousGameViewport.IsValid() )
	{
		check(!GameViewportWidget.IsValid());

		// When in single step mode, register the game viewport so we can unregister it later
		// but do not do any of the other stuff like locking or capturing the mouse.
		if( bLeaveDebugForSingleStep )
		{
			GameViewportWidget = PreviousGameViewport;
		}
		else
		{
			// If we had a game viewport before debugging, re-register it now to capture the mouse and lock the cursor
			RegisterGameViewport( PreviousGameViewport.ToSharedRef() );
		}
	}

	// Reset the scissor rect back to what it was before we started debugging
	GSlateScissorRect = PreviousScissorRect;

	bLeaveDebugForSingleStep = false;
}

void FSlateApplication::LeaveDebuggingMode(  bool bLeavingForSingleStep )
{
	bRequestLeaveDebugMode = true;
	bLeaveDebugForSingleStep = bLeavingForSingleStep;
}

bool FSlateApplication::IsWindowInDestroyQueue(TSharedRef<SWindow> Window) const
{
	return WindowDestroyQueue.Contains(Window);
}

void FSlateApplication::SynthesizeMouseMove()
{
	SLATE_CYCLE_COUNTER_SCOPE(GSlateSynthesizeMouseMove);
	if (PlatformApplication->Cursor.IsValid())
	{
		// Synthetic mouse events accomplish two goals:
		// 1) The UI can change even if the mosue doesn't move.
		//    Synthesizing a mouse move sends out events.
		//    In this case, the current and previous position will be the same.
		//
		// 2) The mouse moves, but the OS decided not to send us an event.
		//    e.g. Mouse moved outside of our window.
		//    In this case, the previous and current positions differ.

		FPointerEvent MouseEvent
		(
			CursorPointerIndex,
			GetCursorPos(),
			GetLastCursorPos(),
			PressedMouseButtons,
			EKeys::Invalid,
			0,
			PlatformApplication->GetModifierKeys()
		);

		ProcessMouseMoveEvent(MouseEvent, true);
	}
}

void FSlateApplication::QueueSynthesizedMouseMove()
{
	SynthesizeMouseMovePending = 2;
}

void FSlateApplication::OnLogSlateEvent(EEventLog::Type Event, const FString& AdditionalContent)
{
#if LOG_SLATE_EVENTS
	if (EventLogger.IsValid())
	{
		LOG_EVENT_CONTENT(Event, AdditionalContent, nullptr);
	}
#endif
}

void FSlateApplication::OnLogSlateEvent(EEventLog::Type Event, const FText& AdditionalContent )
{
#if LOG_SLATE_EVENTS
	if (EventLogger.IsValid())
	{
		LOG_EVENT_CONTENT(Event, AdditionalContent.ToString(), nullptr);
	}
#endif
};

void FSlateApplication::SetSlateUILogger(TSharedPtr<IEventLogger> InEventLogger)
{
#if LOG_SLATE_EVENTS
	EventLogger = InEventLogger;
#endif
}

void FSlateApplication::SetUnhandledKeyDownEventHandler( const FOnKeyEvent& NewHandler )
{
	UnhandledKeyDownEventHandler = NewHandler;
}

float FSlateApplication::GetDragTriggerDistance() const
{
	return DragTriggerDistance;
}

void FSlateApplication::SetDragTriggerDistance( float ScreenPixels )
{
	DragTriggerDistance = ScreenPixels;
}

void FSlateApplication::SetInputPreProcessor(bool bEnable, TSharedPtr<class IInputProcessor> NewInputProcessor)
{
	if (bEnable && NewInputProcessor.IsValid())
	{
		InputPreProcessor = NewInputProcessor;
	}
	else
	{
		InputPreProcessor.Reset();
	}
}

void FSlateApplication::SetCursorRadius(float NewRadius)
{
	CursorRadius = FMath::Max<float>(0.0f, NewRadius);
}

float FSlateApplication::GetCursorRadius() const
{
	return CursorRadius;
}

FVector2D FSlateApplication::CalculatePopupWindowPosition( const FSlateRect& InAnchor, const FVector2D& InSize, const EOrientation Orientation ) const
{
	FVector2D CalculatedPopUpWindowPosition( 0, 0 );

	FPlatformRect AnchorRect;
	AnchorRect.Left = InAnchor.Left;
	AnchorRect.Top = InAnchor.Top;
	AnchorRect.Right = InAnchor.Right;
	AnchorRect.Bottom = InAnchor.Bottom;

	EPopUpOrientation::Type PopUpOrientation = EPopUpOrientation::Horizontal;

	if ( Orientation == EOrientation::Orient_Vertical )
	{
		PopUpOrientation =  EPopUpOrientation::Vertical;
	}

	if ( PlatformApplication->TryCalculatePopupWindowPosition( AnchorRect, InSize, PopUpOrientation, /*OUT*/&CalculatedPopUpWindowPosition ) )
	{
		return CalculatedPopUpWindowPosition;
	}
	else
	{
		// Calculate the rectangle around our work area
		// Use our own rect.  This window as probably doesn't have a size or position yet.
		// Use a size of 1 to get the closest monitor to the start point
		FPlatformRect WorkAreaFinderRect(AnchorRect);
		WorkAreaFinderRect.Left = AnchorRect.Left + 1;
		WorkAreaFinderRect.Top = AnchorRect.Top + 1;
		const FPlatformRect PlatformWorkArea = PlatformApplication->GetWorkArea(WorkAreaFinderRect);

		const FSlateRect WorkAreaRect( 
			PlatformWorkArea.Left, 
			PlatformWorkArea.Top, 
			PlatformWorkArea.Left+(PlatformWorkArea.Right - PlatformWorkArea.Left), 
			PlatformWorkArea.Top+(PlatformWorkArea.Bottom - PlatformWorkArea.Top) );

		// Assume natural left-to-right, top-to-bottom flow; position popup below and to the right.
		const FVector2D ProposedPlacement(
			Orientation == Orient_Horizontal ? AnchorRect.Right : AnchorRect.Left,
			Orientation == Orient_Horizontal ? AnchorRect.Top : AnchorRect.Bottom);

		return ComputePopupFitInRect(InAnchor, FSlateRect(ProposedPlacement, ProposedPlacement+InSize), Orientation, WorkAreaRect);
	}
}

bool FSlateApplication::IsRunningAtTargetFrameRate() const
{
	const float MinimumDeltaTime = 1.0f / TargetFrameRateForResponsiveness.GetValueOnGameThread();
	return ( AverageDeltaTimeForResponsiveness <= MinimumDeltaTime ) || !IsNormalExecution();
}


bool FSlateApplication::AreMenuAnimationsEnabled() const
{
	return bMenuAnimationsEnabled;
}


void FSlateApplication::EnableMenuAnimations( const bool bEnableAnimations )
{
	bMenuAnimationsEnabled = bEnableAnimations;
}


void FSlateApplication::SetAppIcon(const FSlateBrush* const InAppIcon)
{
	check(InAppIcon);
	AppIcon = InAppIcon;
}


const FSlateBrush* FSlateApplication::GetAppIcon() const
{
	return AppIcon;
}


void FSlateApplication::ShowVirtualKeyboard( bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget )
{
	if(SlateTextField == nullptr)
	{
		SlateTextField = new FPlatformTextField();
	}

	SlateTextField->ShowVirtualKeyboard(bShow, UserIndex, TextEntryWidget);
}

FSlateRect FSlateApplication::GetPreferredWorkArea() const
{
	const FWeakWidgetPath & FocusedWidgetPath = UserFocusEntries[GetUserIndexForKeyboard()].WidgetPath;

	// First see if we have a focused widget
	if( FocusedWidgetPath.IsValid() && FocusedWidgetPath.Window.IsValid() )
	{
		const FVector2D WindowPos = FocusedWidgetPath.Window.Pin()->GetPositionInScreen();
		const FVector2D WindowSize = FocusedWidgetPath.Window.Pin()->GetSizeInScreen();
		return GetWorkArea( FSlateRect( WindowPos.X, WindowPos.Y, WindowPos.X + WindowSize.X, WindowPos.Y + WindowSize.Y ) );
	}
	else
	{
		// no focus widget, so use mouse position if there are windows present in the work area
		const FVector2D CursorPos = GetCursorPos();
		const FSlateRect WorkArea = GetWorkArea( FSlateRect( CursorPos.X, CursorPos.Y, CursorPos.X + 1.0f, CursorPos.Y + 1.0f ) );

		if (FSlateWindowHelper::CheckWorkAreaForWindows(SlateWindows, WorkArea))
		{
			return WorkArea;
		}

		// If we can't find a window where the cursor is at, try finding a main window.
		TSharedPtr<SWindow> ActiveTop = GetActiveTopLevelWindow();
		if ( ActiveTop.IsValid() )
		{
			// Use the current top level windows rect
			return GetWorkArea( ActiveTop->GetRectInScreen() );
		}
		
		// If we can't find a top level window check for an active modal window
		TSharedPtr<SWindow> ActiveModal = GetActiveModalWindow();
		if ( ActiveModal.IsValid() )
		{
			// Use the current active modal windows rect
			return GetWorkArea( ActiveModal->GetRectInScreen() );
		}

		// no windows on work area - default to primary display
		FDisplayMetrics DisplayMetrics;
		GetDisplayMetrics( DisplayMetrics );
		const FPlatformRect& DisplayRect = DisplayMetrics.PrimaryDisplayWorkAreaRect;
		return FSlateRect( (float)DisplayRect.Left, (float)DisplayRect.Top, (float)DisplayRect.Right, (float)DisplayRect.Bottom );
	}
}

FSlateRect FSlateApplication::GetWorkArea( const FSlateRect& InRect ) const
{
	FPlatformRect InPlatformRect;
	InPlatformRect.Left = FMath::TruncToInt(InRect.Left);
	InPlatformRect.Top = FMath::TruncToInt(InRect.Top);
	InPlatformRect.Right = FMath::TruncToInt(InRect.Right);
	InPlatformRect.Bottom = FMath::TruncToInt(InRect.Bottom);

	const FPlatformRect OutPlatformRect = PlatformApplication->GetWorkArea( InPlatformRect );
	return FSlateRect( OutPlatformRect.Left, OutPlatformRect.Top, OutPlatformRect.Right, OutPlatformRect.Bottom );
}

bool FSlateApplication::SupportsSourceAccess() const
{
	if(QuerySourceCodeAccessDelegate.IsBound())
	{
		return QuerySourceCodeAccessDelegate.Execute();
	}
	return false;
}

void FSlateApplication::GotoLineInSource(const FString& FileName, int32 LineNumber) const
{
	if ( SupportsSourceAccess() )
	{
		if(SourceCodeAccessDelegate.IsBound())
		{
			SourceCodeAccessDelegate.Execute(FileName, LineNumber, 0);
		}
	}
}

void FSlateApplication::ForceRedrawWindow(TSharedRef<SWindow>& InWindowToDraw)
{
	PrivateDrawWindows( InWindowToDraw );
}

bool FSlateApplication::TakeScreenshot(TSharedRef<SWidget>& Widget, TArray<FColor>&OutColorData, FIntVector& OutSize)
{
	return TakeScreenshot(Widget, FIntRect(), OutColorData, OutSize);
}

bool FSlateApplication::TakeScreenshot(TSharedRef<SWidget>& Widget, const FIntRect& InnerWidgetArea, TArray<FColor>& OutColorData, FIntVector& OutSize)
{
	// We can't screenshot the widget unless there's a valid window handle to draw it in.
	TSharedPtr<SWindow> WidgetWindow = FSlateApplication::Get().FindWidgetWindow(Widget);
	if ( !WidgetWindow.IsValid() )
	{
		return false;
	}

	TSharedRef<SWindow> CurrentWindowRef = WidgetWindow.ToSharedRef();

	FWidgetPath WidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetChecked(Widget, WidgetPath);

	FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(Widget).Get(FArrangedWidget::NullWidget);
	FVector2D Position = ArrangedWidget.Geometry.AbsolutePosition;
	FVector2D Size = ArrangedWidget.Geometry.GetDrawSize();
	FVector2D WindowPosition = WidgetWindow->GetPositionInScreen();

	FIntRect ScreenshotRect = InnerWidgetArea.IsEmpty() ? FIntRect(0, 0, (int32)Size.X, (int32)Size.Y) : InnerWidgetArea;

	ScreenshotRect.Min.X += ( Position.X - WindowPosition.X );
	ScreenshotRect.Min.Y += ( Position.Y - WindowPosition.Y );
	ScreenshotRect.Max.X += ( Position.X - WindowPosition.X );
	ScreenshotRect.Max.Y += ( Position.Y - WindowPosition.Y );

	Renderer->PrepareToTakeScreenshot(ScreenshotRect, &OutColorData);
	PrivateDrawWindows(WidgetWindow);

	OutSize.X = ScreenshotRect.Size().X;
	OutSize.Y = ScreenshotRect.Size().Y;

	return true;
}


/* FSlateApplicationBase interface
 *****************************************************************************/

FVector2D FSlateApplication::GetCursorSize( ) const
{
	if ( PlatformApplication->Cursor.IsValid() )
	{
		int32 X;
		int32 Y;
		PlatformApplication->Cursor->GetSize( X, Y );
		return FVector2D( X, Y );
	}

	return FVector2D( 1.0f, 1.0f );
}

EVisibility FSlateApplication::GetSoftwareCursorVis( ) const
{
	const TSharedPtr<ICursor>& Cursor = PlatformApplication->Cursor;
	if (bSoftwareCursorAvailable && Cursor.IsValid() && Cursor->GetType() != EMouseCursor::None)
	{
		return EVisibility::HitTestInvisible;
	}
	return EVisibility::Hidden;
}

TSharedPtr< SWidget > FSlateApplication::GetKeyboardFocusedWidget() const
{
	const FUserFocusEntry& UserFocusEntry = UserFocusEntries[GetUserIndexForKeyboard()];
	if (UserFocusEntry.WidgetPath.IsValid())
	{
		return UserFocusEntry.WidgetPath.GetLastWidget().Pin();
	}

	return TSharedPtr< SWidget >();
}

TSharedPtr<SWidget> FSlateApplication::GetMouseCaptorImpl() const
{
	return MouseCaptor.ToSharedWidget(CursorPointerIndex);
}

bool FSlateApplication::HasAnyMouseCaptor() const
{
	return MouseCaptor.HasCapture();
}

bool FSlateApplication::HasMouseCapture(const TSharedPtr<const SWidget> Widget) const
{
	for (auto CaptureWidget : MouseCaptor.ToSharedWidgets())
	{
		if (Widget == CaptureWidget)
		{
			return true;
		}
	}
	return false;
}

TOptional<EFocusCause> FSlateApplication::HasUserFocus(const TSharedPtr<const SWidget> Widget, int32 UserIndex) const
{
	const FUserFocusEntry& UserFocusEntry = UserFocusEntries[UserIndex];
	const FWeakWidgetPath & FocusedWidgetPath = UserFocusEntry.WidgetPath;
	if (FocusedWidgetPath.IsValid() && FocusedWidgetPath.GetLastWidget().Pin() == Widget)
	{
		TOptional<EFocusCause> FocusReason(UserFocusEntry.FocusCause);
		return FocusReason;
	}
	return TOptional<EFocusCause>();
}

TOptional<EFocusCause> FSlateApplication::HasAnyUserFocus(const TSharedPtr<const SWidget> Widget) const
{
	for (int32 UserIndex = 0; UserIndex < SlateApplicationDefs::MaxUsers; ++UserIndex)
	{
		const FUserFocusEntry& UserFocusEntry = UserFocusEntries[UserIndex];
		const FWeakWidgetPath & FocusedWidgetPath = UserFocusEntry.WidgetPath;
		if (FocusedWidgetPath.IsValid() && FocusedWidgetPath.GetLastWidget().Pin() == Widget)
		{
			TOptional<EFocusCause> FocusReason(UserFocusEntry.FocusCause);
			return FocusReason;
		}
	}
	return TOptional<EFocusCause>();
}

bool FSlateApplication::ShowUserFocus(const TSharedPtr<const SWidget> Widget) const
{
	for (int32 UserIndex = 0; UserIndex < SlateApplicationDefs::MaxUsers; ++UserIndex)
	{
		const FUserFocusEntry& UserFocusEntry = UserFocusEntries[UserIndex];
		const FWeakWidgetPath & FocusedWidgetPath = UserFocusEntry.WidgetPath;
		if (FocusedWidgetPath.IsValid() && FocusedWidgetPath.GetLastWidget().Pin() == Widget)
		{
			return UserFocusEntry.ShowFocus;
		}
	}

	return false;
}

bool FSlateApplication::HasFocusedDescendants( const TSharedRef< const SWidget >& Widget ) const
{
	for (int32 UserIndex = 0; UserIndex < SlateApplicationDefs::MaxUsers; ++UserIndex)
	{
		const FUserFocusEntry& UserFocusEntry = UserFocusEntries[UserIndex];
		const FWeakWidgetPath & FocusedWidgetPath = UserFocusEntry.WidgetPath;
		if (FocusedWidgetPath.IsValid() && FocusedWidgetPath.GetLastWidget().Pin() != Widget && FocusedWidgetPath.ContainsWidget(Widget))
		{
			return true;
		}
	}
	return false;
}


bool FSlateApplication::IsExternalUIOpened()
{
	return bIsExternalUIOpened;
}


TSharedRef<SWidget> FSlateApplication::MakeImage( const TAttribute<const FSlateBrush*>& Image, const TAttribute<FSlateColor>& Color, const TAttribute<EVisibility>& Visibility ) const
{
	return SNew(SImage)
		.ColorAndOpacity(Color)
		.Image(Image)
		.Visibility(Visibility);
}


TSharedRef<SWidget> FSlateApplication::MakeWindowTitleBar( const TSharedRef<SWindow>& Window, const TSharedPtr<SWidget>& CenterContent, EHorizontalAlignment CenterContentAlignment, TSharedPtr<IWindowTitleBar>& OutTitleBar ) const
{
	TSharedRef<SWindowTitleBar> TitleBar = SNew(SWindowTitleBar, Window, CenterContent, CenterContentAlignment)
		.Visibility(EVisibility::SelfHitTestInvisible);

	OutTitleBar = TitleBar;

	return TitleBar;
}


TSharedRef<IToolTip> FSlateApplication::MakeToolTip( const TAttribute<FString>& ToolTipString )
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	return SNew(SToolTip)
		.Text(ToolTipString);

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}


TSharedRef<IToolTip> FSlateApplication::MakeToolTip(const TAttribute<FText>& ToolTipText)
{
	return SNew(SToolTip)
		.Text(ToolTipText);
}


TSharedRef<IToolTip> FSlateApplication::MakeToolTip( const FText& ToolTipText )
{
	return SNew(SToolTip)
		.Text(ToolTipText);
}


/* FGenericApplicationMessageHandler interface
 *****************************************************************************/

bool FSlateApplication::ShouldProcessUserInputMessages( const TSharedPtr< FGenericWindow >& PlatformWindow ) const
{
	TSharedPtr< SWindow > Window;
	if ( PlatformWindow.IsValid() )
	{
		Window = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, PlatformWindow.ToSharedRef() );
	}

	if ( ActiveModalWindows.Num() == 0 || 
		( Window.IsValid() &&
			( Window->IsDescendantOf( GetActiveModalWindow() ) || ActiveModalWindows.Contains( Window ) ) ) )
	{
		return true;
	}
	return false;
}

bool FSlateApplication::OnKeyChar( const TCHAR Character, const bool IsRepeat )
{
	FCharacterEvent CharacterEvent( Character, PlatformApplication->GetModifierKeys(), 0, IsRepeat );
	return ProcessKeyCharEvent( CharacterEvent );
}

bool FSlateApplication::ProcessKeyCharEvent( FCharacterEvent& InCharacterEvent )
{
	FReply Reply = FReply::Unhandled();

	// NOTE: We intentionally don't reset LastUserInteractionTimeForThrottling here so that the UI can be responsive while typing

	// Bubble the key event
	FWidgetPath EventPath = UserFocusEntries[InCharacterEvent.GetUserIndex()].WidgetPath.ToWidgetPath();
	
	// Switch worlds for widgets in the current path
	FScopedSwitchWorldHack SwitchWorld( EventPath );

	Reply = FEventRouter::RouteAlongFocusPath( this, FEventRouter::FBubblePolicy(EventPath), InCharacterEvent, []( const FArrangedWidget& SomeWidgetGettingEvent, const FCharacterEvent& Event )
	{
		return SomeWidgetGettingEvent.Widget->IsEnabled()
			? SomeWidgetGettingEvent.Widget->OnKeyChar( SomeWidgetGettingEvent.Geometry, Event )
			: FReply::Unhandled();
	});

	LOG_EVENT_CONTENT( EEventLog::KeyChar, FString::Printf(TEXT("%c"), InCharacterEvent.GetCharacter()), Reply );

	return Reply.IsEventHandled();
}

bool FSlateApplication::OnKeyDown( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat ) 
{
	FKey const Key = FInputKeyManager::Get().GetKeyFromCodes( KeyCode, CharacterCode );
	FKeyEvent KeyEvent(Key, PlatformApplication->GetModifierKeys(), GetUserIndexForKeyboard(), IsRepeat, CharacterCode, KeyCode);

	return ProcessKeyDownEvent( KeyEvent );
}

bool FSlateApplication::ProcessKeyDownEvent( FKeyEvent& InKeyEvent )
{
	QueueSynthesizedMouseMove();

	// Analog cursor gets first chance at the input
	if (InputPreProcessor.IsValid() && InputPreProcessor->HandleKeyDownEvent(*this, InKeyEvent))
	{
		return true;
	}

	FReply Reply = FReply::Unhandled();

	LastUserInteractionTime = this->GetCurrentTime();
	
	if (IsDragDropping() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		// Pressing ESC while drag and dropping terminates the drag drop.
		CancelDragDrop();
		Reply = FReply::Handled();
	}
	else
	{
		LastUserInteractionTimeForThrottling = LastUserInteractionTime;

		// If we are inspecting, pressing ESC exits inspection mode.
		if ( InKeyEvent.GetKey() == EKeys::Escape )
		{
			TSharedPtr<IWidgetReflector> WidgetReflector = WidgetReflectorPtr.Pin();
			const bool bIsWidgetReflectorPicking = WidgetReflector.IsValid() && WidgetReflector->IsInPickingMode();
			if ( bIsWidgetReflectorPicking )
			{
					WidgetReflector->OnWidgetPicked();
					Reply = FReply::Handled();

					return Reply.IsEventHandled();
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Ctrl+Shift+~ summons the Toolbox.
		if (InKeyEvent.GetKey() == EKeys::Tilde && InKeyEvent.IsControlDown() && InKeyEvent.IsShiftDown())
		{
			IToolboxModule* ToolboxModule = FModuleManager::LoadModulePtr<IToolboxModule>("Toolbox");
			if (ToolboxModule)
			{
				ToolboxModule->SummonToolbox();
			}
		}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		// Bubble the keyboard event
		FWidgetPath EventPath = UserFocusEntries[InKeyEvent.GetUserIndex()].WidgetPath.ToWidgetPath();

		// Switch worlds for widgets inOnPreviewMouseButtonDown the current path
		FScopedSwitchWorldHack SwitchWorld( EventPath );

		// Tunnel the keyboard event
		Reply = FEventRouter::RouteAlongFocusPath( this, FEventRouter::FTunnelPolicy(EventPath), InKeyEvent, []( const FArrangedWidget& CurrentWidget, const FKeyEvent& Event )
		{
			return (CurrentWidget.Widget->IsEnabled())
				? CurrentWidget.Widget->OnPreviewKeyDown( CurrentWidget.Geometry, Event )
				: FReply::Unhandled();
		});

		// Send out key down events.
		if (!Reply.IsEventHandled())
		{
			Reply = FEventRouter::RouteAlongFocusPath(this, FEventRouter::FBubblePolicy(EventPath), InKeyEvent, [](const FArrangedWidget& SomeWidgetGettingEvent, const FKeyEvent& Event)
			{
				return (SomeWidgetGettingEvent.Widget->IsEnabled())
					? SomeWidgetGettingEvent.Widget->OnKeyDown( SomeWidgetGettingEvent.Geometry, Event )
					: FReply::Unhandled();
			});
		}

		LOG_EVENT_CONTENT( EEventLog::KeyDown, GetKeyName(InKeyEvent.GetKey()).ToString(), Reply );

		// If the key event was not processed by any widget...
		if (!Reply.IsEventHandled() && UnhandledKeyDownEventHandler.IsBound())
		{
			Reply = UnhandledKeyDownEventHandler.Execute( InKeyEvent );
		}
	}

	return Reply.IsEventHandled();
}

bool FSlateApplication::OnKeyUp( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat )
{
	FKey const Key = FInputKeyManager::Get().GetKeyFromCodes( KeyCode, CharacterCode );
	FKeyEvent KeyEvent(Key, PlatformApplication->GetModifierKeys(), GetUserIndexForKeyboard(), IsRepeat, CharacterCode, KeyCode);

	return ProcessKeyUpEvent( KeyEvent );
}

bool FSlateApplication::ProcessKeyUpEvent( FKeyEvent& InKeyEvent )
{
	QueueSynthesizedMouseMove();

	// Analog cursor gets first chance at the input
	if (InputPreProcessor.IsValid() && InputPreProcessor->HandleKeyUpEvent(*this, InKeyEvent))
	{
		return true;
	}

	FReply Reply = FReply::Unhandled();

	LastUserInteractionTime = this->GetCurrentTime();
	
	LastUserInteractionTimeForThrottling = LastUserInteractionTime;

	// Bubble the key event
	FWidgetPath EventPath = UserFocusEntries[InKeyEvent.GetUserIndex()].WidgetPath.ToWidgetPath();

	// Switch worlds for widgets in the current path
	FScopedSwitchWorldHack SwitchWorld( EventPath );

	Reply = FEventRouter::RouteAlongFocusPath(this, FEventRouter::FBubblePolicy(EventPath), InKeyEvent, [](const FArrangedWidget& SomeWidgetGettingEvent, const FKeyEvent& Event)
	{
		return (SomeWidgetGettingEvent.Widget->IsEnabled())
			? SomeWidgetGettingEvent.Widget->OnKeyUp( SomeWidgetGettingEvent.Geometry, Event )
			: FReply::Unhandled();
	});

	LOG_EVENT_CONTENT( EEventLog::KeyUp, GetKeyName(InKeyEvent.GetKey()).ToString(), Reply );

	return Reply.IsEventHandled();
}

bool FSlateApplication::ProcessAnalogInputEvent(FAnalogInputEvent& InAnalogInputEvent)
{
	QueueSynthesizedMouseMove();

	// Analog cursor gets first chance at the input
	if (InputPreProcessor.IsValid() && InputPreProcessor->HandleAnalogInputEvent(*this, InAnalogInputEvent))
	{
		return true;
	}

	FReply Reply = FReply::Unhandled();

	LastUserInteractionTime = this->GetCurrentTime();

	LastUserInteractionTimeForThrottling = LastUserInteractionTime;

	// Bubble the key event
	FWidgetPath EventPath = UserFocusEntries[InAnalogInputEvent.GetUserIndex()].WidgetPath.ToWidgetPath();
	InAnalogInputEvent.SetEventPath(EventPath);

	// Switch worlds for widgets in the current path
	FScopedSwitchWorldHack SwitchWorld(EventPath);

	Reply = FEventRouter::RouteAlongFocusPath(this, FEventRouter::FBubblePolicy(EventPath), InAnalogInputEvent, [](const FArrangedWidget& SomeWidgetGettingEvent, const FAnalogInputEvent& Event)
	{
		return (SomeWidgetGettingEvent.Widget->IsEnabled())
			? SomeWidgetGettingEvent.Widget->OnAnalogValueChanged(SomeWidgetGettingEvent.Geometry, Event)
			: FReply::Unhandled();
	});

	LOG_EVENT_CONTENT(EEventLog::AnalogInput, GetKeyName(InAnalogInputEvent.GetKey()).ToString(), Reply);

	QueueSynthesizedMouseMove();

	return Reply.IsEventHandled();
}

FKey TranslateMouseButtonToKey( const EMouseButtons::Type Button )
{
	FKey Key = EKeys::Invalid;

	switch( Button )
	{
	case EMouseButtons::Left:
		Key = EKeys::LeftMouseButton;
		break;
	case EMouseButtons::Middle:
		Key = EKeys::MiddleMouseButton;
		break;
	case EMouseButtons::Right:
		Key = EKeys::RightMouseButton;
		break;
	case EMouseButtons::Thumb01:
		Key = EKeys::ThumbMouseButton;
		break;
	case EMouseButtons::Thumb02:
		Key = EKeys::ThumbMouseButton2;
		break;
	}

	return Key;
}

void FSlateApplication::SetGameIsFakingTouchEvents(const bool bIsFaking, FVector2D* CursorLocation)
{
	if (bIsFakingTouched && !bIsFaking && bIsGameFakingTouch && !bIsFakingTouch)
	{
		OnTouchEnded((CursorLocation ? *CursorLocation : PlatformApplication->Cursor->GetPosition()), 0, 0);
	}
	bIsGameFakingTouch = bIsFaking;
}

bool FSlateApplication::IsFakingTouchEvents() const
{
	return bIsFakingTouch || bIsGameFakingTouch;
}


bool FSlateApplication::OnMouseDown( const TSharedPtr< FGenericWindow >& PlatformWindow, const EMouseButtons::Type Button )
{
	// convert to touch event if we are faking it	
	if (bIsFakingTouch || bIsGameFakingTouch)
	{
		bIsFakingTouched = true;
		return OnTouchStarted( PlatformWindow, PlatformApplication->Cursor->GetPosition(), 0, 0 );
	}

	FKey Key = TranslateMouseButtonToKey( Button );

	FPointerEvent MouseEvent(
		CursorPointerIndex,
		GetCursorPos(),
		GetLastCursorPos(),
		PressedMouseButtons,
		Key,
		0,
		PlatformApplication->GetModifierKeys()
		);

	return ProcessMouseButtonDownEvent( PlatformWindow, MouseEvent );
}

bool FSlateApplication::ProcessMouseButtonDownEvent( const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& MouseEvent )
{
	QueueSynthesizedMouseMove();
	LastUserInteractionTime = this->GetCurrentTime();
	LastUserInteractionTimeForThrottling = LastUserInteractionTime;
	
	if (PlatformWindow.IsValid())
	{
		PlatformApplication->SetCapture(PlatformWindow);
	}
	PressedMouseButtons.Add( MouseEvent.GetEffectingButton() );

	bool bInGame = false;

	// Only process mouse down messages if we are not drag/dropping
	if ( !IsDragDropping() )
	{
		FReply Reply = FReply::Unhandled();
		if (MouseCaptor.HasCaptureForPointerIndex(MouseEvent.GetPointerIndex()))
		{
			FWidgetPath MouseCaptorPath = MouseCaptor.ToWidgetPath( FWeakWidgetPath::EInterruptedPathHandling::Truncate, &MouseEvent );
			FArrangedWidget& MouseCaptorWidget = MouseCaptorPath.Widgets.Last();

			// Switch worlds widgets in the current path
			FScopedSwitchWorldHack SwitchWorld( MouseCaptorPath );
			bInGame = FApp::IsGame();

			Reply = FEventRouter::Route<FReply>( this, FEventRouter::FToLeafmostPolicy(MouseCaptorPath), MouseEvent, []( const FArrangedWidget& InMouseCaptorWidget, const FPointerEvent& Event )
			{
				return InMouseCaptorWidget.Widget->OnPreviewMouseButtonDown( InMouseCaptorWidget.Geometry, Event );
			});
			
			if (!Reply.IsEventHandled())
			{
				Reply = FEventRouter::Route<FReply>( this, FEventRouter::FToLeafmostPolicy(MouseCaptorPath), MouseEvent,
					[this]( const FArrangedWidget& InMouseCaptorWidget, const FPointerEvent& Event )
					{
						FReply TempReply = FReply::Unhandled();
						if ( Event.IsTouchEvent() )
						{
							TempReply = InMouseCaptorWidget.Widget->OnTouchStarted( InMouseCaptorWidget.Geometry, Event );
						}
						if ( !Event.IsTouchEvent() || ( !TempReply.IsEventHandled() && this->bTouchFallbackToMouse ) )
						{
							TempReply = InMouseCaptorWidget.Widget->OnMouseButtonDown( InMouseCaptorWidget.Geometry, Event );
						}
						return TempReply;
					} );
			}
			LOG_EVENT( EEventLog::MouseButtonDown, Reply );
		}
		else
		{
			FWidgetPath WidgetsUnderCursor = LocateWindowUnderMouse( MouseEvent.GetScreenSpacePosition(), GetInteractiveTopLevelWindows() );

#if PLATFORM_MAC
			NSWindow* ActiveWindow = [NSApp keyWindow];
#endif
			PopupSupport.SendNotifications( WidgetsUnderCursor );

			// Switch worlds widgets in the current path
			FScopedSwitchWorldHack SwitchWorld( WidgetsUnderCursor );
			bInGame = FApp::IsGame();

			const TSharedPtr<SWidget> PreviouslyFocusedWidget = GetKeyboardFocusedWidget();

			Reply = FEventRouter::Route<FReply>(this, FEventRouter::FTunnelPolicy(WidgetsUnderCursor), MouseEvent, [](const FArrangedWidget TargetWidget, const FPointerEvent& Event)
			{
				return TargetWidget.Widget->OnPreviewMouseButtonDown(TargetWidget.Geometry, Event);
			});

			if ( !Reply.IsEventHandled() )
			{
				Reply = FEventRouter::Route<FReply>(this, FEventRouter::FBubblePolicy(WidgetsUnderCursor), MouseEvent, [this](const FArrangedWidget TargetWidget, const FPointerEvent& Event)
				{
					FReply ThisReply = FReply::Unhandled();
					if (!ThisReply.IsEventHandled())
					{
						if (Event.IsTouchEvent())
						{
							ThisReply = TargetWidget.Widget->OnTouchStarted( TargetWidget.Geometry, Event );
						}
						if (!Event.IsTouchEvent() || (!ThisReply.IsEventHandled() && this->bTouchFallbackToMouse))
						{
							ThisReply = TargetWidget.Widget->OnMouseButtonDown( TargetWidget.Geometry, Event );
						}
					}
					return ThisReply;
				});
			}
			LOG_EVENT( EEventLog::MouseButtonDown, Reply );

			// If none of the widgets requested keyboard focus to be set (or set the keyboard focus explicitly), set it to the leaf-most widget under the mouse.
			// On Mac we prevent the OS from activating the window on mouse down, so we have full control and can activate only if there's nothing draggable under the mouse cursor.
			const bool bFocusChangedByEventHandler = PreviouslyFocusedWidget != GetKeyboardFocusedWidget();
			if ((!Reply.GetUserFocusRecepient().IsValid() || (PLATFORM_MAC && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !DragDetector.DetectDragForWidget.IsValid())) && !bFocusChangedByEventHandler)
			{
				// The event handler for OnMouseButton down may have altered the widget hierarchy.
				// Refresh the previously-cached widget path.
				WidgetsUnderCursor = LocateWindowUnderMouse( MouseEvent.GetScreenSpacePosition(), GetInteractiveTopLevelWindows() );

				bool bFocusCandidateFound = false;
				for( int32 WidgetIndex = WidgetsUnderCursor.Widgets.Num()-1; !bFocusCandidateFound && WidgetIndex >= 0 ; --WidgetIndex )
				{
					FArrangedWidget& CurWidget = WidgetsUnderCursor.Widgets[WidgetIndex];
					if (CurWidget.Widget->SupportsKeyboardFocus())
					{
						bFocusCandidateFound = true;
						FWidgetPath NewFocusedWidgetPath = WidgetsUnderCursor.GetPathDownTo( CurWidget.Widget );
						SetKeyboardFocus( NewFocusedWidgetPath, EFocusCause::Mouse );
					}
				}

#if PLATFORM_MAC
				if (WidgetsUnderCursor.TopLevelWindow.IsValid())
				{
					// Clicking on a context menu should not activate anything
					// @todo: This needs to be updated when we have window type in SWindow and we no longer have to guess if WidgetsUnderCursor.TopLevelWindow is a menu
					const bool bIsContextMenu = !WidgetsUnderCursor.TopLevelWindow->IsRegularWindow() && WidgetsUnderCursor.TopLevelWindow->HasMinimizeBox() && WidgetsUnderCursor.TopLevelWindow->HasMaximizeBox();
					if (!bIsContextMenu && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !DragDetector.DetectDragForWidget.IsValid() && ActiveWindow == [NSApp keyWindow])
					{
						MouseCaptorHelper Captor = MouseCaptor;
						FPlatformMisc::ActivateApplication();
						WidgetsUnderCursor.TopLevelWindow->BringToFront(true);
						MouseCaptor = Captor;
					}
				}
#endif
			}
		}

		// See if expensive tasks should be throttled.  By default on mouse down expensive tasks are throttled
		// to ensure Slate responsiveness in low FPS situations
		if (Reply.IsEventHandled() && !bInGame && !MouseEvent.IsTouchEvent())
		{
			// Enter responsive mode if throttling should occur and its not already happening
			if( Reply.ShouldThrottle() && !MouseButtonDownResponsivnessThrottle.IsValid() )
			{
				MouseButtonDownResponsivnessThrottle = FSlateThrottleManager::Get().EnterResponsiveMode();
			}
			else if( !Reply.ShouldThrottle() && MouseButtonDownResponsivnessThrottle.IsValid() )
			{
				// Leave responsive mode if a widget chose not to throttle
				FSlateThrottleManager::Get().LeaveResponsiveMode( MouseButtonDownResponsivnessThrottle );
			}
		}
	}

	PointerIndexLastPositionMap.Add(MouseEvent.GetPointerIndex(), MouseEvent.GetScreenSpacePosition());
	return true;
}

bool FSlateApplication::OnMouseDoubleClick( const TSharedPtr< FGenericWindow >& PlatformWindow, const EMouseButtons::Type Button )
{
	if (bIsFakingTouch || bIsGameFakingTouch)
	{
		bIsFakingTouched = true;
		return OnTouchStarted(PlatformWindow, PlatformApplication->Cursor->GetPosition(), 0, 0);
	}

	FKey Key = TranslateMouseButtonToKey( Button );

	FPointerEvent MouseEvent(
		CursorPointerIndex,
		GetCursorPos(),
		GetLastCursorPos(),
		PressedMouseButtons,
		Key,
		0,
		PlatformApplication->GetModifierKeys()
		);

	return ProcessMouseButtonDoubleClickEvent( PlatformWindow, MouseEvent );
}

bool FSlateApplication::ProcessMouseButtonDoubleClickEvent( const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& InMouseEvent )
{
	QueueSynthesizedMouseMove();
	LastUserInteractionTime = this->GetCurrentTime();
	LastUserInteractionTimeForThrottling = LastUserInteractionTime;
	
	PlatformApplication->SetCapture( PlatformWindow );
	PressedMouseButtons.Add( InMouseEvent.GetEffectingButton() );

	FWidgetPath WidgetsUnderCursor = LocateWindowUnderMouse( InMouseEvent.GetScreenSpacePosition(), GetInteractiveTopLevelWindows() );

	// Switch worlds widgets in the current path
	FScopedSwitchWorldHack SwitchWorld( WidgetsUnderCursor );

	FReply Reply = FEventRouter::Route<FReply>( this, FEventRouter::FBubblePolicy(WidgetsUnderCursor), InMouseEvent, [](const FArrangedWidget& TargetWidget, const FPointerEvent& Event)
	{
		return TargetWidget.Widget->OnMouseButtonDoubleClick( TargetWidget.Geometry, Event );
	} );


	LOG_EVENT( EEventLog::MouseButtonDoubleClick, Reply );

	PointerIndexLastPositionMap.Add(InMouseEvent.GetPointerIndex(), InMouseEvent.GetScreenSpacePosition());

	return Reply.IsEventHandled();
}

bool FSlateApplication::OnMouseUp( const EMouseButtons::Type Button )
{
	// convert to touch event if we are faking it	
	if (bIsFakingTouch || bIsGameFakingTouch)
	{
		bIsFakingTouched = false;
		return OnTouchEnded(PlatformApplication->Cursor->GetPosition(), 0, 0);
	}

	FKey Key = TranslateMouseButtonToKey( Button );

	FPointerEvent MouseEvent(
		CursorPointerIndex,
		GetCursorPos(),
		GetLastCursorPos(),
		PressedMouseButtons,
		Key,
		0,
		PlatformApplication->GetModifierKeys()
		);

	return ProcessMouseButtonUpEvent( MouseEvent );
}

bool FSlateApplication::ProcessMouseButtonUpEvent( FPointerEvent& MouseEvent )
{
	QueueSynthesizedMouseMove();
	LastUserInteractionTime = this->GetCurrentTime();
	LastUserInteractionTimeForThrottling = LastUserInteractionTime;
	PressedMouseButtons.Remove( MouseEvent.GetEffectingButton() );

	if (DragDetector.DetectDragForWidget.IsValid() && MouseEvent.GetEffectingButton() == DragDetector.DetectDragButton )
	{
		// The user has release the button that was supposed to start the drag; stop detecting it.
		DragDetector = FDragDetector();
	}

	if (MouseCaptor.HasCaptureForPointerIndex(MouseEvent.GetPointerIndex()))
	{
		//FWidgetPath MouseCaptorPath = MouseCaptor.ToWidgetPath(MouseEvent.GetPointerIndex());
		FWidgetPath MouseCaptorPath = MouseCaptor.ToWidgetPath( FWeakWidgetPath::EInterruptedPathHandling::Truncate, &MouseEvent );
		if ( ensureMsg(MouseCaptorPath.Widgets.Num() > 0, TEXT("A window had a widget with mouse capture. That entire window has been dismissed before the mouse up could be processed.")) )
		{
#if PLATFORM_MAC
			NSWindow* ActiveNativeWindow = [NSApp keyWindow];
#endif
			// Switch worlds widgets in the current path
			FScopedSwitchWorldHack SwitchWorld( MouseCaptorPath );

			FReply Reply = FEventRouter::Route<FReply>( this, FEventRouter::FToLeafmostPolicy(MouseCaptorPath), MouseEvent, [this]( const FArrangedWidget& TargetWidget, const FPointerEvent& Event )
			{
				FReply TempReply = FReply::Unhandled();
				if (Event.IsTouchEvent())
			{
					TempReply = TargetWidget.Widget->OnTouchEnded( TargetWidget.Geometry, Event );
			}
				if (!Event.IsTouchEvent() || (!TempReply.IsEventHandled() && this->bTouchFallbackToMouse))
			{
					TempReply = TargetWidget.Widget->OnMouseButtonUp( TargetWidget.Geometry, Event );
			}
				return TempReply;
			});

#if PLATFORM_MAC
			// Activate a window under the mouse if it's inactive and mouse up didn't bring any window to front
			TSharedPtr<SWindow> ActiveWindow = GetActiveTopLevelWindow();
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && MouseCaptorPath.TopLevelWindow.IsValid() && ActiveWindow != MouseCaptorPath.TopLevelWindow
				&& ActiveNativeWindow == [NSApp keyWindow] && ![(NSWindow*)MouseCaptorPath.TopLevelWindow->GetNativeWindow()->GetOSWindowHandle() isMiniaturized])
			{
				MouseCaptorPath.TopLevelWindow->BringToFront(true);
			}
#endif
			LOG_EVENT( EEventLog::MouseButtonUp, Reply );
		}
	}
	else
	{
		FWidgetPath WidgetsUnderCursor = LocateWindowUnderMouse( MouseEvent.GetScreenSpacePosition(), GetInteractiveTopLevelWindows() );

		// Cache the drag drop content and reset the pointer in case OnMouseButtonUpMessage re-enters as a result of OnDrop
		const bool bIsDragDropping = IsDragDropping();
		TSharedPtr< FDragDropOperation > LocalDragDropContent = DragDropContent;
		DragDropContent.Reset();

		// Switch worlds widgets in the current path
		FScopedSwitchWorldHack SwitchWorld( WidgetsUnderCursor );
		
		FReply Reply = FEventRouter::Route<FReply>( this, FEventRouter::FBubblePolicy(WidgetsUnderCursor), MouseEvent, [&](const FArrangedWidget& CurWidget, const FPointerEvent& Event)
		{
			FReply TempReply = FReply::Unhandled();
			if (Event.IsTouchEvent())
			{
				TempReply = CurWidget.Widget->OnTouchEnded( CurWidget.Geometry, Event );
			}
			if (!Event.IsTouchEvent() || (!TempReply.IsEventHandled() && bTouchFallbackToMouse))
			{
				TempReply = (bIsDragDropping)
					? CurWidget.Widget->OnDrop( CurWidget.Geometry, FDragDropEvent( Event, LocalDragDropContent ) )
					: CurWidget.Widget->OnMouseButtonUp( CurWidget.Geometry, Event );
			}
			return TempReply;		
		});

		LOG_EVENT( bIsDragDropping ? EEventLog::DragDrop : EEventLog::MouseButtonUp, Reply );

		// If we were dragging, notify the content
		if ( bIsDragDropping )
		{
			// @todo slate : depending on SetEventPath() is not ideal.
			MouseEvent.SetEventPath( WidgetsUnderCursor );
			LocalDragDropContent->OnDrop( Reply.IsEventHandled(), MouseEvent );
			WidgetsUnderCursorLastEvent = FWeakWidgetPath();
		}
	}

	// If in responsive mode throttle, leave it on mouse up.
	if( MouseButtonDownResponsivnessThrottle.IsValid() )
	{
		FSlateThrottleManager::Get().LeaveResponsiveMode( MouseButtonDownResponsivnessThrottle );
	}

	if ( PressedMouseButtons.Num() == 0 )
	{
		// Release Capture
		PlatformApplication->SetCapture( nullptr );
	}

	return true;
}

bool FSlateApplication::OnMouseWheel( const float Delta )
{
	const FVector2D CurrentCursorPosition = GetCursorPos();

	FPointerEvent MouseWheelEvent(
		CursorPointerIndex,
		CurrentCursorPosition,
		CurrentCursorPosition,
		PressedMouseButtons,
		EKeys::Invalid,
		Delta,
		PlatformApplication->GetModifierKeys()
		);

	return ProcessMouseWheelOrGestureEvent( MouseWheelEvent, nullptr );
}

bool FSlateApplication::ProcessMouseWheelOrGestureEvent( FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent )
{
	QueueSynthesizedMouseMove();

	const bool bShouldProcessEvent = InWheelEvent.GetWheelDelta() != 0 ||
		(InGestureEvent != nullptr && InGestureEvent->GetGestureDelta()!=FVector2D::ZeroVector);
	
	if ( !bShouldProcessEvent )
	{
		return false;
	}

	LastUserInteractionTime = this->GetCurrentTime();
	
	// NOTE: We intentionally don't reset LastUserInteractionTimeForThrottling here so that the UI can be responsive while scrolling

	FWidgetPath EventPath = ( MouseCaptor.HasCaptureForPointerIndex(InWheelEvent.GetPointerIndex()) )
		? MouseCaptor.ToWidgetPath(InWheelEvent.GetPointerIndex())
		: LocateWindowUnderMouse( InWheelEvent.GetScreenSpacePosition(), GetInteractiveTopLevelWindows() );

	// Switch worlds widgets in the current path
	FScopedSwitchWorldHack SwitchWorld( EventPath );

	FReply Reply = FEventRouter::Route<FReply>( this, FEventRouter::FBubblePolicy(EventPath), InWheelEvent, [&InGestureEvent](const FArrangedWidget& CurWidget, const FPointerEvent& Event)
	{
		FReply TempReply = FReply::Unhandled();
		// Gesture event gets first shot, if slate doesn't respond to it, we'll try the wheel event.
		if( InGestureEvent != nullptr )
		{
			TempReply = CurWidget.Widget->OnTouchGesture( CurWidget.Geometry, *InGestureEvent );
		}
		
		// Send the mouse wheel event if we haven't already handled the gesture version of this event.
		if( !TempReply.IsEventHandled() )
		{
			TempReply = CurWidget.Widget->OnMouseWheel( CurWidget.Geometry, Event );
		}

		return TempReply;
	} );

	LOG_EVENT( bIsGestureEvent ? EEventLog::TouchGesture : EEventLog::MouseWheel, Reply );

	return Reply.IsEventHandled();
}

bool FSlateApplication::OnMouseMove()
{
	// convert to touch event if we are faking it	
	if (bIsFakingTouched)
	{
		return OnTouchMoved(PlatformApplication->Cursor->GetPosition(), 0, 0);
	}
	else if (!bIsGameFakingTouch && bIsFakingTouch)
	{
		return false;
	}

	bool Result = true;
	const FVector2D CurrentCursorPosition = GetCursorPos();
	const FVector2D LastCursorPosition = GetLastCursorPos();
	if ( LastCursorPosition != CurrentCursorPosition )
	{
		LastMouseMoveTime = GetCurrentTime();

		FPointerEvent MouseEvent(
			CursorPointerIndex,
			CurrentCursorPosition,
			LastCursorPosition,
			PressedMouseButtons,
			EKeys::Invalid,
			0,
			PlatformApplication->GetModifierKeys()
			);

		Result = ProcessMouseMoveEvent( MouseEvent );
	}

	return Result;
}

bool FSlateApplication::OnRawMouseMove( const int32 X, const int32 Y )
{
	if ( X != 0 || Y != 0 )
	{
		FPointerEvent MouseEvent(
			CursorPointerIndex,
			GetCursorPos(),
			GetLastCursorPos(),
			FVector2D( X, Y ), 
			PressedMouseButtons,
			PlatformApplication->GetModifierKeys()
		);

		ProcessMouseMoveEvent(MouseEvent);
	}
	
	return true;
}

bool FSlateApplication::ProcessMouseMoveEvent( FPointerEvent& MouseEvent, bool bIsSynthetic )
{
	if ( !bIsSynthetic )
	{
		QueueSynthesizedMouseMove();

		// Detecting a mouse move of zero delta is our way of filtering out synthesized move events
		const bool AllowSpawningOfToolTips = true;
		UpdateToolTip( AllowSpawningOfToolTips );
		
		// Guard against synthesized mouse moves and only track user interaction if the cursor pos changed
		LastUserInteractionTime = this->GetCurrentTime();
	}

	// When the event came from the OS, we are guaranteed to be over a slate window.
	// Otherwise, we are synthesizing a MouseMove ourselves, and must verify that the
	// cursor is indeed over a Slate window.
	const bool bOverSlateWindow = !bIsSynthetic || PlatformApplication->IsCursorDirectlyOverSlateWindow();
	
	FWidgetPath WidgetsUnderCursor = bOverSlateWindow
		? LocateWindowUnderMouse(MouseEvent.GetScreenSpacePosition(), GetInteractiveTopLevelWindows())
		: FWidgetPath();

	bool bHandled = false;

	FWeakWidgetPath LastWidgetsUnderCursor;

	// User asked us to detect a drag.
	bool bDragDetected = false;
	if( DragDetector.DetectDragForWidget.IsValid() )
	{	
		const FVector2D DragDelta = (DragDetector.DetectDragStartLocation - MouseEvent.GetScreenSpacePosition());
		bDragDetected = ( DragDelta.Size() > FSlateApplication::Get().GetDragTriggerDistance() );
		if (bDragDetected)
		{
			FWidgetPath DragDetectPath = DragDetector.DetectDragForWidget.ToWidgetPath(FWeakWidgetPath::EInterruptedPathHandling::ReturnInvalid);
			const TSharedPtr<SWidget> DragDetectRequestor = DragDetector.DetectDragForWidget.GetLastWidget().Pin();
			if (DragDetectPath.IsValid() && DragDetectRequestor.IsValid())
			{
				FWidgetAndPointer DetectDragForMe = DragDetectPath.FindArrangedWidgetAndCursor(DragDetectRequestor.ToSharedRef()).Get(FWidgetAndPointer());

				// A drag has been triggered. The cursor exited some widgets as a result.
				// This assignment ensures that we will send OnLeave notifications to those widgets.
				LastWidgetsUnderCursor = DragDetector.DetectDragForWidget;

				// We're finished with the drag detect.
				DragDetector = FDragDetector();

				// Send an OnDragDetected to the widget that requested drag-detection.
				
				// Switch worlds widgets in the current path
				FScopedSwitchWorldHack SwitchWorld( DragDetectPath );

				FReply Reply = FEventRouter::Route<FReply>(this, FEventRouter::FDirectPolicy(DetectDragForMe,DragDetectPath), MouseEvent, []( const FArrangedWidget& InDetectDragForMe, const FPointerEvent& TranslatedMouseEvent )
				{
					return InDetectDragForMe.Widget->OnDragDetected(InDetectDragForMe.Geometry, TranslatedMouseEvent );
				});
				
				LOG_EVENT( EEventLog::DragDetected, Reply );
			}
			else
			{
				bDragDetected = false;
			}
		}		
	}


	if (bDragDetected)
	{
		// When a drag was detected, we pretend that the widgets under the mouse last time around.
		// We have set LastWidgetsUnderCursor accordingly when the drag was detected above.
	}
	else
	{
		// No Drag Detection
		LastWidgetsUnderCursor = WidgetsUnderCursorLastEvent;
	}

	// Send out mouse leave events
	// If we are doing a drag and drop, we will send this event instead.
	{
		FDragDropEvent DragDropEvent( MouseEvent, DragDropContent );
		// Switch worlds widgets in the current path
		FScopedSwitchWorldHack SwitchWorld( LastWidgetsUnderCursor.Window.Pin() );

		for ( int32 WidgetIndex = LastWidgetsUnderCursor.Widgets.Num()-1; WidgetIndex >=0; --WidgetIndex )
		{
			// Guards for cases where WidgetIndex can become invalid due to MouseMove being re-entrant.
			while ( WidgetIndex >= LastWidgetsUnderCursor.Widgets.Num() )
			{
				WidgetIndex--;
			}

			if ( WidgetIndex >= 0 )
			{
				const TSharedPtr<SWidget>& SomeWidgetPreviouslyUnderCursor = LastWidgetsUnderCursor.Widgets[WidgetIndex].Pin();
				if( SomeWidgetPreviouslyUnderCursor.IsValid() )
				{
					TOptional<FArrangedWidget> FoundWidget = WidgetsUnderCursor.FindArrangedWidget( SomeWidgetPreviouslyUnderCursor.ToSharedRef() );
					const bool bWidgetNoLongerUnderMouse = !FoundWidget.IsSet();
					if ( bWidgetNoLongerUnderMouse )
					{
						// Widget is no longer under cursor, so send a MouseLeave.
						// The widget might not even be in the hierarchy any more!
						// Thus, we cannot translate the PointerPosition into the appropriate space for this event.
						if ( IsDragDropping() )
						{
							// Note that the event's pointer position is not translated.
							SomeWidgetPreviouslyUnderCursor->OnDragLeave( DragDropEvent );
							LOG_EVENT( EEventLog::DragLeave, SomeWidgetPreviouslyUnderCursor );

							// Reset the cursor override
							DragDropEvent.GetOperation()->SetCursorOverride( TOptional<EMouseCursor::Type>() );
						}
						else
						{
							// Note that the event's pointer position is not translated.
							SomeWidgetPreviouslyUnderCursor->OnMouseLeave( MouseEvent );
							LOG_EVENT( EEventLog::MouseLeave, SomeWidgetPreviouslyUnderCursor );
						}			
					}
				}
			}
		}
	}


	FWidgetPath MouseCaptorPath;
	if (MouseCaptor.HasCaptureForPointerIndex(MouseEvent.GetPointerIndex()))
	{
		//MouseCaptorPath = MouseCaptor.ToWidgetPath(MouseEvent.GetPointerIndex(), FWeakWidgetPath::EInterruptedPathHandling::ReturnInvalid);
		MouseCaptorPath = MouseCaptor.ToWidgetPath(FWeakWidgetPath::EInterruptedPathHandling::ReturnInvalid, &MouseEvent );
	}

	if (MouseCaptorPath.IsValid())
	{
		if ( !bIsSynthetic )
		{
			// Switch worlds widgets in the current path
			FScopedSwitchWorldHack SwitchWorld( MouseCaptorPath );
			
			FReply Reply = FEventRouter::Route<FReply>( this, FEventRouter::FToLeafmostPolicy(MouseCaptorPath), MouseEvent, [this]( const FArrangedWidget& MouseCaptorWidget, const FPointerEvent& Event )
			{
				FReply TempReply = FReply::Unhandled();
				if (Event.IsTouchEvent())
				{
					TempReply = MouseCaptorWidget.Widget->OnTouchMoved( MouseCaptorWidget.Geometry, Event );
				}
				if (!Event.IsTouchEvent() || (!TempReply.IsEventHandled() && this->bTouchFallbackToMouse))
				{
					TempReply = MouseCaptorWidget.Widget->OnMouseMove( MouseCaptorWidget.Geometry, Event );
				}
				return TempReply;
			} );
			bHandled = Reply.IsEventHandled();
		}
	}
	else
	{	
		// Switch worlds widgets in the current path
		FScopedSwitchWorldHack SwitchWorld( WidgetsUnderCursor );

		// Send out mouse enter events.
		if (IsDragDropping())
		{
			FDragDropEvent DragDropEvent( MouseEvent, DragDropContent );
			FEventRouter::Route<FNoReply>( this, FEventRouter::FBubblePolicy(WidgetsUnderCursor), DragDropEvent, [&LastWidgetsUnderCursor](const FArrangedWidget& WidgetUnderCursor, const FDragDropEvent& InDragDropEvent)
			{
				if ( !LastWidgetsUnderCursor.ContainsWidget(WidgetUnderCursor.Widget) )
				{
					WidgetUnderCursor.Widget->OnDragEnter( WidgetUnderCursor.Geometry, InDragDropEvent );
				}
				return FNoReply();
			} );
		}
		else
		{
			FEventRouter::Route<FNoReply>( this, FEventRouter::FBubblePolicy(WidgetsUnderCursor), MouseEvent, [&LastWidgetsUnderCursor](const FArrangedWidget& WidgetUnderCursor, const FPointerEvent& PointerEvent)
			{
				if ( !LastWidgetsUnderCursor.ContainsWidget(WidgetUnderCursor.Widget) )
				{
					WidgetUnderCursor.Widget->OnMouseEnter( WidgetUnderCursor.Geometry, PointerEvent );
				}
				return FNoReply();
			} );
		}
		
		// Bubble the MouseMove event.
		FReply Reply = FEventRouter::Route<FReply>(this, FEventRouter::FBubblePolicy(WidgetsUnderCursor), MouseEvent, [&](const FArrangedWidget& CurWidget, const FPointerEvent& Event)
		{
			FReply TempReply = FReply::Unhandled();

			if (Event.IsTouchEvent())
			{
				TempReply = CurWidget.Widget->OnTouchMoved( CurWidget.Geometry, Event );
			}
			if (!TempReply.IsEventHandled())
			{
				TempReply = (IsDragDropping())
					? CurWidget.Widget->OnDragOver( CurWidget.Geometry, FDragDropEvent( Event, DragDropContent ) )
					: CurWidget.Widget->OnMouseMove( CurWidget.Geometry, Event );
			}

			return TempReply;
		});

		LOG_EVENT( IsDragDropping() ? EEventLog::DragOver : EEventLog::MouseMove, Reply )

			bHandled = Reply.IsEventHandled();
	}

	// Give the current drag drop operation a chance to do something
	// custom (e.g. update the Drag/Drop preview based on content)
	if (IsDragDropping())
	{
		FDragDropEvent DragDropEvent( MouseEvent, DragDropContent );
		FScopedSwitchWorldHack SwitchWorld( WidgetsUnderCursor );
		DragDropContent->OnDragged( DragDropEvent );

		// Update the window we're under for rendering the drag drop operation if
		// it's a windowless drag drop operation.
		if ( WidgetsUnderCursor.IsValid() )
		{
			DragDropWindowPtr = WidgetsUnderCursor.GetWindow();
		}
		else
		{
			DragDropWindowPtr = nullptr;
		}

		// check the drag-drop operation for a cursor switch (on Windows, the OS thinks the mouse is
		// captured so we wont get QueryCursor calls for drag/drops internal to the Slate application)
		FCursorReply CursorResult = DragDropContent->OnCursorQuery();
		if (CursorResult.IsEventHandled())
		{
			// Query was handled, so we should set the cursor.
			PlatformApplication->Cursor->SetType(CursorResult.GetCursorType());
		}
		else
		{
			// reset the cursor to default for drag-drops
			PlatformApplication->Cursor->SetType( EMouseCursor::Default );
		}
	}
	else
	{
		DragDropWindowPtr = nullptr;
	}

	WidgetsUnderCursorLastEvent = FWeakWidgetPath( WidgetsUnderCursor );

	PointerIndexLastPositionMap.Add(MouseEvent.GetPointerIndex(), MouseEvent.GetScreenSpacePosition());
	return bHandled;
}

bool FSlateApplication::OnCursorSet()
{
	QueryCursor();
	return true;
}

FKey TranslateControllerButtonToKey( EControllerButtons::Type Button )
{
	FKey Key = EKeys::Invalid;

	switch ( Button )
	{
	case EControllerButtons::LeftAnalogY: Key = EKeys::Gamepad_LeftY; break;
	case EControllerButtons::LeftAnalogX: Key = EKeys::Gamepad_LeftX; break;

	case EControllerButtons::RightAnalogY: Key = EKeys::Gamepad_RightY; break;
	case EControllerButtons::RightAnalogX: Key = EKeys::Gamepad_RightX; break;

	case EControllerButtons::LeftTriggerAnalog: Key = EKeys::Gamepad_LeftTriggerAxis; break;
	case EControllerButtons::RightTriggerAnalog: Key = EKeys::Gamepad_RightTriggerAxis; break;

	case EControllerButtons::FaceButtonBottom: Key = EKeys::Gamepad_FaceButton_Bottom; break;
	case EControllerButtons::FaceButtonRight: Key = EKeys::Gamepad_FaceButton_Right; break;
	case EControllerButtons::FaceButtonLeft: Key = EKeys::Gamepad_FaceButton_Left; break;
	case EControllerButtons::FaceButtonTop: Key = EKeys::Gamepad_FaceButton_Top; break;

	case EControllerButtons::LeftShoulder: Key = EKeys::Gamepad_LeftShoulder; break;
	case EControllerButtons::RightShoulder: Key = EKeys::Gamepad_RightShoulder; break;
	case EControllerButtons::SpecialLeft: Key = EKeys::Gamepad_Special_Left; break;
	case EControllerButtons::SpecialRight: Key = EKeys::Gamepad_Special_Right; break;
	case EControllerButtons::LeftThumb: Key = EKeys::Gamepad_LeftThumbstick; break;
	case EControllerButtons::RightThumb: Key = EKeys::Gamepad_RightThumbstick; break;
	case EControllerButtons::LeftTriggerThreshold: Key = EKeys::Gamepad_LeftTrigger; break;
	case EControllerButtons::RightTriggerThreshold: Key = EKeys::Gamepad_RightTrigger; break;

	case EControllerButtons::DPadUp: Key = EKeys::Gamepad_DPad_Up; break;
	case EControllerButtons::DPadDown: Key = EKeys::Gamepad_DPad_Down; break;
	case EControllerButtons::DPadLeft: Key = EKeys::Gamepad_DPad_Left; break;
	case EControllerButtons::DPadRight: Key = EKeys::Gamepad_DPad_Right; break;

	case EControllerButtons::LeftStickUp: Key = EKeys::Gamepad_LeftStick_Up; break;
	case EControllerButtons::LeftStickDown: Key = EKeys::Gamepad_LeftStick_Down; break;
	case EControllerButtons::LeftStickLeft: Key = EKeys::Gamepad_LeftStick_Left; break;
	case EControllerButtons::LeftStickRight: Key = EKeys::Gamepad_LeftStick_Right; break;

	case EControllerButtons::RightStickUp: Key = EKeys::Gamepad_RightStick_Up; break;
	case EControllerButtons::RightStickDown: Key = EKeys::Gamepad_RightStick_Down; break;
	case EControllerButtons::RightStickLeft: Key = EKeys::Gamepad_RightStick_Left; break;
	case EControllerButtons::RightStickRight: Key = EKeys::Gamepad_RightStick_Right; break;

	case EControllerButtons::Touch0: Key = EKeys::Steam_Touch_0; break;
	case EControllerButtons::Touch1: Key = EKeys::Steam_Touch_1; break;
	case EControllerButtons::Touch2: Key = EKeys::Steam_Touch_2; break;
	case EControllerButtons::Touch3: Key = EKeys::Steam_Touch_3; break;
	case EControllerButtons::BackLeft: Key = EKeys::Steam_Back_Left; break;
	case EControllerButtons::BackRight: Key = EKeys::Steam_Back_Right; break;

	case EControllerButtons::GlobalMenu: Key = EKeys::Global_Menu; break;
	case EControllerButtons::GlobalView: Key = EKeys::Global_View; break;
	case EControllerButtons::GlobalPause: Key = EKeys::Global_Pause; break;
	case EControllerButtons::GlobalPlay: Key = EKeys::Global_Play; break;
	case EControllerButtons::GlobalBack: Key = EKeys::Global_Back; break;

	case EControllerButtons::AndroidBack: Key = EKeys::Android_Back; break;
	case EControllerButtons::AndroidVolumeUp: Key = EKeys::Android_Volume_Up; break;
	case EControllerButtons::AndroidVolumeDown: Key = EKeys::Android_Volume_Down; break;
	case EControllerButtons::AndroidMenu: Key = EKeys::Android_Menu; break;

	case EControllerButtons::Invalid: Key = EKeys::Invalid; break;
	}

	return Key;
}

bool FSlateApplication::AttemptNavigation(const FNavigationEvent& NavigationEvent, const FNavigationReply& NavigationReply, const FArrangedWidget& BoundaryWidget)
{
	TSharedPtr<SWidget> FocusedWidget = GetUserFocusedWidget(NavigationEvent.GetUserIndex());
	TSharedPtr<SWidget> NewFocusedWidget = TSharedPtr<SWidget>();
	if (FocusedWidget.IsValid())
	{
		EUINavigation NavigationType = NavigationEvent.GetNavigationType();

		// If the FocusedWidget is the Boundary widget we don't need to do any complex work. 
		if (FocusedWidget == NavigationReply.GetHandler())
		{
			if (NavigationReply.GetBoundaryRule() == EUINavigationRule::Explicit)
			{
				NewFocusedWidget = NavigationReply.GetFocusRecipient();
			}
			else if (NavigationReply.GetBoundaryRule() == EUINavigationRule::Custom)
			{
				const FNavigationDelegate& FocusDelegate = NavigationReply.GetFocusDelegate();
				if (FocusDelegate.IsBound())
				{
					NewFocusedWidget = FocusDelegate.Execute(NavigationType);
				}
			}
			// If it's not explicit or custom we do noting.
			// This is an optimization because wrapping and stopping on the widget that is currently focused can't go anywhere.
		}
		else
		{
			// Get the controller focus target for this user
			FWeakWidgetPath FocusedWeakWidgetPath = UserFocusEntries[NavigationEvent.GetUserIndex()].WidgetPath;

			// Find the next widget
			if (NavigationType == EUINavigation::Next || NavigationType == EUINavigation::Previous)
			{
				// Fond the next widget
				FWidgetPath NewFocusedWidgetPath = FocusedWeakWidgetPath.ToNextFocusedPath(NavigationType);

				// Resolve the Widget Path
				FArrangedWidget& NewFocusedArrangedWidget = NewFocusedWidgetPath.Widgets.Last();
				NewFocusedWidget = NewFocusedArrangedWidget.Widget;
			}
			else
			{
				// Resolve the Widget Path
				FWidgetPath FocusedWidgetPath = FocusedWeakWidgetPath.ToWidgetPath();
				FArrangedWidget& FocusedArrangedWidget = FocusedWidgetPath.Widgets.Last();

				// Switch worlds for widgets in the current path 
				FScopedSwitchWorldHack SwitchWorld(FocusedWidgetPath);

				NewFocusedWidget = FocusedWidgetPath.GetWindow()->GetHittestGrid()->FindNextFocusableWidget(FocusedArrangedWidget, NavigationType, NavigationReply, BoundaryWidget);
			}
		}
	}
	
	// Set controller focus if we have a valid widget
	if (NewFocusedWidget.IsValid())
	{
		SetUserFocus(NavigationEvent.GetUserIndex(), NewFocusedWidget, EFocusCause::Navigation);
		return true;
	}

	return false;
}

bool FSlateApplication::OnControllerAnalog( EControllerButtons::Type Button, int32 ControllerId, float AnalogValue )
{
	FKey Key = TranslateControllerButtonToKey(Button);
	int32 UserIndex = GetUserIndexForController(ControllerId);
	
	FAnalogInputEvent AnalogInputEvent(Key, PlatformApplication->GetModifierKeys(), UserIndex, false, 0, 0, AnalogValue);

	return ProcessAnalogInputEvent(AnalogInputEvent);
}

bool FSlateApplication::OnControllerButtonPressed( EControllerButtons::Type Button, int32 ControllerId, bool IsRepeat )
{
	FKey Key = TranslateControllerButtonToKey(Button);
	int32 UserIndex = GetUserIndexForController(ControllerId);

	FKeyEvent KeyEvent(Key, PlatformApplication->GetModifierKeys(), UserIndex, IsRepeat, 0, 0);

	return ProcessKeyDownEvent(KeyEvent);
}

bool FSlateApplication::OnControllerButtonReleased( EControllerButtons::Type Button, int32 ControllerId, bool IsRepeat )
{
	FKey Key = TranslateControllerButtonToKey(Button);
	int32 UserIndex = GetUserIndexForController(ControllerId);
	
	FKeyEvent KeyEvent(Key, PlatformApplication->GetModifierKeys(),UserIndex, IsRepeat,  0, 0);

	return ProcessKeyUpEvent(KeyEvent);
}

bool FSlateApplication::OnTouchGesture( EGestureEvent::Type GestureType, const FVector2D &Delta, const float MouseWheelDelta )
{
	const FVector2D CurrentCursorPosition = GetCursorPos();
	
	FPointerEvent GestureEvent(
		CurrentCursorPosition,
		CurrentCursorPosition,
		PressedMouseButtons,
		PlatformApplication->GetModifierKeys(),
		GestureType,
		Delta
	);
	
	FPointerEvent MouseWheelEvent(
		CursorPointerIndex,
		CurrentCursorPosition,
		CurrentCursorPosition,
		PressedMouseButtons,
		EKeys::Invalid,
		MouseWheelDelta,
		PlatformApplication->GetModifierKeys()
	);
	
	return ProcessMouseWheelOrGestureEvent( MouseWheelEvent, &GestureEvent );
}

bool FSlateApplication::OnTouchStarted( const TSharedPtr< FGenericWindow >& PlatformWindow, const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
{
	FPointerEvent PointerEvent(
		ControllerId,
		TouchIndex,
		Location,
		Location,
		true);
	ProcessTouchStartedEvent( PlatformWindow, PointerEvent );

	return true;
}

void FSlateApplication::ProcessTouchStartedEvent( const TSharedPtr< FGenericWindow >& PlatformWindow, FPointerEvent& PointerEvent )
{
	ProcessMouseButtonDownEvent( PlatformWindow, PointerEvent );
}

bool FSlateApplication::OnTouchMoved( const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
{
	FVector2D LastLocation = PointerIndexLastPositionMap.Contains(TouchIndex) ? 
		PointerIndexLastPositionMap[TouchIndex] : Location;
	FPointerEvent PointerEvent(
		ControllerId,
		TouchIndex,
		Location,
		LastLocation,
		true);
	ProcessTouchMovedEvent(PointerEvent);

	return true;
}

void FSlateApplication::ProcessTouchMovedEvent( FPointerEvent& PointerEvent )
{
	ProcessMouseMoveEvent(PointerEvent);
}

bool FSlateApplication::OnTouchEnded( const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
{
	FPointerEvent PointerEvent(
		ControllerId,
		TouchIndex,
		Location,
		Location,
		true);
	ProcessTouchEndedEvent(PointerEvent);

	return true;
}

void FSlateApplication::ProcessTouchEndedEvent( FPointerEvent& PointerEvent )
{
	ProcessMouseButtonUpEvent(PointerEvent);
}

bool FSlateApplication::OnMotionDetected(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration, int32 ControllerId)
{
	FMotionEvent MotionEvent( 
		ControllerId,
		Tilt,
		RotationRate,
		Gravity,
		Acceleration
		);
	ProcessMotionDetectedEvent(MotionEvent);

	return true;
}

void FSlateApplication::ProcessMotionDetectedEvent( FMotionEvent& MotionEvent )
{
	QueueSynthesizedMouseMove();
	LastUserInteractionTime = this->GetCurrentTime();
	
	if (MotionEvent.GetUserIndex() < ARRAY_COUNT(UserFocusEntries) && UserFocusEntries[MotionEvent.GetUserIndex()].WidgetPath.IsValid())
	{
		/* Get the controller focus target for this user */
		const FWidgetPath PathToWidget = UserFocusEntries[MotionEvent.GetUserIndex()].WidgetPath.ToWidgetPath();
		
		FScopedSwitchWorldHack SwitchWorld(PathToWidget);

		FReply Reply = FEventRouter::Route<FReply>(this, FEventRouter::FBubblePolicy(PathToWidget), MotionEvent, [] (const FArrangedWidget& SomeWidget, const FMotionEvent& InMotionEvent)
		{
			return SomeWidget.Widget->OnMotionDetected(SomeWidget.Geometry, InMotionEvent);
		});
	}
}

bool FSlateApplication::OnSizeChanged( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 Width, const int32 Height, bool bWasMinimized )
{
	TSharedPtr< SWindow > Window = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, PlatformWindow );

	if ( Window.IsValid() )
	{
		Window->SetCachedSize( FVector2D( Width, Height ) );

		Renderer->RequestResize( Window, Width, Height );

		if ( !bWasMinimized && Window->IsRegularWindow() && !Window->HasOSWindowBorder() && Window->IsVisible() )
		{
			PrivateDrawWindows( Window );
		}

		if( !bWasMinimized && Window->IsVisible() && Window->IsRegularWindow() && Window->IsAutosized() )
		{
			// Reduces flickering due to one frame lag when windows are resized automatically
			Renderer->FlushCommands();
		}

		// Inform the notification manager we have activated a window - it may want to force notifications 
		// back to the front of the z-order
		FSlateNotificationManager::Get().ForceNotificationsInFront( Window.ToSharedRef() );
	}

	return true;
}

void FSlateApplication::OnOSPaint( const TSharedRef< FGenericWindow >& PlatformWindow )
{
	TSharedPtr< SWindow > Window = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, PlatformWindow );
	PrivateDrawWindows( Window );
	Renderer->FlushCommands();
}

FWindowSizeLimits FSlateApplication::GetSizeLimitsForWindow(const TSharedRef<FGenericWindow>& Window) const
{
	TSharedPtr<SWindow> SlateWindow = FSlateWindowHelper::FindWindowByPlatformWindow(SlateWindows, Window);
	if (SlateWindow.IsValid())
	{
		return SlateWindow->GetSizeLimits();
	}
	else
	{
		return FWindowSizeLimits();
	}

}

void FSlateApplication::OnResizingWindow( const TSharedRef< FGenericWindow >& PlatformWindow )
{
	// Flush the rendering command queue to ensure that there aren't pending viewport draw commands for the old viewport size.
	Renderer->FlushCommands();
}

bool FSlateApplication::BeginReshapingWindow( const TSharedRef< FGenericWindow >& PlatformWindow )
{
	if(!IsExternalUIOpened())
	{
		if (!ThrottleHandle.IsValid())
		{
			ThrottleHandle = FSlateThrottleManager::Get().EnterResponsiveMode();
		}

		return true;
	}

	return false;
}

void FSlateApplication::FinishedReshapingWindow( const TSharedRef< FGenericWindow >& PlatformWindow )
{
	if (ThrottleHandle.IsValid())
	{
		FSlateThrottleManager::Get().LeaveResponsiveMode(ThrottleHandle);
	}
}

void FSlateApplication::OnMovedWindow( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 X, const int32 Y )
{
	TSharedPtr< SWindow > Window = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, PlatformWindow );

	if ( Window.IsValid() )
	{
		Window->SetCachedScreenPosition( FVector2D( X, Y ) );
	}
}

FWindowActivateEvent::EActivationType TranslationWindowActivationMessage( const EWindowActivation::Type ActivationType )
{
	FWindowActivateEvent::EActivationType Result = FWindowActivateEvent::EA_Activate;

	switch( ActivationType )
	{
	case EWindowActivation::Activate:
		Result = FWindowActivateEvent::EA_Activate;
		break;
	case EWindowActivation::ActivateByMouse:
		Result = FWindowActivateEvent::EA_ActivateByMouse;
		break;
	case EWindowActivation::Deactivate:
		Result = FWindowActivateEvent::EA_Deactivate;
		break;
	default:
		check( false );
	}

	return Result;
}

bool FSlateApplication::OnWindowActivationChanged( const TSharedRef< FGenericWindow >& PlatformWindow, const EWindowActivation::Type ActivationType )
{
	TSharedPtr< SWindow > Window = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, PlatformWindow );

	if ( !Window.IsValid() )
	{
		return false;
	}

	FWindowActivateEvent::EActivationType TranslatedActivationType = TranslationWindowActivationMessage( ActivationType );
	FWindowActivateEvent WindowActivateEvent( TranslatedActivationType, Window.ToSharedRef() );

	return ProcessWindowActivatedEvent( WindowActivateEvent );
}

bool FSlateApplication::ProcessWindowActivatedEvent( const FWindowActivateEvent& ActivateEvent )
{
	TSharedPtr<SWindow> ActiveModalWindow = GetActiveModalWindow();

	if ( ActivateEvent.GetActivationType() != FWindowActivateEvent::EA_Deactivate )
	{
		// Only window activate by mouse is considered a user interaction
		if (ActivateEvent.GetActivationType() == FWindowActivateEvent::EA_ActivateByMouse)
		{
			LastUserInteractionTime = this->GetCurrentTime();
		}
		
		// NOTE: The window is brought to front even when a modal window is active and this is not the modal window one of its children 
		// The reason for this is so that the Slate window order is in sync with the OS window order when a modal window is open.  This is important so that when the modal window closes the proper window receives input from Slate.
		// If you change this be sure to test windows are activated properly and receive input when they are opened when a modal dialog is open.
		FSlateWindowHelper::BringWindowToFront(SlateWindows, ActivateEvent.GetAffectedWindow());

		// Do not process activation messages unless we have no modal windows or the current window is modal
		if( !ActiveModalWindow.IsValid() || ActivateEvent.GetAffectedWindow() == ActiveModalWindow || ActivateEvent.GetAffectedWindow()->IsDescendantOf(ActiveModalWindow) )
		{
			// Window being ACTIVATED
			{
				// Switch worlds widgets in the current path
				FScopedSwitchWorldHack SwitchWorld( ActivateEvent.GetAffectedWindow() );
				ActivateEvent.GetAffectedWindow()->OnIsActiveChanged( ActivateEvent );
			}

			if ( ActivateEvent.GetAffectedWindow()->IsRegularWindow() )
			{
				ActiveTopLevelWindow = ActivateEvent.GetAffectedWindow();
			}

			// A Slate window was activated
			bSlateWindowActive = true;


			{
				FScopedSwitchWorldHack SwitchWorld( ActivateEvent.GetAffectedWindow() );
				// let the menu stack know of new window being activated.  We may need to close menus as a result
				MenuStack.OnWindowActivated( ActivateEvent.GetAffectedWindow() );
			}

			// Inform the notification manager we have activated a window - it may want to force notifications 
			// back to the front of the z-order
			FSlateNotificationManager::Get().ForceNotificationsInFront( ActivateEvent.GetAffectedWindow() );

			// As we've just been activated, attempt to restore the resolution that the engine previously cached.
			// This allows us to force ourselves back to the correct resolution after alt-tabbing out of a fullscreen
			// window and then going back in again.
			Renderer->RestoreSystemResolution(ActivateEvent.GetAffectedWindow());
		}
		else
		{
			// An attempt is being made to activate another window when a modal window is running
			ActiveModalWindow->BringToFront();
			ActiveModalWindow->FlashWindow();
		}
	}
	else
	{
		// Window being DEACTIVATED

		// If our currently-active top level window was deactivated, take note of that
		if ( ActivateEvent.GetAffectedWindow()->IsRegularWindow() &&
			ActivateEvent.GetAffectedWindow() == ActiveTopLevelWindow.Pin() )
		{
			ActiveTopLevelWindow.Reset();
		}

		// A Slate window was deactivated.  Currently there is no active Slate window
		bSlateWindowActive = false;

		// Switch worlds for the activated window
		FScopedSwitchWorldHack SwitchWorld( ActivateEvent.GetAffectedWindow() );
		ActivateEvent.GetAffectedWindow()->OnIsActiveChanged( ActivateEvent );

		// A window was deactivated; mouse capture should be cleared
		ResetToDefaultPointerInputSettings();
	}

	return true;
}

bool FSlateApplication::OnApplicationActivationChanged( const bool IsActive )
{
	ProcessApplicationActivationEvent( IsActive );
	return true;
}

void FSlateApplication::ProcessApplicationActivationEvent( bool InAppActivated )
{
	const bool UserSwitchedAway = bAppIsActive && !InAppActivated;

	bAppIsActive = InAppActivated;


	// If the user switched to a different application then we should dismiss our pop-ups.  In the case
	// where a user clicked on a different Slate window, OnWindowActivatedMessage() will be call MenuStack.OnWindowActivated()
	// to destroy any windows in our stack that are no longer appropriate to be displayed.
	if (UserSwitchedAway)
	{
		// Close pop-up menus
		DismissAllMenus();

		// Close tool-tips
		CloseToolTip();

		// No slate window is active when our entire app becomes inactive
		bSlateWindowActive = false;

		// Clear keyboard focus when the app is deactivated, if the widget isn't a child of the game viewport.
		bool bClearKeyboardFocus = true;

		if (!SlateDefs::ClearGameViewportFocusWhenWindowLosesFocus && GameViewportWidget.IsValid())
		{
			FUserFocusEntry& UserFocusEntry = UserFocusEntries[GetUserIndexForKeyboard()];
			bClearKeyboardFocus = !(UserFocusEntry.WidgetPath.ContainsWidget(GameViewportWidget.Pin().ToSharedRef()));
		}
		if (bClearKeyboardFocus)
		{
			ClearKeyboardFocus(EFocusCause::OtherWidgetLostFocus);
		}

		// If we have a slate-only drag-drop occurring, stop the drag drop.
		if ( IsDragDropping() && !DragDropContent->IsExternalOperation() )
		{
			DragDropContent.Reset();
		}
	}
}


bool FSlateApplication::OnConvertibleLaptopModeChanged()
{
	EConvertibleLaptopMode NewMode = FPlatformMisc::GetConvertibleLaptopMode();

	// Notify that we want the mobile experience when in tablet mode, otherwise use mouse and keyboard
	if (!(FParse::Param(FCommandLine::Get(), TEXT("simmobile")) || FParse::Param(FCommandLine::Get(), TEXT("faketouches"))))
	{
		// Not sure what the correct long-term strategy is. Use bIsFakingTouch for now to get things going.
		if (NewMode == EConvertibleLaptopMode::Tablet)
		{
			bIsFakingTouch = true;
		}
		else
		{
			bIsFakingTouch = false;
		}
	}

	FCoreDelegates::PlatformChangedLaptopMode.Broadcast(NewMode);

	return true;
}


EWindowZone::Type FSlateApplication::GetWindowZoneForPoint( const TSharedRef< FGenericWindow >& PlatformWindow, const int32 X, const int32 Y )
{
	TSharedPtr< SWindow > Window = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, PlatformWindow );

	if ( Window.IsValid() )
	{
		return Window->GetCurrentWindowZone( FVector2D( X, Y ) );
	}

	return EWindowZone::NotInWindow;
}


void FSlateApplication::PrivateDestroyWindow( const TSharedRef<SWindow>& DestroyedWindow )
{
	// Notify the window that it is going to be destroyed.  The window must be completely intact when this is called 
	// because delegates are allowed to leave Slate here
	DestroyedWindow->NotifyWindowBeingDestroyed();

	// Release rendering resources.  
	// This MUST be done before destroying the native window as the native window is required to be valid before releasing rendering resources with some API's
	Renderer->OnWindowDestroyed( DestroyedWindow );

	// Destroy the native window
	DestroyedWindow->DestroyWindowImmediately();

	// Remove the window and all its children from the Slate window list
	FSlateWindowHelper::RemoveWindowFromList(SlateWindows, DestroyedWindow);

	// Shutdown the application if there are no more windows
	{
		bool bAnyRegularWindows = false;
		for( auto WindowIter( SlateWindows.CreateConstIterator() ); WindowIter; ++WindowIter )
		{
			auto Window = *WindowIter;
			if( Window->IsRegularWindow() )
			{
				bAnyRegularWindows = true;
				break;
			}
		}

		if (!bAnyRegularWindows)
		{
			OnExitRequested.ExecuteIfBound();
		}
	}
}

void FSlateApplication::OnWindowClose( const TSharedRef< FGenericWindow >& PlatformWindow )
{
	TSharedPtr< SWindow > Window = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, PlatformWindow );

	if ( Window.IsValid() )
	{
		Window->RequestDestroyWindow();
	}
}

EDropEffect::Type FSlateApplication::OnDragEnterText( const TSharedRef< FGenericWindow >& Window, const FString& Text )
{
	const TSharedPtr< FExternalDragOperation > DragDropOperation = FExternalDragOperation::NewText( Text );
	const TSharedPtr< SWindow > EffectingWindow = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, Window );

	EDropEffect::Type Result = EDropEffect::None;
	if ( DragDropOperation.IsValid() && EffectingWindow.IsValid() )
	{
		Result = OnDragEnter( EffectingWindow.ToSharedRef(), DragDropOperation.ToSharedRef() );
	}

	return Result;
}

EDropEffect::Type FSlateApplication::OnDragEnterFiles( const TSharedRef< FGenericWindow >& Window, const TArray< FString >& Files )
{
	const TSharedPtr< FExternalDragOperation > DragDropOperation = FExternalDragOperation::NewFiles( Files );
	const TSharedPtr< SWindow > EffectingWindow = FSlateWindowHelper::FindWindowByPlatformWindow( SlateWindows, Window );

	EDropEffect::Type Result = EDropEffect::None;
	if ( DragDropOperation.IsValid() && EffectingWindow.IsValid() )
	{
		Result = OnDragEnter( EffectingWindow.ToSharedRef(), DragDropOperation.ToSharedRef() );
	}

	return Result;
}

EDropEffect::Type FSlateApplication::OnDragEnter( const TSharedRef< SWindow >& Window, const TSharedRef<FExternalDragOperation>& DragDropOperation )
{
	// We are encountering a new drag and drop operation.
	// Assume we cannot handle it.
	DragIsHandled = false;

	const FVector2D CurrentCursorPosition = GetCursorPos();
	const FVector2D LastCursorPosition = GetLastCursorPos();

	// Tell slate to enter drag and drop mode.
	// Make a faux mouse event for slate, so we can initiate a drag and drop.
	FDragDropEvent DragDropEvent(
		FPointerEvent(
		CursorPointerIndex,
		CurrentCursorPosition,
		LastCursorPosition,
		PressedMouseButtons,
		EKeys::Invalid,
		0,
		PlatformApplication->GetModifierKeys() ),
		DragDropOperation
	);

	ProcessDragEnterEvent( Window, DragDropEvent );
	return EDropEffect::None;
}

bool FSlateApplication::ProcessDragEnterEvent( TSharedRef<SWindow> WindowEntered, FDragDropEvent& DragDropEvent )
{
	LastUserInteractionTime = this->GetCurrentTime();
	
	FWidgetPath WidgetsUnderCursor = LocateWindowUnderMouse( DragDropEvent.GetScreenSpacePosition(), GetInteractiveTopLevelWindows() );

	// Switch worlds for widgets in the current path
	FScopedSwitchWorldHack SwitchWorld( WidgetsUnderCursor );

	FReply TriggerDragDropReply = FReply::Handled().BeginDragDrop( DragDropEvent.GetOperation().ToSharedRef() );
	ProcessReply( WidgetsUnderCursor, TriggerDragDropReply, &WidgetsUnderCursor, &DragDropEvent );

	PointerIndexLastPositionMap.Add(DragDropEvent.GetPointerIndex(), DragDropEvent.GetScreenSpacePosition());
	return true;
}

EDropEffect::Type FSlateApplication::OnDragOver( const TSharedPtr< FGenericWindow >& Window )
{
	EDropEffect::Type Result = EDropEffect::None;

	if ( IsDragDropping() )
	{
		bool MouseMoveHandled = true;
		FVector2D CursorMovementDelta( 0, 0 );
		const FVector2D CurrentCursorPosition = GetCursorPos();
		const FVector2D LastCursorPosition = GetLastCursorPos();

		if ( LastCursorPosition != CurrentCursorPosition )
		{
			FPointerEvent MouseEvent(
				CursorPointerIndex,
				CurrentCursorPosition,
				LastCursorPosition,
				PressedMouseButtons,
				EKeys::Invalid,
				0,
				PlatformApplication->GetModifierKeys()
			);

			MouseMoveHandled = ProcessMouseMoveEvent( MouseEvent );
			CursorMovementDelta = MouseEvent.GetCursorDelta();
		}

		// Slate is now in DragAndDrop mode. It is tracking the payload.
		// We just need to convey mouse movement.
		if ( CursorMovementDelta.SizeSquared() > 0 )
		{
			DragIsHandled = MouseMoveHandled;
		}

		if ( DragIsHandled )
		{
			Result = EDropEffect::Copy;
		}
	}

	return Result;
}

void FSlateApplication::OnDragLeave( const TSharedPtr< FGenericWindow >& Window )
{
	DragDropContent.Reset();
}

EDropEffect::Type FSlateApplication::OnDragDrop( const TSharedPtr< FGenericWindow >& Window )
{
	EDropEffect::Type Result = EDropEffect::None;

	if ( IsDragDropping() )
	{
		FPointerEvent MouseEvent(
			CursorPointerIndex,
			GetCursorPos(),
			GetLastCursorPos(),
			PressedMouseButtons,
			EKeys::LeftMouseButton,
			0,
			PlatformApplication->GetModifierKeys()
			);

		// User dropped into a Slate window. Slate is already in drag and drop mode.
		// It knows what to do based on a mouse up.
		if ( ProcessMouseButtonUpEvent( MouseEvent ) )
{
			Result = EDropEffect::Copy;
		}
	}

	return Result;
}

bool FSlateApplication::OnWindowAction( const TSharedRef< FGenericWindow >& PlatformWindow, const EWindowAction::Type InActionType)
{
	return !IsExternalUIOpened();
}

void FSlateApplication::OnVirtualDesktopSizeChanged(const FDisplayMetrics& NewDisplayMetric)
{
	const FPlatformRect& VirtualDisplayRect = NewDisplayMetric.VirtualDisplayRect;
	VirtualDesktopRect = FSlateRect(
		VirtualDisplayRect.Left,
		VirtualDisplayRect.Top,
		VirtualDisplayRect.Right,
		VirtualDisplayRect.Bottom);
}


/* 
 *****************************************************************************/

TSharedRef<FSlateApplication> FSlateApplication::InitializeAsStandaloneApplication(const TSharedRef<FSlateRenderer>& PlatformRenderer)
{
	return InitializeAsStandaloneApplication(PlatformRenderer, MakeShareable(FPlatformMisc::CreateApplication()));
}


TSharedRef<FSlateApplication> FSlateApplication::InitializeAsStandaloneApplication(const TSharedRef< class FSlateRenderer >& PlatformRenderer, const TSharedRef<class GenericApplication>& PlatformApplication)
{
	// create the platform slate application (what FSlateApplication::Get() returns)
	TSharedRef<FSlateApplication> Slate = FSlateApplication::Create(PlatformApplication);

	// initialize renderer
	FSlateApplication::Get().InitializeRenderer(PlatformRenderer);

	// set the normal UE4 GIsRequestingExit when outer frame is closed
	FSlateApplication::Get().SetExitRequestedHandler(FSimpleDelegate::CreateStatic(&OnRequestExit));

	return Slate;
}

void FSlateApplication::SetWidgetReflector(const TSharedRef<IWidgetReflector>& WidgetReflector)
{
	if ( SourceCodeAccessDelegate.IsBound() )
	{
		WidgetReflector->SetSourceAccessDelegate(SourceCodeAccessDelegate);
	}

	if ( AssetAccessDelegate.IsBound() )
	{
		WidgetReflector->SetAssetAccessDelegate(AssetAccessDelegate);
	}

	WidgetReflectorPtr = WidgetReflector;
}
