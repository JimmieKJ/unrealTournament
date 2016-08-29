// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SlateWrapperTypes.h"
#include "Components/Widget.h"
#include "Geometry.h"
#include "Engine/GameInstance.h"
#include "Layout/Anchors.h"
#include "NamedSlotInterface.h"
#include "Engine/LocalPlayer.h"
#include "Logging/MessageLog.h"

#include "UserWidget.generated.h"

class UWidget;
class UWidgetAnimation;
struct FLocalPlayerContext;

/**
 * The state passed into OnPaint that we can expose as a single painting structure to blueprints to
 * allow script code to override OnPaint behavior.
 */
USTRUCT(BlueprintType)
struct UMG_API FPaintContext
{
	GENERATED_USTRUCT_BODY()

public:

	/** Don't ever use this constructor.  Needed for code generation. */
	FPaintContext();

	FPaintContext(const FGeometry& InAllottedGeometry, const FSlateRect& InMyClippingRect, FSlateWindowElementList& InOutDrawElements, const int32 InLayerId, const FWidgetStyle& InWidgetStyle, const bool bInParentEnabled)
		: AllottedGeometry(InAllottedGeometry)
		, MyClippingRect(InMyClippingRect)
		, OutDrawElements(InOutDrawElements)
		, LayerId(InLayerId)
		, WidgetStyle(InWidgetStyle)
		, bParentEnabled(bInParentEnabled)
		, MaxLayer(InLayerId)
	{
	}

	/** We override the assignment operator to allow generated code to compile with the const ref member. */
	void operator=( const FPaintContext& Other )
	{
		FPaintContext* Ptr = this;
		Ptr->~FPaintContext();
		new(Ptr) FPaintContext(Other.AllottedGeometry, Other.MyClippingRect, Other.OutDrawElements, Other.LayerId, Other.WidgetStyle, Other.bParentEnabled);
		Ptr->MaxLayer = Other.MaxLayer;
	}

public:

	const FGeometry& AllottedGeometry;
	const FSlateRect& MyClippingRect;
	FSlateWindowElementList& OutDrawElements;
	int32 LayerId;
	const FWidgetStyle& WidgetStyle;
	bool bParentEnabled;

	int32 MaxLayer;
};

/**
* The state passed into OnPaint that we can expose as a single painting structure to blueprints to
* allow script code to override OnPaint behavior.
*/
USTRUCT()
struct UMG_API FNamedSlotBinding
{
	GENERATED_USTRUCT_BODY()

public:

	FNamedSlotBinding()
		: Name(NAME_None)
		, Content(nullptr)
	{ }

	UPROPERTY()
	FName Name;

	UPROPERTY()
	UWidget* Content;
};

class UUMGSequencePlayer;

/** Describes playback modes for UMG sequences. */
UENUM()
namespace EUMGSequencePlayMode
{
	enum Type
	{
		/** Animation plays and loops from the beginning to the end. */
		Forward,
		/** Animation plays and loops from the end to the beginning. */
		Reverse,
		/** Animation plays from the begging to the end and then from the end to beginning. */
		PingPong,
	};
}

#if WITH_EDITORONLY_DATA

UENUM()
enum class EDesignPreviewSizeMode : uint8
{
	FillScreen,
	Custom,
	CustomOnScreen,
	Desired,
	DesiredOnScreen,
};

#endif

//TODO UMG If you want to host a widget that's full screen there may need to be a SWindow equivalent that you spawn it into.

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnConstructEvent);

DECLARE_DYNAMIC_DELEGATE( FOnInputAction );

/**
 * The user widget is extensible by users through the WidgetBlueprint.
 */
UCLASS(Abstract, editinlinenew, BlueprintType, Blueprintable, meta=( Category="User Controls", DontUseGenericSpawnObject="True" ) )
class UMG_API UUserWidget : public UWidget, public INamedSlotInterface
{
	GENERATED_BODY()

public:
	UUserWidget(const FObjectInitializer& ObjectInitializer);

	//UObject interface
	virtual void PostInitProperties() override;
	virtual class UWorld* GetWorld() const override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	virtual bool Initialize();
	virtual void CustomNativeInitilize() {}

	//UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	// UNamedSlotInterface Begin
	virtual void GetSlotNames(TArray<FName>& SlotNames) const override;
	virtual UWidget* GetContentForSlot(FName SlotName) const override;
	virtual void SetContentForSlot(FName SlotName, UWidget* Content) override;
	// UNamedSlotInterface End

	/**
	 * Adds it to the game's viewport and fills the entire screen, unless SetDesiredSizeInViewport is called
	 * to explicitly set the size.
	 *
	 * @param ZOrder The higher the number, the more on top this widget will be.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Viewport", meta=( AdvancedDisplay = "ZOrder" ))
	void AddToViewport(int32 ZOrder = 0);

	/**
	 * Adds the widget to the game's viewport in a section dedicated to the player.  This is valuable in a split screen
	 * game where you need to only show a widget over a player's portion of the viewport.
	 *
	 * @param ZOrder The higher the number, the more on top this widget will be.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Viewport", meta=( AdvancedDisplay = "ZOrder" ))
	bool AddToPlayerScreen(int32 ZOrder = 0);

	/**
	 * Removes the widget from the viewport.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Viewport", meta=( DeprecatedFunction, DeprecationMessage="Use RemoveFromParent instead" ))
	void RemoveFromViewport();

	/**
	 * Removes the widget from its parent widget.  If this widget was added to the player's screen or the viewport
	 * it will also be removed from those containers.
	 */
	virtual void RemoveFromParent() override;

