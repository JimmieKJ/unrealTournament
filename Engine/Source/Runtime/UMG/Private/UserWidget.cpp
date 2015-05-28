// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "UMGSequencePlayer.h"
#include "SceneViewport.h"
#include "WidgetAnimation.h"

#include "WidgetBlueprintLibrary.h"
#include "WidgetLayoutLibrary.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UUserWidget

UUserWidget::UUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ViewportAnchors = FAnchors(0, 0, 1, 1);
	Visiblity_DEPRECATED = Visibility = ESlateVisibility::SelfHitTestInvisible;

	bInitialized = false;
	bSupportsKeyboardFocus = true;
	ColorAndOpacity = FLinearColor::White;
	ForegroundColor = FSlateColor::UseForeground();

#if WITH_EDITORONLY_DATA
	bUseDesignTimeSize_DEPRECATED = false;
	bUseDesiredSizeAtDesignTime_DEPRECATED = false;
	DesignTimeSize = FVector2D(100, 100);
	PaletteCategory = LOCTEXT("UserCreated", "User Created");
	DesignSizeMode = EDesignPreviewSizeMode::FillScreen;
#endif
}

void UUserWidget::Initialize()
{
	// If it's not initialized initialize it, as long as it's not the CDO, we never initialize the CDO.
	if ( !bInitialized && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		bInitialized = true;

		// Only do this if this widget is of a blueprint class
		UWidgetBlueprintGeneratedClass* BGClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass());
		if ( BGClass != nullptr )
		{
			BGClass->InitializeWidget(this);
		}

		// Map the named slot bindings to the available slots.
		WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			if ( UNamedSlot* NamedWidet = Cast<UNamedSlot>(Widget) )
			{
				for ( FNamedSlotBinding& Binding : NamedSlotBindings )
				{
					if ( Binding.Content && Binding.Name == NamedWidet->GetFName() )
					{
						NamedWidet->ClearChildren();
						NamedWidet->AddChild(Binding.Content);
						return;
					}
				}
			}
		});
	}
}

void UUserWidget::BeginDestroy()
{
	Super::BeginDestroy();

	// If anyone ever calls BeginDestroy explicitly on a widget we need to immediately remove it from
	// the the parent as it may be owned currently by a slate widget.  As long as it's the viewport we're
	// fine.
	RemoveFromParent();

	// If it's not owned by the viewport we need to take more extensive measures.  If the GC widget still
	// exists after this point we should just reset the widget, which will forcefully cause the SObjectWidget
	// to lose access to this UObject.
	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		SafeGCWidget->ResetWidget();
	}
}

void UUserWidget::PostEditImport()
{
	Super::PostEditImport();

	Initialize();
}

void UUserWidget::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	Initialize();
}

void UUserWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	UWidget* RootWidget = GetRootWidget();
	if ( RootWidget )
	{
		RootWidget->ReleaseSlateResources(bReleaseChildren);
	}
}

void UUserWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// We get the GCWidget directly because MyWidget could be the fullscreen host widget if we've been added
	// to the viewport.
	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		TAttribute<FLinearColor> ColorBinding = OPTIONAL_BINDING(FLinearColor, ColorAndOpacity);
		TAttribute<FSlateColor> ForegroundColorBinding = OPTIONAL_BINDING(FSlateColor, ForegroundColor);

		SafeGCWidget->SetColorAndOpacity(ColorBinding);
		SafeGCWidget->SetForegroundColor(ForegroundColorBinding);
	}
}

void UUserWidget::SetColorAndOpacity(FLinearColor InColorAndOpacity)
{
	ColorAndOpacity = InColorAndOpacity;

	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		SafeGCWidget->SetColorAndOpacity(ColorAndOpacity);
	}
}

void UUserWidget::SetForegroundColor(FSlateColor InForegroundColor)
{
	ForegroundColor = InForegroundColor;

	TSharedPtr<SObjectWidget> SafeGCWidget = MyGCWidget.Pin();
	if ( SafeGCWidget.IsValid() )
	{
		SafeGCWidget->SetForegroundColor(ForegroundColor);
	}
}

