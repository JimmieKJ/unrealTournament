// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorAvatarActor.h"
#include "VREditorMode.h"
#include "VREditorInteractor.h"
#include "ViewportWorldInteraction.h"
#include "IHeadMountedDisplay.h"

namespace VREd
{
	static FAutoConsoleVariable GridMovementTolerance( TEXT( "VREd.GridMovementTolerance" ), 0.1f, TEXT( "Tolerance for movement when the grid must disappear" ) );
	static FAutoConsoleVariable GridScaleMultiplier( TEXT( "VREd.GridScaleMultiplier" ), 35.0f, TEXT( "Scale of the grid" ) );
	static FAutoConsoleVariable GridFadeMultiplier( TEXT( "VREd.GridFadeMultiplier" ), 3.0f, TEXT( "Grid fade in/out speed, in 'fades per second'" ) );
	static FAutoConsoleVariable GridFadeStartVelocity( TEXT( "VREd.GridFadeStartVelocity" ), 10.f, TEXT( "Grid fade duration" ) );
	static FAutoConsoleVariable GridMaxOpacity( TEXT( "VREd.GridMaxFade" ), 0.8f, TEXT( "Grid maximum opacity" ) );
	static FAutoConsoleVariable GridHeightOffset( TEXT( "VREd.GridHeightOffset" ), 0.0f, TEXT( "Height offset for the world movement grid.  Useful when tracking space is not properly calibrated" ) );

	static FAutoConsoleVariable WorldMovementFogOpacity( TEXT( "VREd.WorldMovementFogOpacity" ), 0.8f, TEXT( "How opaque the fog should be at the 'end distance' (0.0 - 1.0)" ) );
	static FAutoConsoleVariable WorldMovementFogStartDistance( TEXT( "VREd.WorldMovementFogStartDistance" ), 300.0f, TEXT( "How far away fog will start rendering while in world movement mode" ) );
	static FAutoConsoleVariable WorldMovementFogEndDistance( TEXT( "VREd.WorldMovementFogEndDistance" ), 800.0f, TEXT( "How far away fog will finish rendering while in world movement mode" ) );
	static FAutoConsoleVariable WorldMovementFogSkyboxDistance( TEXT( "VREd.WorldMovementFogSkyboxDistance" ), 20000.0f, TEXT( "Anything further than this distance will be completed fogged and not visible" ) );

	static FAutoConsoleVariable ScaleProgressBarLength( TEXT( "VREd.ScaleProgressBarLength" ), 50.0f, TEXT( "Length of the progressbar that appears when scaling" ) );
	static FAutoConsoleVariable ScaleProgressBarRadius( TEXT( "VREd.ScaleProgressBarRadius" ), 1.0f, TEXT( "Radius of the progressbar that appears when scaling" ) );

	static FAutoConsoleVariable ShowMovementGrid( TEXT( "VREd.ShowMovementGrid" ), 1, TEXT( "Showing the ground movement grid" ) );
	static FAutoConsoleVariable ShowWorldMovementPostProcess( TEXT( "VREd.ShowWorldMovementPostProcess" ), 1, TEXT( "Showing the movement post processing" ) );
}