	/**
	 * Sets the widgets position in the viewport.
	 * @param Position The 2D position to set the widget to in the viewport.
	 * @param bRemoveDPIScale If you've already calculated inverse DPI, set this to false.  
	 * Otherwise inverse DPI is applied to the position so that when the location is scaled 
	 * by DPI, it ends up in the expected position.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Viewport")
	void SetPositionInViewport(FVector2D Position, bool bRemoveDPIScale = true);

	/*  */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Viewport")
	void SetDesiredSizeInViewport(FVector2D Size);

	/*  */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Viewport")
	void SetAnchorsInViewport(FAnchors Anchors);

	/*  */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Viewport")
	void SetAlignmentInViewport(FVector2D Alignment);

	/*  */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Appearance", meta=( DeprecatedFunction, DeprecationMessage="Use IsInViewport instead" ))
	bool GetIsVisible() const;

	/* @return true if the widget was added to the viewport using AddToViewport. */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Appearance")
	bool IsInViewport() const;

	/** Sets the player context associated with this UI. */
	void SetPlayerContext(const FLocalPlayerContext& InPlayerContext);

	/** Gets the player context associated with this UI. */
	const FLocalPlayerContext& GetPlayerContext() const;

	/**
	 * Gets the local player associated with this UI.
	 * @return The owning local player.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Player")
	ULocalPlayer* GetOwningLocalPlayer() const;

	/**
	 * Sets the local player associated with this UI.
	 * @param LocalPlayer The local player you want to be the conceptual owner of this UI.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Player")
	void SetOwningLocalPlayer(ULocalPlayer* LocalPlayer);

	/**
	 * Gets the player controller associated with this UI.
	 * @return The player controller that owns the UI.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Player")
	class APlayerController* GetOwningPlayer() const;

	/**
	 * Gets the player pawn associated with this UI.
	 * @return Gets the owning player pawn that's owned by the player controller assigned to this widget.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Player")
	class APawn* GetOwningPlayerPawn() const;

	/**
	 * Called after the underlying slate widget is constructed.  Depending on how the slate object is used
	 * this event may be called multiple times due to adding and removing from the hierarchy.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="User Interface", meta=( Keywords="Begin Play" ))
	void Construct();

	DEPRECATED(4.8, "Use NativeConstruct")
	virtual void Construct_Implementation();

	/**
	 * Called when a widget is no longer referenced causing the slate resource to destroyed.  Just like
	 * Construct this event can be called multiple times.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="User Interface", meta=( Keywords="End Play, Destroy" ))
	void Destruct();

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  MyGeometry The space allotted for this widget
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="User Interface")
	void Tick(FGeometry MyGeometry, float InDeltaTime);

	DEPRECATED(4.8, "Use NativeTick")
	virtual void Tick_Implementation(FGeometry MyGeometry, float InDeltaTime);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="User Interface | Painting")
	void OnPaint(UPARAM(ref) FPaintContext& Context) const;

	DEPRECATED(4.8, "Use NativePaint")
	virtual void OnPaint_Implementation(FPaintContext& Context) const;

	/**
	 * Gets a value indicating if the widget is interactive.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="User Interface | Interaction")
	bool IsInteractable() const;

	/**
	 * Called when keyboard focus is given to this widget.  This event does not bubble.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param InFocusEvent  FocusEvent
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Input")
	FEventReply OnFocusReceived(FGeometry MyGeometry, FFocusEvent InFocusEvent);

	DEPRECATED(4.8, "Use NativeOnFocusReceived")
	FEventReply OnFocusReceived_Implementation(FGeometry MyGeometry, FFocusEvent InFocusEvent);

	/**
	 * Called when this widget loses focus.  This event does not bubble.
	 *
	 * @param  InFocusEvent  FocusEvent
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Input")
	void OnFocusLost(FFocusEvent InFocusEvent);

	DEPRECATED(4.8, "Use NativeOnFocusLost")
	virtual void OnFocusLost_Implementation(FFocusEvent InFocusEvent);

	/**
	 * Called after a character is entered while this widget has focus
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InCharacterEvent  Character event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Input")
	FEventReply OnKeyChar(FGeometry MyGeometry, FCharacterEvent InCharacterEvent);

	DEPRECATED(4.8, "Use NativeOnKeyChar")
	virtual FEventReply OnKeyChar_Implementation(FGeometry MyGeometry, FCharacterEvent InCharacterEvent);

	/**
	 * Called after a key (keyboard, controller, ...) is pressed when this widget or a child of this widget has focus
	 * If a widget handles this event, OnKeyDown will *not* be passed to the focused widget.
	 *
	 * This event is primarily to allow parent widgets to consume an event before a child widget processes
	 * it and it should be used only when there is no better design alternative.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InKeyEvent  Key event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Input")
	FEventReply OnPreviewKeyDown(FGeometry MyGeometry, FKeyEvent InKeyEvent);

	DEPRECATED(4.8, "Use NativeOnPreviewKeyDown")
	virtual FEventReply OnPreviewKeyDown_Implementation(FGeometry MyGeometry, FKeyEvent InKeyEvent);

	/**
	 * Called after a key (keyboard, controller, ...) is pressed when this widget has focus (this event bubbles if not handled)
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InKeyEvent  Key event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Input")
	FEventReply OnKeyDown(FGeometry MyGeometry, FKeyEvent InKeyEvent);

	DEPRECATED(4.8, "Use NativeOnKeyDown")
	virtual FEventReply OnKeyDown_Implementation(FGeometry MyGeometry, FKeyEvent InKeyEvent);

	/**
	 * Called after a key (keyboard, controller, ...) is released when this widget has focus
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InKeyEvent  Key event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Input")
	FEventReply OnKeyUp(FGeometry MyGeometry, FKeyEvent InKeyEvent);

	DEPRECATED(4.8, "Use NativeOnKeyUp")
	virtual FEventReply OnKeyUp_Implementation(FGeometry MyGeometry, FKeyEvent InKeyEvent);

	/**
	* Called when an analog value changes on a button that supports analog
	*
	* @param MyGeometry The Geometry of the widget receiving the event
	* @param  InAnalogInputEvent  Analog Event
	* @return  Returns whether the event was handled, along with other possible actions
	*/
	UFUNCTION(BlueprintImplementableEvent, Category = "Input")
	FEventReply OnAnalogValueChanged(FGeometry MyGeometry, FAnalogInputEvent InAnalogInputEvent);