void UUserWidget::PostInitProperties()
{
	Super::PostInitProperties();
}

UWorld* UUserWidget::GetWorld() const
{
	if ( HasAllFlags(RF_ClassDefaultObject) )
	{
		// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
		return nullptr;
	}

	// Use the Player Context's world, if a specific player context is given, otherwise fall back to
	// following the outer chain.
	if ( PlayerContext.IsValid() )
	{
		if ( UWorld* World = PlayerContext.GetWorld() )
		{
			return World;
		}
	}

	// Could be a GameInstance, could be World, could also be a WidgetTree, so we're just going to follow
	// the outer chain to find the world we're in.
	UObject* Outer = GetOuter();

	while ( Outer )
	{
		UWorld* World = Outer->GetWorld();
		if ( World )
		{
			return World;
		}

		Outer = Outer->GetOuter();
	}

	return nullptr;
}

void UUserWidget::SetIsDesignTime(bool bInDesignTime)
{
	Super::SetIsDesignTime(bInDesignTime);

	WidgetTree->ForEachWidget([&] (UWidget* Widget) {
		Widget->SetIsDesignTime(bInDesignTime);
	});
}

void UUserWidget::PlayAnimation( const UWidgetAnimation* InAnimation, float StartAtTime, int32 NumberOfLoops, EUMGSequencePlayMode::Type PlayMode)
{
	if( InAnimation )
	{
		// @todo UMG sequencer - Restart animations which have had Play called on them?
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate(
			[&](const UUMGSequencePlayer* Player)
			{
				return Player->GetAnimation() == InAnimation;
			});

		if( !FoundPlayer )
		{
			UUMGSequencePlayer* NewPlayer = NewObject<UUMGSequencePlayer>(this);
			ActiveSequencePlayers.Add( NewPlayer );

			NewPlayer->OnSequenceFinishedPlaying().AddUObject( this, &UUserWidget::OnAnimationFinishedPlaying );

			NewPlayer->InitSequencePlayer( *InAnimation, *this );

			NewPlayer->Play( StartAtTime, NumberOfLoops, PlayMode );
		}
		else
		{
			( *FoundPlayer )->Play( StartAtTime, NumberOfLoops, PlayMode );
		}

		OnAnimationStarted( InAnimation );
	}
}

void UUserWidget::StopAnimation(const UWidgetAnimation* InAnimation)
{
	if(InAnimation)
	{
		// @todo UMG sequencer - Restart animations which have had Play called on them?
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate([&](const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; } );

		if(FoundPlayer)
		{
			(*FoundPlayer)->Stop();
		}
	}
}

float UUserWidget::PauseAnimation(const UWidgetAnimation* InAnimation)
{
	if ( InAnimation )
	{
		// @todo UMG sequencer - Restart animations which have had Play called on them?
		UUMGSequencePlayer** FoundPlayer = ActiveSequencePlayers.FindByPredicate([&] (const UUMGSequencePlayer* Player) { return Player->GetAnimation() == InAnimation; });

		if ( FoundPlayer )
		{
			( *FoundPlayer )->Pause();
			return (float)( *FoundPlayer )->GetTimeCursorPosition();
		}
	}

	return 0;
}

void UUserWidget::OnAnimationFinishedPlaying( UUMGSequencePlayer& Player )
{
	OnAnimationFinished( Player.GetAnimation() );
	StoppedSequencePlayers.Add( &Player );
}

void UUserWidget::PlaySound(USoundBase* SoundToPlay)
{
	if (SoundToPlay)
	{
		FSlateSound NewSound;
		NewSound.SetResourceObject(SoundToPlay);
		FSlateApplication::Get().PlaySound(NewSound);
	}
}

UWidget* UUserWidget::GetWidgetHandle(TSharedRef<SWidget> InWidget)
{
	return WidgetTree->FindWidget(InWidget);
}

