// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorFloatingUI.h"
#include "VREditorUISystem.h"
#include "VREditorBaseUserWidget.h"
#include "VREditorMode.h"
#include "WidgetComponent.h"
#include "VREditorWidgetComponent.h"
#include "VREditorInteractor.h"
#include "SLevelViewport.h"	// For tab manager tricks


namespace VREd
{
	static FAutoConsoleVariable UIFadeSpeed( TEXT( "VREd.UIFadeSpeed" ), 6.0f, TEXT( "How fast UI should fade in and out" ) );
	static FAutoConsoleVariable UIBrightness( TEXT( "VREd.UIBrightness" ), 0.3f, TEXT( "How bright the UI should be" ) );
}


AVREditorFloatingUI::AVREditorFloatingUI()
	: Super(),
	SlateWidget( nullptr ),
	UserWidget( nullptr ),
	WidgetComponent( nullptr ),
	Resolution( 0, 0 ),
	Owner( nullptr ),
	UserWidgetClass( nullptr ),
	bShouldBeVisible(),
	FadeAlpha( 1.0f ),
	bCollisionOnShowUI( true ),
	FadeDelay( 0.0f ),
	InitialScale( 1.0f )
{
	const bool bTransient = true;
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>( TEXT( "SceneComponent" ), bTransient );
	check( SceneComponent != nullptr );
	this->RootComponent = SceneComponent;

	WidgetComponent = CreateDefaultSubobject<UVREditorWidgetComponent>( TEXT( "WidgetComponent" ), bTransient );
	WidgetComponent->SetEditTimeUsable(true);
	WidgetComponent->SetupAttachment( SceneComponent );

	InitialScale = Scale;
}


void AVREditorFloatingUI::SetupWidgetComponent()
{
	WidgetComponent->SetTwoSided( false );	// No VR UI is two-sided

	if( SlateWidget.IsValid() )
	{
		// Slate UIs have bogus opacity in their texture's alpha, so ignore texture alpha for VR
		WidgetComponent->SetOpacityFromTexture( 0.0f );	// Slate UIs have bogus opacity in their texture's alpha, so ignore texture alpha for VR
		WidgetComponent->SetBackgroundColor( FLinearColor::Black );
		WidgetComponent->SetBlendMode( EWidgetBlendMode::Opaque );
	}
	else
	{
		WidgetComponent->SetOpacityFromTexture( 1.0f );
		WidgetComponent->SetBackgroundColor( FLinearColor::Transparent );
		WidgetComponent->SetBlendMode( EWidgetBlendMode::Masked );
	}

	// @todo vreditor: Ideally we use automatic mip map generation, otherwise the UI looks too crunchy at a distance.
	// However, I tried this and on D3D11 the mips are all black.
	WidgetComponent->SetDrawSize( FVector2D( Resolution.X, Resolution.Y ) );		// NOTE: Must be called before RegisterComponent() because collision data will be created during registration

	// NOTE: Must be called *after* RegisterComponent() because UWidgetComponent nulls out Widget if no WidgetClass is set (WidgetClass is protected and there is no accessor)
	if( SlateWidget.IsValid() )
	{
		WidgetComponent->SetSlateWidget( SlateWidget.ToSharedRef() );
	}
	else
	{
		// @todo vreditor unreal: Ideally we would do this in the constructor and not again after.  Due to an apparent bug in UMG, 
		// we need to re-create the widget in lock-step with the WidgetComponent, otherwise input doesn't function correctly on the 
		// widget after the widget component is destroyed and recreated with the same user widget.
		check( UserWidgetClass != nullptr );
		UserWidget = CreateWidget<UVREditorBaseUserWidget>( GetWorld(), UserWidgetClass );
		check( UserWidget != nullptr );
		UserWidget->SetOwner( this );

		WidgetComponent->SetWidget( UserWidget );
	}

	// @todo vreditor: Is this useful?
	//WidgetComponent->SetMaxInteractionDistance( 10000.0f );

	// Default to visible
	ShowUI( true );

	// Set initial opacity
	UpdateFadingState( 0.0f );

	// Set initial transform
	UpdateTransformIfDocked();
}

void AVREditorFloatingUI::SetSlateWidget( UVREditorUISystem& InitOwner, const TSharedRef<SWidget>& InitSlateWidget, const FIntPoint InitResolution, const float InitScale, const EDockedTo InitDockedTo )
{
	Owner = &InitOwner;
	SetVRMode( &Owner->GetOwner() );

	SlateWidget = InitSlateWidget;

	Resolution = InitResolution;
	check( Resolution.X > 0 && Resolution.Y > 0 );

	Scale = InitScale;
	InitialScale = Scale;

	SetDockedTo( InitDockedTo );

	SetupWidgetComponent();
}


void AVREditorFloatingUI::SetUMGWidget( UVREditorUISystem& InitOwner, TSubclassOf<UVREditorBaseUserWidget> InitUserWidgetClass, const FIntPoint InitResolution, const float InitScale, const EDockedTo InitDockedTo )
{
	Owner = &InitOwner;
	SetVRMode( &Owner->GetOwner() );

	check( InitUserWidgetClass != nullptr );
	UserWidgetClass = InitUserWidgetClass;

	Resolution = InitResolution;
	check( Resolution.X > 0 && Resolution.Y > 0 );

	Scale = InitScale;
	InitialScale = Scale;

	SetDockedTo( InitDockedTo );

	SetupWidgetComponent();
}