	DEPRECATED(4.8, "Use NativeOnAnalogValueChanged")
	virtual FEventReply OnAnalogValueChanged_Implementation(FGeometry MyGeometry, FAnalogInputEvent InAnalogInputEvent);

	/**
	 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Mouse")
	FEventReply OnMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	DEPRECATED(4.8, "Use NativeOnMouseButtonDown")
	virtual FEventReply OnMouseButtonDown_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/**
	 * Just like OnMouseButtonDown, but tunnels instead of bubbling.
	 * If this even is handled, OnMouseButtonDown will not be sent.
	 * 
	 * Use this event sparingly as preview events generally make UIs more
	 * difficult to reason about.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Mouse")
	FEventReply OnPreviewMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	DEPRECATED(4.8, "Use NativeOnPreviewMouseButtonDown")
	virtual FEventReply OnPreviewMouseButtonDown_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/**
	 * The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Mouse")
	FEventReply OnMouseButtonUp(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	DEPRECATED(4.8, "Use NativeOnMouseButtonUp")
	virtual FEventReply OnMouseButtonUp_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/**
	 * The system calls this method to notify the widget that a mouse moved within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Mouse")
	FEventReply OnMouseMove(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	DEPRECATED(4.8, "Use NativeOnMouseMove")
	virtual FEventReply OnMouseMove_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/**
	 * The system will use this event to notify a widget that the cursor has entered it. This event is NOT bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Mouse")
	void OnMouseEnter(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	DEPRECATED(4.8, "Use NativeOnMouseEnter")
	virtual void OnMouseEnter_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/**
	 * The system will use this event to notify a widget that the cursor has left it. This event is NOT bubbled.
	 *
	 * @param MouseEvent Information about the input event
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Mouse")
	void OnMouseLeave(const FPointerEvent& MouseEvent);

	DEPRECATED(4.8, "Use NativeOnMouseLeave")
	virtual void OnMouseLeave_Implementation(const FPointerEvent& MouseEvent);

	/**
	 * Called when the mouse wheel is spun. This event is bubbled.
	 *
	 * @param  MouseEvent  Mouse event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Mouse")
	FEventReply OnMouseWheel(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	DEPRECATED(4.8, "Use NativeOnMouseWheel")
	virtual FEventReply OnMouseWheel_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/**
	 * Called when a mouse button is double clicked.  Override this in derived classes.
	 *
	 * @param  InMyGeometry  Widget geometry
	 * @param  InMouseEvent  Mouse button event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Mouse")
	FEventReply OnMouseButtonDoubleClick(FGeometry InMyGeometry, const FPointerEvent& InMouseEvent);

	DEPRECATED(4.8, "Use NativeOnMouseButtonDoubleClick")
	virtual FEventReply OnMouseButtonDoubleClick_Implementation(FGeometry InMyGeometry, const FPointerEvent& InMouseEvent);

	// TODO
	//UFUNCTION(BlueprintImplementableEvent, Category="Mouse")
	//FCursorReply OnCursorQuery(FGeometry MyGeometry, const FPointerEvent& CursorEvent) const;

	// TODO
	//virtual bool OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent);

	/**
	 * Called when Slate detects that a widget started to be dragged.
	 *
	 * @param  InMyGeometry  Widget geometry
	 * @param  PointerEvent  MouseMove that triggered the drag
	 * @param  Operation     The drag operation that was detected.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Drag and Drop")
	void OnDragDetected(FGeometry MyGeometry, const FPointerEvent& PointerEvent, UDragDropOperation*& Operation);

	DEPRECATED(4.8, "Use NativeOnDragDetected")
	virtual void OnDragDetected_Implementation(FGeometry MyGeometry, const FPointerEvent& PointerEvent, UDragDropOperation*& Operation);

	/**
	 * Called when the user cancels the drag operation, typically when they simply release the mouse button after
	 * beginning a drag operation, but failing to complete the drag.
	 *
	 * @param  PointerEvent  Last mouse event from when the drag was canceled.
	 * @param  Operation     The drag operation that was canceled.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Drag and Drop")
	void OnDragCancelled(const FPointerEvent& PointerEvent, UDragDropOperation* Operation);

	DEPRECATED(4.8, "Use NativeOnDragCancelled")
	virtual void OnDragCancelled_Implementation(const FPointerEvent& PointerEvent, UDragDropOperation* Operation);
	
	/**
	 * Called during drag and drop when the drag enters the widget.
	 *
	 * @param MyGeometry     The geometry of the widget receiving the event.
	 * @param PointerEvent   The mouse event from when the drag entered the widget.
	 * @param Operation      The drag operation that entered the widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Drag and Drop")
	void OnDragEnter(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation);

	DEPRECATED(4.8, "Use NativeOnDragEnter")
	virtual void OnDragEnter_Implementation(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation);

	/**
	 * Called during drag and drop when the drag leaves the widget.
	 *
	 * @param PointerEvent   The mouse event from when the drag left the widget.
	 * @param Operation      The drag operation that entered the widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Drag and Drop")
	void OnDragLeave(FPointerEvent PointerEvent, UDragDropOperation* Operation);

	DEPRECATED(4.8, "Use NativeOnDragLeave")
	void OnDragLeave_Implementation(FPointerEvent PointerEvent, UDragDropOperation* Operation);

	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry     The geometry of the widget receiving the event.
	 * @param PointerEvent   The mouse event from when the drag occurred over the widget.
	 * @param Operation      The drag operation over the widget.
	 *
	 * @return 'true' to indicate that you handled the drag over operation.  Returning 'false' will cause the operation to continue to bubble up.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Drag and Drop")
	bool OnDragOver(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation);

	DEPRECATED(4.8, "Use NativeOnDragOver")
	virtual bool OnDragOver_Implementation(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation);

	/**
	 * Called when the user is dropping something onto a widget.  Ends the drag and drop operation, even if no widget handles this.
	 *
	 * @param MyGeometry     The geometry of the widget receiving the event.
	 * @param PointerEvent   The mouse event from when the drag occurred over the widget.
	 * @param Operation      The drag operation over the widget.
	 * 
	 * @return 'true' to indicate you handled the drop operation.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Drag and Drop")
	bool OnDrop(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation);

	DEPRECATED(4.8, "Use NativeOnDrop")
	virtual bool OnDrop_Implementation(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation);

	UFUNCTION(BlueprintImplementableEvent, meta = ( DeprecatedFunction, DeprecationMessage = "Use OnKeyDown() instead" ), Category = "Gamepad Input")
	FEventReply OnControllerButtonPressed(FGeometry MyGeometry, FControllerEvent ControllerEvent);

	UFUNCTION(BlueprintImplementableEvent, meta = ( DeprecatedFunction, DeprecationMessage = "Use OnKeyUp() instead" ), Category = "Gamepad Input")
	FEventReply OnControllerButtonReleased(FGeometry MyGeometry, FControllerEvent ControllerEvent);

	UFUNCTION(BlueprintImplementableEvent, meta = ( DeprecatedFunction, DeprecationMessage = "Use OnAnalogValueChanged() instead" ), Category = "Gamepad Input")
	FEventReply OnControllerAnalogValueChanged(FGeometry MyGeometry, FControllerEvent ControllerEvent);

	/**
	 * Called when the user performs a gesture on trackpad. This event is bubbled.
	 *
	 * @param MyGeometry     The geometry of the widget receiving the event.
	 * @param  GestureEvent  gesture event
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Touch Input")
	FEventReply OnTouchGesture(FGeometry MyGeometry, const FPointerEvent& GestureEvent);

	DEPRECATED(4.8, "Use NativeOnTouchGesture")
	virtual FEventReply OnTouchGesture_Implementation(FGeometry MyGeometry, const FPointerEvent& GestureEvent);

	/**
	 * Called when a touchpad touch is started (finger down)
	 * 
	 * @param MyGeometry    The geometry of the widget receiving the event.
	 * @param InTouchEvent	The touch event generated
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Touch Input")
	FEventReply OnTouchStarted(FGeometry MyGeometry, const FPointerEvent& InTouchEvent);

	DEPRECATED(4.8, "Use NativeOnTouchStarted")
	virtual FEventReply OnTouchStarted_Implementation(FGeometry MyGeometry, const FPointerEvent& InTouchEvent);
	
	/**
	 * Called when a touchpad touch is moved  (finger moved)
	 * 
	 * @param MyGeometry    The geometry of the widget receiving the event.
	 * @param InTouchEvent	The touch event generated
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Touch Input")
	FEventReply OnTouchMoved(FGeometry MyGeometry, const FPointerEvent& InTouchEvent);

	DEPRECATED(4.8, "Use NativeOnTouchMoved")
	virtual FEventReply OnTouchMoved_Implementation(FGeometry MyGeometry, const FPointerEvent& InTouchEvent);

	/**
	 * Called when a touchpad touch is ended (finger lifted)
	 * 
	 * @param MyGeometry    The geometry of the widget receiving the event.
	 * @param InTouchEvent	The touch event generated
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Touch Input")
	FEventReply OnTouchEnded(FGeometry MyGeometry, const FPointerEvent& InTouchEvent);

	DEPRECATED(4.8, "Use NativeOnTouchEnded")
	virtual FEventReply OnTouchEnded_Implementation(FGeometry MyGeometry, const FPointerEvent& InTouchEvent);
	
	/**
	 * Called when motion is detected (controller or device)
	 * e.g. Someone tilts or shakes their controller.
	 * 
	 * @param MyGeometry    The geometry of the widget receiving the event.
	 * @param MotionEvent	The motion event generated
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Touch Input")
	FEventReply OnMotionDetected(FGeometry MyGeometry, FMotionEvent InMotionEvent);

	DEPRECATED(4.8, "Use NativeOnMotionDetected")
	virtual FEventReply OnMotionDetected_Implementation(FGeometry MyGeometry, FMotionEvent InMotionEvent);

public:

	/**
	 * Called when an animation is started.
	 *
	 * @param Animation the animation that started
	 */
	UFUNCTION( BlueprintNativeEvent, BlueprintCosmetic, Category = "Animation" )
	void OnAnimationStarted( const UWidgetAnimation* Animation );