TSharedRef<SWidget> UUserWidget::RebuildWidget()
{
	// In the event this widget is replaced in memory by the blueprint compiler update
	// the widget won't be properly initialized, so we ensure it's initialized and initialize
	// it if it hasn't been.
	if ( !bInitialized )
	{
		Initialize();
	}

	// Setup the player context on sub user widgets, if we have a valid context
	if (PlayerContext.IsValid())
	{
		WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			if ( UUserWidget* UserWidget = Cast<UUserWidget>(Widget) )
			{
				UserWidget->SetPlayerContext(PlayerContext);
			}
		});
	}

	// Add the first component to the root of the widget surface.
	TSharedRef<SWidget> UserRootWidget = WidgetTree->RootWidget ? WidgetTree->RootWidget->TakeWidget() : TSharedRef<SWidget>(SNew(SSpacer));

	return UserRootWidget;
}

void UUserWidget::OnWidgetRebuilt()
{
	// When a user widget is rebuilt we can safely initialize the navigation now since all the slate
	// widgets should be held onto by a smart pointer at this point.
	WidgetTree->ForEachWidget([&] (UWidget* Widget) {
		Widget->BuildNavigation();
	});

	if (!IsDesignTime())
	{
		// Notify the widget that it has been constructed.
		NativeConstruct();
	}
}

TSharedPtr<SWidget> UUserWidget::GetSlateWidgetFromName(const FName& Name) const
{
	UWidget* WidgetObject = WidgetTree->FindWidget(Name);
	if ( WidgetObject )
	{
		return WidgetObject->GetCachedWidget();
	}

	return TSharedPtr<SWidget>();
}

UWidget* UUserWidget::GetWidgetFromName(const FName& Name) const
{
	return WidgetTree->FindWidget(Name);
}

void UUserWidget::GetSlotNames(TArray<FName>& SlotNames) const
{
	// Only do this if this widget is of a blueprint class
	if ( UWidgetBlueprintGeneratedClass* BGClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()) )
	{
		SlotNames.Append(BGClass->NamedSlots);
	}
	else // For non-blueprint widget blueprints we have to go through the widget tree to locate the named slots dynamically.
	{
		TArray<FName> NamedSlots;
		WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			if ( Widget && Widget->IsA<UNamedSlot>() )
			{
				NamedSlots.Add(Widget->GetFName());
			}
		});
	}
}

UWidget* UUserWidget::GetContentForSlot(FName SlotName) const
{
	for ( const FNamedSlotBinding& Binding : NamedSlotBindings )
	{
		if ( Binding.Name == SlotName )
		{
			return Binding.Content;
		}
	}

	return nullptr;
}

void UUserWidget::SetContentForSlot(FName SlotName, UWidget* Content)
{
	// Find the binding in the existing set and replace the content for that binding.
	for ( int32 BindingIndex = 0; BindingIndex < NamedSlotBindings.Num(); BindingIndex++)
	{
		FNamedSlotBinding& Binding = NamedSlotBindings[BindingIndex];

		if ( Binding.Name == SlotName )
		{
			if ( Content )
			{
				Binding.Content = Content;
			}
			else
			{
				NamedSlotBindings.RemoveAt(BindingIndex);
			}

			return;
		}
	}

	if ( Content )
	{
		// Add the new binding to the list of bindings.
		FNamedSlotBinding NewBinding;
		NewBinding.Name = SlotName;
		NewBinding.Content = Content;

		NamedSlotBindings.Add(NewBinding);
	}

	// Dynamically insert the new widget into the hierarchy if it exists.
	if ( WidgetTree )
	{
		UNamedSlot* NamedSlot = Cast<UNamedSlot>(WidgetTree->FindWidget(SlotName));
		NamedSlot->ClearChildren();
		NamedSlot->AddChild(Content);
	}
}

TSharedRef<SWidget> UUserWidget::MakeViewportWidget(TSharedPtr<SWidget>& UserSlateWidget)
{
	UserSlateWidget = TakeWidget();

	return SNew(SConstraintCanvas)

		+ SConstraintCanvas::Slot()
		.Offset(BIND_UOBJECT_ATTRIBUTE(FMargin, GetFullScreenOffset))
		.Anchors(BIND_UOBJECT_ATTRIBUTE(FAnchors, GetViewportAnchors))
		.Alignment(BIND_UOBJECT_ATTRIBUTE(FVector2D, GetFullScreenAlignment))
		[
			UserSlateWidget.ToSharedRef()
		];
}