AVREditorAvatarActor::AVREditorAvatarActor( const FObjectInitializer& ObjectInitializer ) :
	Super( ObjectInitializer ),
	WorldMovementGridOpacity( 0.0f ),
	bIsDrawingWorldMovementPostProcess( false ),
	VRMode( nullptr )
{
	// Give us a head mesh
	{
		HeadMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "HeadMeshComponent" ) );
		AddOwnedComponent( HeadMeshComponent );
		HeadMeshComponent->SetupAttachment( RootComponent );

		// @todo vreditor: This needs to adapt based on the device you're using
		UStaticMesh* HeadMesh = LoadObject<UStaticMesh>( nullptr, TEXT( "/Engine/VREditor/Devices/Generic/GenericHMD" ) );
		check( HeadMesh != nullptr );

		HeadMeshComponent->SetStaticMesh( HeadMesh );
		HeadMeshComponent->SetMobility( EComponentMobility::Movable );
		HeadMeshComponent->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	}

	// World movement grid mesh
	{
		WorldMovementGridMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "WorldMovementGridMeshComponent" ) );
		AddOwnedComponent( WorldMovementGridMeshComponent );
		WorldMovementGridMeshComponent->SetupAttachment( RootComponent );
	
		UStaticMesh* GridMesh = LoadObject<UStaticMesh>( nullptr, TEXT( "/Engine/VREditor/WorldMovementGrid/PlaneMesh" ) );
		check( GridMesh != nullptr );
		WorldMovementGridMeshComponent->SetStaticMesh( GridMesh );
		WorldMovementGridMeshComponent->SetMobility( EComponentMobility::Movable );
		WorldMovementGridMeshComponent->SetCollisionEnabled( ECollisionEnabled::NoCollision );

		UMaterialInterface* GridMaterial = LoadObject<UMaterialInterface>( nullptr, TEXT( "/Engine/VREditor/WorldMovementGrid/GridMaterial" ) );
		check( GridMaterial != nullptr );

		WorldMovementGridMID = UMaterialInstanceDynamic::Create( GridMaterial, GetTransientPackage( ) );
		check( WorldMovementGridMID != nullptr );
		WorldMovementGridMeshComponent->SetMaterial( 0, WorldMovementGridMID );

		// The grid starts off hidden
		WorldMovementGridMeshComponent->SetVisibility( false );
	}

	{
		{
			UMaterialInterface* UserScaleIndicatorMaterial = LoadObject<UMaterialInterface>( nullptr, TEXT( "/Engine/VREditor/LaserPointer/LaserPointerMaterialInst" ) );
			check( UserScaleIndicatorMaterial != nullptr );

			UMaterialInstance* TranslucentUserScaleIndicatorMaterial = LoadObject<UMaterialInstance>( nullptr, TEXT( "/Engine/VREditor/LaserPointer/TranslucentLaserPointerMaterialInst" ) );
			check( TranslucentUserScaleIndicatorMaterial != nullptr );

			FixedUserScaleMID = UMaterialInstanceDynamic::Create( UserScaleIndicatorMaterial, GetTransientPackage() );
			check( FixedUserScaleMID != nullptr );

			TranslucentFixedUserScaleMID = UMaterialInstanceDynamic::Create( TranslucentUserScaleIndicatorMaterial, GetTransientPackage() );
			check( TranslucentFixedUserScaleMID != nullptr );
			
			CurrentUserScaleMID = UMaterialInstanceDynamic::Create( UserScaleIndicatorMaterial, GetTransientPackage() );
			check( CurrentUserScaleMID != nullptr );

			TranslucentCurrentUserScaleMID = UMaterialInstanceDynamic::Create( TranslucentUserScaleIndicatorMaterial, GetTransientPackage() );
			check( TranslucentCurrentUserScaleMID != nullptr );

			UStaticMesh* ScaleLineMesh = LoadObject<UStaticMesh>( nullptr, TEXT( "/Engine/VREditor/LaserPointer/LaserPointerMesh" ) );
			check( ScaleLineMesh != nullptr );

			// Creating the background bar progress of the scale 
			{
				ScaleProgressMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "ScaleProgressMeshComponent" ) );
				this->AddOwnedComponent( ScaleProgressMeshComponent );
				ScaleProgressMeshComponent->SetupAttachment( RootComponent );

				ScaleProgressMeshComponent->SetStaticMesh( ScaleLineMesh );
				ScaleProgressMeshComponent->SetMobility( EComponentMobility::Movable );
				ScaleProgressMeshComponent->SetCollisionEnabled( ECollisionEnabled::NoCollision );
				ScaleProgressMeshComponent->SetMaterial( 0, FixedUserScaleMID );
				ScaleProgressMeshComponent->SetMaterial( 1, TranslucentFixedUserScaleMID );

				// The user scale indicator starts invisible
				ScaleProgressMeshComponent->SetVisibility( false );
			}

			// Creating the current progress of the scale 
			{
				CurrentScaleProgressMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>( TEXT( "CurrentScaleProgressMeshComponent" ) );
				AddOwnedComponent( CurrentScaleProgressMeshComponent );
				CurrentScaleProgressMeshComponent->SetupAttachment( RootComponent );

				CurrentScaleProgressMeshComponent->SetStaticMesh( ScaleLineMesh );
				CurrentScaleProgressMeshComponent->SetMobility( EComponentMobility::Movable );
				CurrentScaleProgressMeshComponent->SetCollisionEnabled( ECollisionEnabled::NoCollision );
				CurrentScaleProgressMeshComponent->SetMaterial( 0, CurrentUserScaleMID );
				CurrentScaleProgressMeshComponent->SetMaterial( 1, TranslucentCurrentUserScaleMID );

				// The user scale indicator starts invisible
				CurrentScaleProgressMeshComponent->SetVisibility( false );
			}
		}

		// Creating the text for scaling
		{
			UFont* TextFont = LoadObject<UFont>( nullptr, TEXT( "/Engine/VREditor/Fonts/VRText_RobotoLarge" ) );
			check( TextFont != nullptr );

			UMaterialInterface* UserScaleIndicatorMaterial = LoadObject<UMaterialInterface>( nullptr, TEXT( "/Engine/VREditor/Fonts/VRTextMaterial" ) );
			check( UserScaleIndicatorMaterial != nullptr );

			UserScaleIndicatorText = CreateDefaultSubobject<UTextRenderComponent>( TEXT( "UserScaleIndicatorText" ) );
			AddOwnedComponent( UserScaleIndicatorText );
			UserScaleIndicatorText->SetupAttachment( RootComponent );

			UserScaleIndicatorText->SetMobility( EComponentMobility::Movable );
			UserScaleIndicatorText->SetCollisionEnabled( ECollisionEnabled::NoCollision );
			UserScaleIndicatorText->SetCollisionProfileName( UCollisionProfile::NoCollision_ProfileName );

			UserScaleIndicatorText->bGenerateOverlapEvents = false;
			UserScaleIndicatorText->SetCanEverAffectNavigation( false );
			UserScaleIndicatorText->bCastDynamicShadow = false;
			UserScaleIndicatorText->bCastStaticShadow = false;
			UserScaleIndicatorText->bAffectDistanceFieldLighting = false;
			UserScaleIndicatorText->bAffectDynamicIndirectLighting = false;

			// Use a custom font.  The text will be visible up close.
			UserScaleIndicatorText->SetFont( TextFont );
			UserScaleIndicatorText->SetWorldSize( 8.0f );
			UserScaleIndicatorText->SetTextMaterial( UserScaleIndicatorMaterial );

			// Center the text horizontally
			UserScaleIndicatorText->SetHorizontalAlignment( EHTA_Center );
			UserScaleIndicatorText->SetVisibility( false );
		}
	}

	// Load our post process material for the world movement grid
	{
		UMaterial* Material = LoadObject<UMaterial>( nullptr, TEXT( "/Engine/VREditor/WorldMovementGrid/GridPostProcess" ) );
		check( Material != nullptr );
		WorldMovementPostProcessMaterial = UMaterialInstanceDynamic::Create( Material, GetTransientPackage() );
		check( WorldMovementPostProcessMaterial != nullptr );
	}

	// Post processing
	{
		PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>( TEXT( "PostProcessComponent" ) );
		AddOwnedComponent( PostProcessComponent );
		PostProcessComponent->SetupAttachment( this->GetRootComponent( ) );

		// Unlimited size
		PostProcessComponent->bEnabled = VREd::ShowWorldMovementPostProcess->GetInt( ) == 1 ? true : false;
		PostProcessComponent->bUnbound = true;
	}

}