	virtual void OnAnimationStarted_Implementation(const UWidgetAnimation* Animation);

	/**
	 * Called when an animation has either played all the way through or is stopped
	 *
	 * @param Animation The animation that has finished playing
	 */
	UFUNCTION( BlueprintNativeEvent, BlueprintCosmetic, Category = "Animation" )
	void OnAnimationFinished( const UWidgetAnimation* Animation );

	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation);

public:

	/**
	 * Sets the tint of the widget, this affects all child widgets.
	 * 
	 * @param InColorAndOpacity	The tint to apply to all child widgets.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Appearance")
	void SetColorAndOpacity(FLinearColor InColorAndOpacity);

	/**
	 * Sets the foreground color of the widget, this is inherited by sub widgets.  Any color property 
	 * that is marked as inherit will use this color.
	 * 
	 * @param InForegroundColor	The foreground color.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Appearance")
	void SetForegroundColor(FSlateColor InForegroundColor);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Appearance")
	void SetPadding(FMargin InPadding);

	/**
	 * Plays an animation in this widget a specified number of times
	 * 
	 * @param InAnimation The animation to play
	 * @param StartAtTime The time in the animation from which to start playing, relative to the start position. For looped animations, this will only affect the first playback of the animation.
	 * @param NumLoopsToPlay The number of times to loop this animation (0 to loop indefinitely)
	 * @param PlaybackSpeed The speed at which the animation should play
	 * @param PlayMode Specifies the playback mode
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface|Animation")
	void PlayAnimation(const UWidgetAnimation* InAnimation, float StartAtTime = 0.0f, int32 NumLoopsToPlay = 1, EUMGSequencePlayMode::Type PlayMode = EUMGSequencePlayMode::Forward, float PlaybackSpeed = 1.0f);

	/**
	 * Plays an animation in this widget a specified number of times stoping at a specified time
	 * 
	 * @param InAnimation The animation to play
	 * @param StartAtTime The time in the animation from which to start playing, relative to the start position. For looped animations, this will only affect the first playback of the animation.
	 * @param EndAtTime The absolute time in the animation where to stop, this is only considered in the last loop.
	 * @param NumLoopsToPlay The number of times to loop this animation (0 to loop indefinitely)
	 * @param PlaybackSpeed The speed at which the animation should play
	 * @param PlayMode Specifies the playback mode
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Animation")
	void PlayAnimationTo(const UWidgetAnimation* InAnimation, float StartAtTime = 0.0f, float EndAtTime = 0.0f, int32 NumLoopsToPlay = 1, EUMGSequencePlayMode::Type PlayMode = EUMGSequencePlayMode::Forward, float PlaybackSpeed = 1.0f);

	/**
	 * Stops an already running animation in this widget
	 * 
	 * @param The name of the animation to stop
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Animation")
	void StopAnimation(const UWidgetAnimation* InAnimation);

	/**
	 * Pauses an already running animation in this widget
	 * 
	 * @param The name of the animation to pause
	 * @return the time point the animation was at when it was paused, relative to its start position.  Use this as the StartAtTime when you trigger PlayAnimation.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Animation")
	float PauseAnimation(const UWidgetAnimation* InAnimation);

	/**
	 * Gets the current time of the animation in this widget
	 * 
	 * @param The name of the animation to get the current time for
	 * @return the current time of the animation.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface|Animation")
	float GetAnimationCurrentTime(const UWidgetAnimation* InAnimation) const;

	/**
	 * Gets whether an animation is currently playing on this widget.
	 * 
	 * @param InAnimation The animation to check the playback status of
	 * @return True if the animation is currently playing
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Animation")
	bool IsAnimationPlaying(const UWidgetAnimation* InAnimation) const;

	/**
	 * @return True if any animation is currently playing
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="User Interface|Animation")
	bool IsAnyAnimationPlaying() const;

	/**
	* Changes the number of loops to play given a playing animation
	*
	* @param InAnimation The animation that is already playing
	* @param NumLoopsToPlay The number of loops to play. (0 to loop indefinitely)
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface|Animation")
	void SetNumLoopsToPlay(const UWidgetAnimation* InAnimation, int32 NumLoopsToPlay);

	/**
	* Changes the playback rate of a playing animation
	*
	* @param InAnimation The animation that is already playing
	* @param PlaybackRate Playback rate multiplier (1 is default)
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface|Animation")
	void SetPlaybackSpeed(const UWidgetAnimation* InAnimation, float PlaybackSpeed = 1.0f);

	/**
	* If an animation is playing, this function will reverse the playback.
	*
	* @param InAnimation The playing animation that we want to reverse
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "User Interface|Animation")
	void ReverseAnimation(const UWidgetAnimation* InAnimation);

	/** Called when a sequence player is finished playing an animation */
	void OnAnimationFinishedPlaying(UUMGSequencePlayer& Player );

	/**
	 * Plays a sound through the UI
	 *
	 * @param The sound to play
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Sound", meta=( DeprecatedFunction, DeprecationMessage="Use the UGameplayStatics::PlaySound2D instead." ))
	void PlaySound(class USoundBase* SoundToPlay);

	/** @returns The UObject wrapper for a given SWidget */
	UWidget* GetWidgetHandle(TSharedRef<SWidget> InWidget);

	/** @returns The root UObject widget wrapper */
	UWidget* GetRootWidget() const;

	/** @returns The slate widget corresponding to a given name */
	TSharedPtr<SWidget> GetSlateWidgetFromName(const FName& Name) const;

	/** @returns The uobject widget corresponding to a given name */
	UWidget* GetWidgetFromName(const FName& Name) const;

	//~ Begin UObject Interface
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	//~ End UObject Interface

	/** Are we currently playing any animations? */
	UFUNCTION(BlueprintCallable, Category="User Interface|Animation")
	bool IsPlayingAnimation() const { return ActiveSequencePlayers.Num() > 0; }