UWidget* UUserWidget::GetRootWidget() const
{
	if ( WidgetTree )
	{
		return WidgetTree->RootWidget;
	}

	return nullptr;
}

void UUserWidget::AddToViewport(int32 ZOrder)
{
	AddToScreen(nullptr, ZOrder);
}

bool UUserWidget::AddToPlayerScreen(int32 ZOrder)
{
	if ( ULocalPlayer* LocalPlayer = GetOwningLocalPlayer() )
	{
		AddToScreen(LocalPlayer, ZOrder);
		return true;
	}

	FMessageLog("PIE").Error(LOCTEXT("AddToPlayerScreen_NoPlayer", "AddToPlayerScreen Failed.  No Owning Player!"));
	return false;
}

void UUserWidget::AddToScreen(ULocalPlayer* Player, int32 ZOrder)
{
	if ( !FullScreenWidget.IsValid() )
	{
		TSharedPtr<SWidget> OutUserSlateWidget;
		TSharedRef<SWidget> RootWidget = MakeViewportWidget(OutUserSlateWidget);

		FullScreenWidget = RootWidget;

		// If this is a game world add the widget to the current worlds viewport.
		UWorld* World = GetWorld();
		if ( World && World->IsGameWorld() )
		{
			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				if ( Player )
				{
					ViewportClient->AddViewportWidgetForPlayer(Player, RootWidget, ZOrder);
				}
				else
				{
					// We add 10 to the zorder when adding to the viewport to avoid 
					// displaying below any built-in controls, like the virtual joysticks on mobile builds.
					ViewportClient->AddViewportWidgetContent(RootWidget, ZOrder + 10);
				}

				// Widgets added to the viewport are automatically removed if the persistant level is unloaded.
				FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UUserWidget::OnLevelRemovedFromWorld);
			}
		}
	}
}

void UUserWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	// If the InLevel is null, it's a signal that the entire world is about to disappear, so
	// go ahead and remove this widget from the viewport, it could be holding onto too many
	// dangerous actor references that won't carry over into the next world.
	if ( InLevel == nullptr && InWorld == GetWorld() )
	{
		RemoveFromParent();
		MarkPendingKill();
	}
}

void UUserWidget::RemoveFromViewport()
{
	RemoveFromParent();
}

void UUserWidget::RemoveFromParent()
{
	if ( FullScreenWidget.IsValid() )
	{
		TSharedPtr<SWidget> WidgetHost = FullScreenWidget.Pin();

		// If this is a game world add the widget to the current worlds viewport.
		UWorld* World = GetWorld();
		if ( World && World->IsGameWorld() )
		{
			if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
			{
				TSharedRef<SWidget> WidgetHostRef = WidgetHost.ToSharedRef();

				ViewportClient->RemoveViewportWidgetContent(WidgetHostRef);

				if ( ULocalPlayer* LocalPlayer = GetOwningLocalPlayer() )
				{
					ViewportClient->RemoveViewportWidgetForPlayer(LocalPlayer, WidgetHostRef);
				}

				FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
			}
		}
	}
	else
	{
		Super::RemoveFromParent();
	}
}

bool UUserWidget::GetIsVisible() const
{
	return FullScreenWidget.IsValid();
}

bool UUserWidget::IsInViewport() const
{
	return FullScreenWidget.IsValid();
}

void UUserWidget::SetPlayerContext(FLocalPlayerContext InPlayerContext)
{
	PlayerContext = InPlayerContext;
}

const FLocalPlayerContext& UUserWidget::GetPlayerContext() const
{
	return PlayerContext;
}

ULocalPlayer* UUserWidget::GetOwningLocalPlayer() const
{
	APlayerController* PC = PlayerContext.IsValid() ? PlayerContext.GetPlayerController() : nullptr;
	return PC ? Cast<ULocalPlayer>(PC->Player) : nullptr;
}

void UUserWidget::SetOwningLocalPlayer(ULocalPlayer* LocalPlayer)
{
	if ( LocalPlayer )
	{
		PlayerContext = FLocalPlayerContext(LocalPlayer);
	}
}