AVREditorAvatarActor::~AVREditorAvatarActor()
{
	WorldMovementGridMID = nullptr;
	WorldMovementPostProcessMaterial = nullptr;
	HeadMeshComponent = nullptr;
	WorldMovementGridMeshComponent = nullptr;
	PostProcessComponent = nullptr;
	ScaleProgressMeshComponent = nullptr;
	CurrentScaleProgressMeshComponent = nullptr;
	UserScaleIndicatorText = nullptr;
}

void AVREditorAvatarActor::Init( FVREditorMode* InVRMode  )
{
	VRMode = InVRMode;

	// Set the default color for the progress bar
	{
		static FName StaticLaserColorName( "LaserColor" );
		const FLinearColor FixedProgressbarColor = VRMode->GetColor( FVREditorMode::EColors::WorldDraggingColor_TwoHands );
		FixedUserScaleMID->SetVectorParameterValue( StaticLaserColorName, FixedProgressbarColor );
		TranslucentFixedUserScaleMID->SetVectorParameterValue( StaticLaserColorName, FixedProgressbarColor );

		const FLinearColor CurrentProgressbarColor = VRMode->GetColor( FVREditorMode::EColors::GreenGizmoColor );
		CurrentUserScaleMID->SetVectorParameterValue( StaticLaserColorName, CurrentProgressbarColor );
		TranslucentCurrentUserScaleMID->SetVectorParameterValue( StaticLaserColorName, CurrentProgressbarColor );
	}

	UserScaleIndicatorText->SetTextRenderColor( VRMode->GetColor( FVREditorMode::EColors::WorldDraggingColor_TwoHands ).ToFColor( false ) );

	// Tell the grid to stay relative to the rootcomponent
	const FAttachmentTransformRules AttachmentTransformRules = FAttachmentTransformRules( EAttachmentRule::KeepRelative, true );
	WorldMovementGridMeshComponent->AttachToComponent( RootComponent, AttachmentTransformRules );
}