#if WITH_EDITOR
	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	//~ End UWidget Interface

	void SetDesignerFlags(EWidgetDesignFlags::Type NewFlags);
#endif

public:
	/** The color and opacity of this widget.  Tints all child widgets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FLinearColor ColorAndOpacity;

	UPROPERTY()
	FGetLinearColor ColorAndOpacityDelegate;

	/**
	 * The foreground color of the widget, this is inherited by sub widgets.  Any color property
	 * that is marked as inherit will use this color.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FSlateColor ForegroundColor;

	UPROPERTY()
	FGetSlateColor ForegroundColorDelegate;

	/** The padding area around the content. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance")
	FMargin Padding;

	UPROPERTY()
	bool bSupportsKeyboardFocus_DEPRECATED;

	/** Setting this flag to true, allows this widget to accept focus when clicked, or when navigated to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	bool bIsFocusable;

	/** The widget tree contained inside this user widget initialized by the blueprint */
	UPROPERTY(Transient)
	class UWidgetTree* WidgetTree;

	/** All the sequence players currently playing */
	UPROPERTY(Transient)
	TArray<UUMGSequencePlayer*> ActiveSequencePlayers;

	/** List of sequence players to cache and clean up when safe */
	UPROPERTY(Transient)
	TArray<UUMGSequencePlayer*> StoppedSequencePlayers;

	/** Stores the widgets being assigned to named slots */
	UPROPERTY()
	TArray<FNamedSlotBinding> NamedSlotBindings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bStopAction;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Input" )
	int32 Priority;