APlayerController* UUserWidget::GetOwningPlayer() const
{
	return PlayerContext.IsValid() ? PlayerContext.GetPlayerController() : nullptr;
}

class APawn* UUserWidget::GetOwningPlayerPawn() const
{
	if ( APlayerController* PC = GetOwningPlayer() )
	{
		return PC->GetPawn();
	}

	return nullptr;
}

void UUserWidget::SetPositionInViewport(FVector2D Position, bool bRemoveDPIScale )
{
	if ( bRemoveDPIScale )
	{
		float Scale = UWidgetLayoutLibrary::GetViewportScale(this);

		ViewportOffsets.Left = Position.X / Scale;
		ViewportOffsets.Top = Position.Y / Scale;
	}
	else
	{
		ViewportOffsets.Left = Position.X;
		ViewportOffsets.Top = Position.Y;
	}

	ViewportAnchors = FAnchors(0, 0);
}

void UUserWidget::SetDesiredSizeInViewport(FVector2D DesiredSize)
{
	ViewportOffsets.Right = DesiredSize.X;
	ViewportOffsets.Bottom = DesiredSize.Y;

	ViewportAnchors = FAnchors(0, 0);
}

void UUserWidget::SetAnchorsInViewport(FAnchors Anchors)
{
	ViewportAnchors = Anchors;
}

void UUserWidget::SetAlignmentInViewport(FVector2D Alignment)
{
	ViewportAlignment = Alignment;
}

FMargin UUserWidget::GetFullScreenOffset() const
{
	// If the size is zero, and we're not stretched, then use the desired size.
	FVector2D FinalSize = FVector2D(ViewportOffsets.Right, ViewportOffsets.Bottom);
	if ( FinalSize.IsZero() && !ViewportAnchors.IsStretchedVertical() && !ViewportAnchors.IsStretchedHorizontal() )
	{
		TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
		if ( CachedWidget.IsValid() )
		{
			FinalSize = CachedWidget->GetDesiredSize();
		}
	}

	return FMargin(ViewportOffsets.Left, ViewportOffsets.Top, FinalSize.X, FinalSize.Y);
}

FAnchors UUserWidget::GetViewportAnchors() const
{
	return ViewportAnchors;
}

FVector2D UUserWidget::GetFullScreenAlignment() const
{
	return ViewportAlignment;
}

void UUserWidget::PreSave()
{
	Super::PreSave();

	// Remove bindings that are no longer contained in the class.
	if ( UWidgetBlueprintGeneratedClass* BGClass = Cast<UWidgetBlueprintGeneratedClass>(GetClass()) )
	{
		for ( int32 BindingIndex = 0; BindingIndex < NamedSlotBindings.Num(); BindingIndex++ )
		{
			const FNamedSlotBinding& Binding = NamedSlotBindings[BindingIndex];

			if ( !BGClass->NamedSlots.Contains(Binding.Name) )
			{
				NamedSlotBindings.RemoveAt(BindingIndex);
				BindingIndex--;
			}
		}
	}
}

#if WITH_EDITOR

const FSlateBrush* UUserWidget::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.UserWidget");
}

const FText UUserWidget::GetPaletteCategory()
{
	return PaletteCategory;
}

#endif

void UUserWidget::OnAnimationStarted_Implementation(const UWidgetAnimation* Animation)
{

}

void UUserWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{

}

// Native handling for SObjectWidget

void UUserWidget::NativeConstruct()
{
	Construct();
}

void UUserWidget::NativeDestruct()
{
	Destruct();
}

void UUserWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	GInitRunaway();

	TickActionsAndAnimation(MyGeometry, InDeltaTime);

	Tick(MyGeometry, InDeltaTime);
}

void UUserWidget::TickActionsAndAnimation(const FGeometry& MyGeometry, float InDeltaTime)
{
	if ( bDesignTime )
	{
		return;
	}

	// Update active movie scenes
	for ( UUMGSequencePlayer* Player : ActiveSequencePlayers )
	{
		Player->Tick(InDeltaTime);
	}

	// The process of ticking the players above can stop them so we remove them after all players have ticked
	for ( UUMGSequencePlayer* StoppedPlayer : StoppedSequencePlayers )
	{
		ActiveSequencePlayers.Remove(StoppedPlayer);
	}

	StoppedSequencePlayers.Empty();

	UWorld* World = GetWorld();
	if ( World )
	{
		// Update any latent actions we have for this actor
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		LatentActionManager.ProcessLatentActions(this, InDeltaTime);
	}
}