void AVREditorFloatingUI::Destroyed()
{
	if( WidgetComponent != nullptr )
	{
		// NOTE: We're nulling out widgets so that we don't have to wait for a GC to free up Slate resources (avoid shutdown crash)
		WidgetComponent->SetSlateWidget( nullptr );
		WidgetComponent->SetWidget( nullptr );
		WidgetComponent = nullptr;
	}

	this->SlateWidget = nullptr;

	// @todo vreditor unreal: UMG has a bug that prevents you from re-using the user widget for a new widget component
	// after a previous widget component that was using it was destroyed
	if( UserWidget != nullptr )
	{
		UserWidget->MarkPendingKill();
		UserWidget = nullptr;
	}

	Super::Destroyed();
}

void AVREditorFloatingUI::SetTransform( const FTransform& Transform )
{
	const FVector AnimatedScale = CalculateAnimatedScale();

	FTransform AnimatedTransform = Transform;
	AnimatedTransform.SetScale3D( AnimatedTransform.GetScale3D() * AnimatedScale );

	const float Aspect = ( float ) Resolution.X / ( float ) Resolution.Y;

	RootComponent->SetWorldLocation( AnimatedTransform.GetLocation() );
	RootComponent->SetWorldRotation( AnimatedTransform.GetRotation() );
	WidgetComponent->SetWorldScale3D( FVector( 1.0f / AnimatedTransform.GetScale3D().X, 1.0f / ( float ) Resolution.X, 1.0f / ( float ) Resolution.Y / Aspect ) * AnimatedTransform.GetScale3D() );
}


void AVREditorFloatingUI::UpdateFadingState( const float DeltaTime )
{
	if ( FadeDelay > 0.f )
	{
		FadeDelay -= DeltaTime;
	}
	else
	{
		if( bShouldBeVisible.GetValue() )
		{
			FadeAlpha += VREd::UIFadeSpeed->GetFloat() * DeltaTime;
		}
		else
		{
			FadeAlpha -= VREd::UIFadeSpeed->GetFloat() * DeltaTime;
		}
		FadeAlpha = FMath::Clamp( FadeAlpha, 0.0f, 1.0f );

		if( FadeAlpha > 0.0f + KINDA_SMALL_NUMBER )
		{
			// At least a little bit visible
			if( bHidden )
			{
				SetActorHiddenInGame( false );
				WidgetComponent->SetVisibility( true );
				FadeDelay = 0.0f;
			}
		}

		if( FadeAlpha >= 1.0f - KINDA_SMALL_NUMBER )
		{
			// Fully visible
		}
		else if( FadeAlpha <= 0.0f + KINDA_SMALL_NUMBER )
		{
			// Fully invisible
			if( !bHidden )
			{
				SetActorHiddenInGame( true );
				WidgetComponent->SetVisibility( false );
				FadeDelay = 0.0f;
			}
		}

 		// Set material color
		// NOTE: We intentionally make the UI quite dark here, so it doesn't bloom out in HDR
		const float UIBrightness = FadeAlpha * VREd::UIBrightness->GetFloat();
		WidgetComponent->SetTintColorAndOpacity( FLinearColor( UIBrightness, UIBrightness, UIBrightness ).CopyWithNewOpacity( FadeAlpha ) );
	}
}

FVector AVREditorFloatingUI::CalculateAnimatedScale() const
{
	const float AnimationOvershootAmount = 0.7f;	// @todo vreditor tweak
	const float EasedAlpha = FVREditorMode::OvershootEaseOut( FadeAlpha, AnimationOvershootAmount );

	// Animate vertically more than horizontally; just looks a little better
	const float ZScale = FMath::Max( 0.001f, EasedAlpha );
	const float YScale = FMath::Max( 0.001f, 0.7f + 0.3f * EasedAlpha );

	FVector AnimatedScale = FVector( 1.0f, YScale, ZScale );
	AnimatedScale.Y *= YScale;
	AnimatedScale.Z *= ZScale;

	return AnimatedScale;
}

void AVREditorFloatingUI::SetCollisionOnShow( const bool bInCollisionOnShow )
{
	bCollisionOnShowUI = bInCollisionOnShow;
}

float AVREditorFloatingUI::GetInitialScale() const
{
	return InitialScale;
}


void AVREditorFloatingUI::ShowUI( const bool bShow, const bool bAllowFading, const float InitFadeDelay )
{
	if( !bShouldBeVisible.IsSet() || bShow != bShouldBeVisible.GetValue() )
	{
		bShouldBeVisible = bShow;

		if( bCollisionOnShowUI )
		{
			SetActorEnableCollision( bShow );
		}

		if( !bAllowFading )
		{
			SetActorHiddenInGame( !bShow );
			WidgetComponent->SetVisibility( bShow );
			FadeAlpha = bShow ? 1.0f : 0.0f;
		}

		FadeDelay = InitFadeDelay;
	}
}


FVector2D AVREditorFloatingUI::GetSize() const
{
	const float Aspect = (float)Resolution.X / (float)Resolution.Y;
	return FVector2D( Scale, Scale / Aspect );
}

float AVREditorFloatingUI::GetScale() const
{
	return Scale;
}

void AVREditorFloatingUI::SetScale( const float NewSize )
{
	Scale = NewSize;

	const float WorldScaleFactor = Owner->GetOwner().GetWorldScaleFactor();
	const FVector NewScale( Scale * WorldScaleFactor );
	const float Aspect = (float)Resolution.X / (float)Resolution.Y;
	WidgetComponent->SetWorldScale3D( FVector( 1.0f / NewScale.X, 1.0f / (float)Resolution.X, 1.0f / (float)Resolution.Y / Aspect ) * NewScale );
}