#if WITH_EDITORONLY_DATA

	/** Stores the design time desired size of the user widget */
	UPROPERTY()
	FVector2D DesignTimeSize;

	/** A flag that determines if the design time size is used for previewing the widget in the designer. */
	UPROPERTY()
	bool bUseDesignTimeSize_DEPRECATED;

	/** A flag that determines if the widget's desired size is used for previewing the widget in the designer. */
	UPROPERTY()
	bool bUseDesiredSizeAtDesignTime_DEPRECATED;

	UPROPERTY()
	EDesignPreviewSizeMode DesignSizeMode;

	/** The category this widget appears in the palette. */
	UPROPERTY()
	FText PaletteCategory;

	/**
	 * A preview background that you can use when designing the UI to get a sense of scale on the screen.  Use
	 * a texture with a screenshot of your game in it, for example if you were designing a HUD.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Designer")
	UTexture2D* PreviewBackground;

#endif

	/** If a widget doesn't ever need to tick the blueprint, setting this to false is an optimization. */
	bool bCanEverTick : 1;

	/** If a widget doesn't ever need to do custom painting in the blueprint, setting this to false is an optimization. */
	bool bCanEverPaint : 1;

protected:

	/** Adds the widget to the screen, either to the viewport or to the player's screen depending on if the LocalPlayer is null. */
	virtual void AddToScreen(ULocalPlayer* LocalPlayer, int32 ZOrder);

	/**
	 * Called when a top level widget is in the viewport and the world is potentially coming to and end. When this occurs, 
	 * it's not save to keep widgets on the screen.  We automatically remove them when this happens and mark them for pending kill.
	 */
	virtual void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnWidgetRebuilt() override;

	FMargin GetFullScreenOffset() const;
	FAnchors GetViewportAnchors() const;
	FVector2D GetFullScreenAlignment() const;

	//native SObjectWidget methods
	friend class SObjectWidget;

	virtual void NativeConstruct();
	virtual void NativeDestruct();

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime);
	virtual void NativePaint( FPaintContext& InContext ) const;

	virtual bool NativeIsInteractable() const;
	virtual bool NativeSupportsKeyboardFocus() const;

	virtual FReply NativeOnFocusReceived( const FGeometry& InGeometry, const FFocusEvent& InFocusEvent );
	virtual void NativeOnFocusLost( const FFocusEvent& InFocusEvent );
	virtual FReply NativeOnKeyChar( const FGeometry& InGeometry, const FCharacterEvent& InCharEvent );
	virtual FReply NativeOnPreviewKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent );
	virtual FReply NativeOnKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent );
	virtual FReply NativeOnKeyUp( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent );
	virtual FReply NativeOnAnalogValueChanged( const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent );
	virtual FReply NativeOnMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent );
	virtual FReply NativeOnPreviewMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent );
	virtual FReply NativeOnMouseButtonUp( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent );
	virtual FReply NativeOnMouseMove( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent );
	virtual void NativeOnMouseEnter( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent );
	virtual void NativeOnMouseLeave( const FPointerEvent& InMouseEvent );
	virtual FReply NativeOnMouseWheel( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent );
	virtual FReply NativeOnMouseButtonDoubleClick( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent );
	virtual void NativeOnDragDetected( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& InOperation );
	virtual void NativeOnDragEnter( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation );
	virtual void NativeOnDragLeave( const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation );
	virtual bool NativeOnDragOver( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation );
	virtual bool NativeOnDrop( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation );
	virtual void NativeOnDragCancelled( const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation );
	virtual FReply NativeOnTouchGesture( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent );
	virtual FReply NativeOnTouchStarted( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent );
	virtual FReply NativeOnTouchMoved( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent );
	virtual FReply NativeOnTouchEnded( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent );
	virtual FReply NativeOnMotionDetected( const FGeometry& InGeometry, const FMotionEvent& InMotionEvent );
	virtual FCursorReply NativeOnCursorQuery( const FGeometry& InGeometry, const FPointerEvent& InCursorEvent );