void AVREditorAvatarActor::TickManually( const float DeltaTime )
{
	SetActorTransform( VRMode->GetRoomTransform() );
	const float WorldScaleFactor = VRMode->GetWorldScaleFactor();
	const FVector WorldScaleFactorVector = FVector( WorldScaleFactor );

	// Our head will lock to the HMD position
	{
		FTransform RoomSpaceTransformWithWorldToMetersScaling = VRMode->GetRoomSpaceHeadTransform( );
		RoomSpaceTransformWithWorldToMetersScaling.SetScale3D( RoomSpaceTransformWithWorldToMetersScaling.GetScale3D( ) * WorldScaleFactorVector );

		// @todo vreditor urgent: Head disabled until we can fix late frame update issue
		if ( false && VRMode->IsActuallyUsingVR() && GEngine->HMDDevice.IsValid( ) && GEngine->HMDDevice->IsHeadTrackingAllowed( ) )
		{
			HeadMeshComponent->SetVisibility( true );

			// Apply late frame update to the head mesh
			HeadMeshComponent->ResetRelativeTransform( );
			const FTransform ParentToWorld = HeadMeshComponent->GetComponentToWorld( );
			GEngine->HMDDevice->SetupLateUpdate( ParentToWorld, HeadMeshComponent );
			HeadMeshComponent->SetRelativeTransform( RoomSpaceTransformWithWorldToMetersScaling );
		}
		else
		{
			HeadMeshComponent->SetVisibility( false );
		}
	}

	// Scale the grid so that it stays the same size in the tracking space
	WorldMovementGridMeshComponent->SetRelativeScale3D( WorldScaleFactorVector * VREd::GridScaleMultiplier->GetFloat() );
	WorldMovementGridMeshComponent->SetRelativeLocation( WorldScaleFactorVector * FVector( 0.0f, 0.0f, VREd::GridHeightOffset->GetFloat() ) );

	// Update the user scale indicator
	{
		UVREditorInteractor* LeftHandInteractor = VRMode->GetHandInteractor( EControllerHand::Left );
		UVREditorInteractor* RightHandInteractor = VRMode->GetHandInteractor( EControllerHand::Right );
		if ( ( LeftHandInteractor != nullptr && RightHandInteractor != nullptr ) &&
			 ( ( LeftHandInteractor->GetDraggingMode( ) == EViewportInteractionDraggingMode::World && RightHandInteractor->GetDraggingMode( ) == EViewportInteractionDraggingMode::AssistingDrag ) ||
			 ( LeftHandInteractor->GetDraggingMode( ) == EViewportInteractionDraggingMode::AssistingDrag && RightHandInteractor->GetDraggingMode( ) == EViewportInteractionDraggingMode::World ) ) )
		{
			// Setting all components to be visible
			CurrentScaleProgressMeshComponent->SetVisibility( true );
			ScaleProgressMeshComponent->SetVisibility( true );
			UserScaleIndicatorText->SetVisibility( true );

			// Here we calculate the distance, direction and center of the two hands
			const FVector ScaleIndicatorStartPosition = LeftHandInteractor->GetTransform( ).GetLocation( );
			const FVector ScaleIndicatorEndPosition = RightHandInteractor->GetTransform( ).GetLocation( );
			const FVector ScaleIndicatorDirection = ( ScaleIndicatorEndPosition - ScaleIndicatorStartPosition ).GetSafeNormal( );

			const float ScaleIndicatorLength = FMath::Max( 0.000001f, ( ScaleIndicatorEndPosition - ScaleIndicatorStartPosition ).Size( ) );
			const float Radius = ( VREd::ScaleProgressBarRadius->GetFloat( ) * WorldScaleFactor );
			const float ProgressbarLength = VREd::ScaleProgressBarLength->GetFloat( );
			const float Scale = WorldScaleFactor;

			// Add an offset to the center, we don't want the hands to clip through the hand meshes
			FVector MiddleLocation = ScaleIndicatorEndPosition - ( ScaleIndicatorLength * 0.5f ) * ScaleIndicatorDirection;
			MiddleLocation.Z += Scale * 5.f;

			// Setting the transform for the scale progressbar
			{
				const FVector ProgressbarStart = MiddleLocation - ( ( ProgressbarLength * 0.5f ) * Scale ) * ScaleIndicatorDirection;

				ScaleProgressMeshComponent->SetWorldTransform( FTransform( ScaleIndicatorDirection.ToOrientationRotator( ),
					ProgressbarStart,
					FVector( ProgressbarLength * Scale, Radius, Radius )
					) );
			}

			// Setting the transform for the current scale progressbar from the center
			{
				const float CurrentProgressScale = ( Scale * Scale ) * ( ProgressbarLength / ( VRMode->GetWorldInteraction().GetMaxScale( ) / 100 ) );
				const FVector CurrentProgressStart = MiddleLocation - ( CurrentProgressScale * 0.5f ) * ScaleIndicatorDirection;

				CurrentScaleProgressMeshComponent->SetWorldTransform( FTransform( ScaleIndicatorDirection.ToOrientationRotator( ),
					CurrentProgressStart,
					FVector( CurrentProgressScale, Radius * 2, Radius * 2 )
					) );
			}

			//Setting the transform for the scale text
			{
				MiddleLocation.Z += Scale * 5.0f;
				UserScaleIndicatorText->SetWorldTransform( FTransform( ( VRMode->GetHeadTransform().GetLocation( ) - MiddleLocation ).ToOrientationRotator( ),
					MiddleLocation,
					VRMode->GetRoomSpaceHeadTransform().GetScale3D()  * Scale
					) );

				FNumberFormattingOptions NumberFormat;
				NumberFormat.MinimumIntegralDigits = 1;
				NumberFormat.MaximumIntegralDigits = 10000;
				NumberFormat.MinimumFractionalDigits = 1;
				NumberFormat.MaximumFractionalDigits = 1;
				UserScaleIndicatorText->SetText( FText::AsNumber( Scale, &NumberFormat ) );
			}
		}
		else
		{
			//Setting all components invisible
			CurrentScaleProgressMeshComponent->SetVisibility( false );
			ScaleProgressMeshComponent->SetVisibility( false );
			UserScaleIndicatorText->SetVisibility( false );
		}
	}

	// Updating the opacity and visibility of the grid according to the controllers //@todo
	{
		if ( VREd::ShowMovementGrid->GetInt( ) == 1 )
		{
			UVREditorInteractor* LeftHandInteractor = VRMode->GetHandInteractor( EControllerHand::Left );
			UVREditorInteractor* RightHandInteractor = VRMode->GetHandInteractor( EControllerHand::Right );

			if ( LeftHandInteractor != nullptr && RightHandInteractor )
			{
				// Show the grid full opacity when dragging or scaling
				float GoalOpacity = 0.f;
				if ( LeftHandInteractor->GetDraggingMode() == EViewportInteractionDraggingMode::World || RightHandInteractor->GetDraggingMode() == EViewportInteractionDraggingMode::World )
				{
					GoalOpacity = 1.0f;
				}
				else if ( ( LeftHandInteractor->GetLastDraggingMode() == EViewportInteractionDraggingMode::World && !LeftHandInteractor->GetDragTranslationVelocity().IsNearlyZero( VREd::GridMovementTolerance->GetFloat() ) ) )
				{
					GoalOpacity = FMath::Clamp( LeftHandInteractor->GetDragTranslationVelocity().Size() / VREd::GridFadeStartVelocity->GetFloat(), 0.0f, 1.0f );
				}
				else if ( ( RightHandInteractor->GetLastDraggingMode() == EViewportInteractionDraggingMode::World && !RightHandInteractor->GetDragTranslationVelocity().IsNearlyZero( VREd::GridMovementTolerance->GetFloat() ) ) )
				{
					GoalOpacity = FMath::Clamp( RightHandInteractor->GetDragTranslationVelocity().Size() / VREd::GridFadeStartVelocity->GetFloat(), 0.0f, 1.0f );
				}

				// Check the current opacity and add or subtract to reach the goal
				float NewOpacity = WorldMovementGridOpacity;
				if ( NewOpacity < GoalOpacity )
				{
					NewOpacity = FMath::Min( GoalOpacity, NewOpacity + DeltaTime * VREd::GridFadeMultiplier->GetFloat() );
				}
				else if ( NewOpacity > GoalOpacity )
				{
					NewOpacity = FMath::Max( GoalOpacity, NewOpacity - DeltaTime * VREd::GridFadeMultiplier->GetFloat() );
				}

				// Only update when the opacity is different
				if ( !FMath::IsNearlyEqual( WorldMovementGridOpacity, GoalOpacity ) )
				{
					WorldMovementGridOpacity = NewOpacity;

					// See if the opacity is almost zero and if the goal opacity is lower than the new opacity set it to zero if so. Otherwise it will flicker
					if ( WorldMovementGridOpacity < SMALL_NUMBER )
					{
						WorldMovementGridOpacity = 0.f;
						WorldMovementGridMeshComponent->SetVisibility( false );
					}
					else
					{
						WorldMovementGridMeshComponent->SetVisibility( true );
					}

					static const FName OpacityName( "OpacityParam" );
					WorldMovementGridMID->SetScalarParameterValue( OpacityName, WorldMovementGridOpacity * VREd::GridMaxOpacity->GetFloat() );
				}
			}
		}
		else
		{
			WorldMovementGridMeshComponent->SetVisibility( false );
		}
	}

	// Apply a post process effect while the user is moving the world around.  The effect "greys out" any pixels
	// that are not nearby, and completely hides the skybox and other very distant objects.  This is to help
	// prevent simulation sickness when moving/rotating/scaling the entire world around them.
	{
		PostProcessComponent->bEnabled = VREd::ShowWorldMovementPostProcess->GetInt( ) == 1 ? true : false;

		// Make sure our world size is reflected in the post process material
		static FName WorldScaleFactorParameterName( "WorldScaleFactor" );
		WorldMovementPostProcessMaterial->SetScalarParameterValue( WorldScaleFactorParameterName, WorldScaleFactor );

		static FName RoomOriginParameterName( "RoomOrigin" );
		WorldMovementPostProcessMaterial->SetVectorParameterValue( RoomOriginParameterName, VRMode->GetRoomTransform( ).GetLocation( ) );

		static FName FogColorParameterName( "FogColor" );
		// WorldMovementPostProcessMaterial->SetVectorParameterValue( FogColorParameterName, FLinearColor::Black );

		static FName StartDistanceParameterName( "StartDistance" );
		WorldMovementPostProcessMaterial->SetScalarParameterValue( StartDistanceParameterName, VREd::WorldMovementFogStartDistance->GetFloat( ) );

		static FName EndDistanceParameterName( "EndDistance" );
		WorldMovementPostProcessMaterial->SetScalarParameterValue( EndDistanceParameterName, VREd::WorldMovementFogEndDistance->GetFloat( ) );

		static FName FogOpacityParameterName( "FogOpacity" );
		WorldMovementPostProcessMaterial->SetScalarParameterValue( FogOpacityParameterName, VREd::WorldMovementFogOpacity->GetFloat( ) );

		static FName SkyboxDistanceParameterName( "SkyboxDistance" );
		WorldMovementPostProcessMaterial->SetScalarParameterValue( SkyboxDistanceParameterName, VREd::WorldMovementFogSkyboxDistance->GetFloat( ) );

		const bool bShouldDrawWorldMovementPostProcess = WorldMovementGridOpacity > KINDA_SMALL_NUMBER;
		if ( bShouldDrawWorldMovementPostProcess != bIsDrawingWorldMovementPostProcess )
		{
			UPostProcessComponent* PostProcess = PostProcessComponent;
			if ( bShouldDrawWorldMovementPostProcess )
			{
				PostProcess->AddOrUpdateBlendable( WorldMovementPostProcessMaterial );
				bIsDrawingWorldMovementPostProcess = true;
			}
			else
			{
				bIsDrawingWorldMovementPostProcess = false;
				PostProcess->Settings.RemoveBlendable( WorldMovementPostProcessMaterial );
			}
		}

		if ( bIsDrawingWorldMovementPostProcess )
		{
			static FName OpacityParameterName( "Opacity" );
			WorldMovementPostProcessMaterial->SetScalarParameterValue( OpacityParameterName, FMath::Clamp( WorldMovementGridOpacity, 0.0f, 1.0f ) );
		}
	}
}