void UUserWidget::NativePaint( FPaintContext& InContext ) const 
{
	OnPaint( InContext );
}

bool UUserWidget::NativeIsInteractable() const
{
	return IsInteractable();
}

bool UUserWidget::NativeSupportsKeyboardFocus() const
{
	return bSupportsKeyboardFocus;
}

FReply UUserWidget::NativeOnFocusReceived( const FGeometry& InGeometry, const FFocusEvent& InFocusEvent )
{
	return OnFocusReceived( InGeometry, InFocusEvent ).NativeReply;
}

void UUserWidget::NativeOnFocusLost( const FFocusEvent& InFocusEvent )
{
	OnFocusLost( InFocusEvent );
}

FReply UUserWidget::NativeOnKeyChar( const FGeometry& InGeometry, const FCharacterEvent& InCharEvent )
{
	return OnKeyChar( InGeometry, InCharEvent ).NativeReply;
}

FReply UUserWidget::NativeOnPreviewKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	return OnPreviewKeyDown( InGeometry, InKeyEvent ).NativeReply;
}

FReply UUserWidget::NativeOnKeyDown( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	return OnKeyDown( InGeometry, InKeyEvent ).NativeReply;
}

FReply UUserWidget::NativeOnKeyUp( const FGeometry& InGeometry, const FKeyEvent& InKeyEvent )
{
	return OnKeyUp( InGeometry, InKeyEvent ).NativeReply;
}

FReply UUserWidget::NativeOnAnalogValueChanged( const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent )
{
	return OnAnalogValueChanged( InGeometry, InAnalogEvent ).NativeReply;
}

FReply UUserWidget::NativeOnMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDown( InGeometry, InMouseEvent ).NativeReply;
}

FReply UUserWidget::NativeOnPreviewMouseButtonDown( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnPreviewMouseButtonDown( InGeometry, InMouseEvent ).NativeReply;
}

FReply UUserWidget::NativeOnMouseButtonUp( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonUp(InGeometry, InMouseEvent).NativeReply;
}

FReply UUserWidget::NativeOnMouseMove( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseMove( InGeometry, InMouseEvent ).NativeReply;
}

void UUserWidget::NativeOnMouseEnter( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	OnMouseEnter( InGeometry, InMouseEvent );
}

void UUserWidget::NativeOnMouseLeave( const FPointerEvent& InMouseEvent )
{
	OnMouseLeave( InMouseEvent );
}

FReply UUserWidget::NativeOnMouseWheel( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseWheel( InGeometry, InMouseEvent ).NativeReply;
}

FReply UUserWidget::NativeOnMouseButtonDoubleClick( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDoubleClick( InGeometry, InMouseEvent ).NativeReply;
}

void UUserWidget::NativeOnDragDetected( const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& InOperation )
{
	OnDragDetected( InGeometry, InMouseEvent, InOperation );
}

void UUserWidget::NativeOnDragEnter( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	OnDragEnter( InGeometry, InDragDropEvent, InOperation );
}

void UUserWidget::NativeOnDragLeave( const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	OnDragLeave( InDragDropEvent, InOperation );
}

bool UUserWidget::NativeOnDragOver( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	return OnDragOver( InGeometry, InDragDropEvent, InOperation );
}

bool UUserWidget::NativeOnDrop( const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	return OnDrop( InGeometry, InDragDropEvent, InOperation );
}

void UUserWidget::NativeOnDragCancelled( const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation )
{
	OnDragCancelled( InDragDropEvent, InOperation );
}

FReply UUserWidget::NativeOnTouchGesture( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent )
{
	return OnTouchGesture( InGeometry, InGestureEvent ).NativeReply;
}