protected:
	/**
	 * Ticks the active sequences and latent actions that have been scheduled for this Widget.
	 */
	void TickActionsAndAnimation(const FGeometry& MyGeometry, float InDeltaTime);

	void RemoveObsoleteBindings(const TArray<FName>& NamedSlots);

	UUMGSequencePlayer* GetOrAddPlayer(const UWidgetAnimation* InAnimation);
	void Invalidate();
	
	/**
	 * Listens for a particular Player Input Action by name.  This requires that those actions are being executed, and
	 * that we're not currently in UI-Only Input Mode.
	 */
	UFUNCTION( BlueprintCallable, Category = "Input", meta = ( BlueprintProtected = "true" ) )
	void ListenForInputAction( FName ActionName, TEnumAsByte< EInputEvent > EventType, bool bConsume, FOnInputAction Callback );

	/**
	 * Removes the binding for a particular action's callback.
	 */
	UFUNCTION( BlueprintCallable, Category = "Input", meta = ( BlueprintProtected = "true" ) )
	void StopListeningForInputAction( FName ActionName, TEnumAsByte< EInputEvent > EventType );

	/**
	 * Stops listening to all input actions, and unregisters the input component with the player controller.
	 */
	UFUNCTION( BlueprintCallable, Category = "Input", meta = ( BlueprintProtected = "true" ) )
	void StopListeningForAllInputActions();

	/**
	 * ListenForInputAction will automatically Register an Input Component with the player input system.
	 * If you however, want to Pause and Resume, listening for a set of actions, the best way is to use
	 * UnregisterInputComponent to pause, and RegisterInputComponent to resume listening.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input", meta = ( BlueprintProtected = "true" ))
	void RegisterInputComponent();

	/**
	 * StopListeningForAllInputActions will automatically Register an Input Component with the player input system.
	 * If you however, want to Pause and Resume, listening for a set of actions, the best way is to use
	 * UnregisterInputComponent to pause, and RegisterInputComponent to resume listening.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input", meta = ( BlueprintProtected = "true" ))
	void UnregisterInputComponent();

	/**
	 * Checks if the action has a registered callback with the input component.
	 */
	UFUNCTION( BlueprintCallable, Category = "Input", meta = ( BlueprintProtected = "true" ) )
	bool IsListeningForInputAction( FName ActionName ) const;

	UFUNCTION( BlueprintCallable, Category = "Input", meta = ( BlueprintProtected = "true" ) )
	void SetInputActionPriority( int32 NewPriority );

	UFUNCTION( BlueprintCallable, Category = "Input", meta = ( BlueprintProtected = "true" ) )
	void SetInputActionBlocking( bool bShouldBlock );

	void OnInputAction( FOnInputAction Callback );

	virtual void InitializeInputComponent();

	UPROPERTY( Transient, DuplicateTransient )
	class UInputComponent* InputComponent;

private:
	FAnchors ViewportAnchors;
	FMargin ViewportOffsets;
	FVector2D ViewportAlignment;

	TWeakPtr<SWidget> FullScreenWidget;

	FLocalPlayerContext PlayerContext;

	/** Get World calls can be expensive for Widgets, we speed them up by caching the last found world until it goes away. */
	mutable TWeakObjectPtr<UWorld> CachedWorld;

	/** Has this widget been initialized by its class yet? */
	bool bInitialized;
};

#define LOCTEXT_NAMESPACE "UMG"

template< class T >
T* CreateWidget(UWorld* World, UClass* UserWidgetClass  = T::StaticClass() )
{
	if ( World == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("WorldNull", "Unable to create a widget outered to a null world."));
		return nullptr;
	}

	if ( !UserWidgetClass->IsChildOf(UUserWidget::StaticClass()) )
	{
		FMessageLog("PIE").Error(LOCTEXT("NotUserWidget", "CreateWidget can only be used on UUserWidget children."));
		return nullptr;
	}

	if ( UserWidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated) )
	{
		FMessageLog("PIE").Error(LOCTEXT("NotValidClass", "Abstract, Deprecated or Replaced classes are not allowed to be used to construct a user widget."));
		return nullptr;
	}

	// Assign the outer to the game instance if it exists, otherwise use the world
	UObject* Outer = World->GetGameInstance() ? StaticCast<UObject*>(World->GetGameInstance()) : StaticCast<UObject*>(World);
	UUserWidget* NewWidget = NewObject<UUserWidget>(Outer, UserWidgetClass);

	if ( ULocalPlayer* Player = World->GetFirstLocalPlayerFromController() )
	{
		NewWidget->SetPlayerContext(FLocalPlayerContext(Player));
	}

	NewWidget->Initialize();

	return Cast<T>(NewWidget);
}

template< class T >
T* CreateWidget(APlayerController* OwningPlayer, UClass* UserWidgetClass = T::StaticClass() )
{
	if ( OwningPlayer == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("PlayerNull", "Unable to create a widget outered to a null player controller."));
		return nullptr;
	}

	if ( !UserWidgetClass->IsChildOf(UUserWidget::StaticClass()) )
	{
		FMessageLog("PIE").Error(LOCTEXT("NotUserWidget", "CreateWidget can only be used on UUserWidget children."));
		return nullptr;
	}

	if ( UserWidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated) )
	{
		FMessageLog("PIE").Error(LOCTEXT("NotValidClass", "Abstract, Deprecated or Replaced classes are not allowed to be used to construct a user widget."));
		return nullptr;
	}

	if ( !OwningPlayer->IsLocalPlayerController() )
	{
		FMessageLog("PIE").Error(LOCTEXT("NotLocalPlayer", "Only Local Player Controllers can be assigned to widgets."));
		return nullptr;
	}

	if (!OwningPlayer->Player)
	{
		FMessageLog("PIE").Error(LOCTEXT("NoPLayer", "CreateWidget cannot be used on Player Controller with no attached player."));
		return nullptr;
	}

	// Assign the outer to the game instance if it exists, otherwise use the player controller's world
	UWorld* World = OwningPlayer->GetWorld();
	UObject* Outer = World->GetGameInstance() ? StaticCast<UObject*>(World->GetGameInstance()) : StaticCast<UObject*>(World);
	UUserWidget* NewWidget = NewObject<UUserWidget>(Outer, UserWidgetClass);
	
	NewWidget->SetPlayerContext(FLocalPlayerContext(OwningPlayer));
	NewWidget->Initialize();

	return Cast<T>(NewWidget);
}

template< class T >
T* CreateWidget(UGameInstance* OwningGame, UClass* UserWidgetClass = T::StaticClass() )
{
	if ( OwningGame == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("GameNull", "Unable to create a widget outered to a null game instance."));
		return nullptr;
	}

	if ( !UserWidgetClass->IsChildOf(UUserWidget::StaticClass()) )
	{
		FMessageLog("PIE").Error(LOCTEXT("NotUserWidget", "CreateWidget can only be used on UUserWidget children."));
		return nullptr;
	}

	if ( UserWidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated) )
	{
		FMessageLog("PIE").Error(LOCTEXT("NotValidClass", "Abstract, Deprecated or Replaced classes are not allowed to be used to construct a user widget."));
		return nullptr;
	}

	UUserWidget* NewWidget = NewObject<UUserWidget>(OwningGame, UserWidgetClass);

	if ( ULocalPlayer* Player = OwningGame->GetFirstGamePlayer() )
	{
		NewWidget->SetPlayerContext(FLocalPlayerContext(Player));
	}
	
	NewWidget->Initialize();

	return Cast<T>(NewWidget);
}

#undef LOCTEXT_NAMESPACE