FReply UUserWidget::NativeOnTouchStarted( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent )
{
	return OnTouchStarted( InGeometry, InGestureEvent ).NativeReply;
}

FReply UUserWidget::NativeOnTouchMoved( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent )
{
	return OnTouchMoved( InGeometry, InGestureEvent ).NativeReply;
}

FReply UUserWidget::NativeOnTouchEnded( const FGeometry& InGeometry, const FPointerEvent& InGestureEvent )
{
	return OnTouchEnded( InGeometry, InGestureEvent ).NativeReply;
}

FReply UUserWidget::NativeOnMotionDetected( const FGeometry& InGeometry, const FMotionEvent& InMotionEvent )
{
	return OnMotionDetected( InGeometry, InMotionEvent ).NativeReply;
}

void UUserWidget::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_USER_WIDGET_DESIGN_SIZE )
	{
		if ( bUseDesignTimeSize_DEPRECATED )
		{
			DesignSizeMode = EDesignPreviewSizeMode::Custom;
		}
		else if ( bUseDesiredSizeAtDesignTime_DEPRECATED )
		{
			DesignSizeMode = EDesignPreviewSizeMode::Desired;
		}
	}

#endif
}

/////////////////////////////////////////////////////
//
// DEPRECATED
//
/////////////////////////////////////////////////////

void UUserWidget::Construct_Implementation()
{

}

void UUserWidget::Tick_Implementation(FGeometry MyGeometry, float InDeltaTime)
{

}

void UUserWidget::OnPaint_Implementation(FPaintContext& Context) const
{

}

FEventReply UUserWidget::OnFocusReceived_Implementation(FGeometry MyGeometry, FFocusEvent InFocusEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

void UUserWidget::OnFocusLost_Implementation(FFocusEvent InFocusEvent)
{

}

FEventReply UUserWidget::OnKeyChar_Implementation(FGeometry MyGeometry, FCharacterEvent InCharacterEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnPreviewKeyDown_Implementation(FGeometry MyGeometry, FKeyEvent InKeyEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnKeyDown_Implementation(FGeometry MyGeometry, FKeyEvent InKeyEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnKeyUp_Implementation(FGeometry MyGeometry, FKeyEvent InKeyEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnAnalogValueChanged_Implementation(FGeometry MyGeometry, FAnalogInputEvent InAnalogInputEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnMouseButtonDown_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnPreviewMouseButtonDown_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnMouseButtonUp_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnMouseMove_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

void UUserWidget::OnMouseEnter_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{

}

void UUserWidget::OnMouseLeave_Implementation(const FPointerEvent& MouseEvent)
{

}

FEventReply UUserWidget::OnMouseWheel_Implementation(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnMouseButtonDoubleClick_Implementation(FGeometry InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

void UUserWidget::OnDragDetected_Implementation(FGeometry MyGeometry, const FPointerEvent& PointerEvent, UDragDropOperation*& Operation)
{

}

void UUserWidget::OnDragCancelled_Implementation(const FPointerEvent& PointerEvent, UDragDropOperation* Operation)
{

}

void UUserWidget::OnDragEnter_Implementation(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation)
{

}

void UUserWidget::OnDragLeave_Implementation(FPointerEvent PointerEvent, UDragDropOperation* Operation)
{

}

bool UUserWidget::OnDragOver_Implementation(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation)
{
	return false;
}

bool UUserWidget::OnDrop_Implementation(FGeometry MyGeometry, FPointerEvent PointerEvent, UDragDropOperation* Operation)
{
	return false;
}

FEventReply UUserWidget::OnTouchGesture_Implementation(FGeometry MyGeometry, const FPointerEvent& GestureEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnTouchStarted_Implementation(FGeometry MyGeometry, const FPointerEvent& InTouchEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnTouchMoved_Implementation(FGeometry MyGeometry, const FPointerEvent& InTouchEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnTouchEnded_Implementation(FGeometry MyGeometry, const FPointerEvent& InTouchEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

FEventReply UUserWidget::OnMotionDetected_Implementation(FGeometry MyGeometry, FMotionEvent InMotionEvent)
{
	return UWidgetBlueprintLibrary::Unhandled();
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
