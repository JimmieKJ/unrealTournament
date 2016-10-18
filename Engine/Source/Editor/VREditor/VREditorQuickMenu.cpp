// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorQuickMenu.h"
#include "VREditorUISystem.h"
#include "VREditorMode.h"
#include "VREditorWorldInteraction.h"
#include "VREditorFloatingUI.h"
#include "VREditorTransformGizmo.h"
#include "SLevelViewport.h"	// For Simulate toggle
#include "ImageUtils.h"


#define LOCTEXT_NAMESPACE "VREditorQuickMenu"

UVREditorQuickMenu::UVREditorQuickMenu( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}


void UVREditorQuickMenu::OnExitVRButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().StartExitingVRMode();
}


void UVREditorQuickMenu::OnDeleteButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().DeleteSelectedObjects();
}


void UVREditorQuickMenu::OnUndoButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Undo();
}


void UVREditorQuickMenu::OnRedoButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Redo();
}


void UVREditorQuickMenu::OnCopyButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Copy();
}


void UVREditorQuickMenu::OnPasteButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Paste();
}


bool UVREditorQuickMenu::OnTranslationSnapButtonClicked( const bool bIsChecked )
{
	GUnrealEd->Exec( GetWorld(), *FString::Printf( TEXT( "MODE GRID=%d" ), bIsChecked ? 1 : 0 ) );

	return IsTranslationSnapEnabled();
}


bool UVREditorQuickMenu::IsTranslationSnapEnabled() const
{
	return GetDefault<ULevelEditorViewportSettings>()->GridEnabled;
}


FText UVREditorQuickMenu::OnTranslationSnapSizeButtonClicked()
{
	const TArray<float>& GridSizes = GEditor->GetCurrentPositionGridArray();
	const int32 CurrentGridSize = GetDefault<ULevelEditorViewportSettings>()->CurrentPosGridSize;

	int32 NewGridSize = CurrentGridSize + 1;
	if( NewGridSize >= GridSizes.Num() )
	{
		NewGridSize = 0;
	}

	GEditor->SetGridSize( NewGridSize );

	return GetTranslationSnapSizeText();
}


FText UVREditorQuickMenu::GetTranslationSnapSizeText() const
{
	return FText::AsNumber( GEditor->GetGridSize() );
}


FText UVREditorQuickMenu::OnRotationSnapSizeButtonClicked()
{
	const TArray<float>& GridSizes = GEditor->GetCurrentRotationGridArray();
	const int32 CurrentGridSize = GetDefault<ULevelEditorViewportSettings>()->CurrentRotGridSize;
	const ERotationGridMode CurrentGridMode = GetDefault<ULevelEditorViewportSettings>()->CurrentRotGridMode;

	int32 NewGridSize = CurrentGridSize + 1;
	if( NewGridSize >= GridSizes.Num() )
	{
		NewGridSize = 0;
	}

	GEditor->SetRotGridSize( NewGridSize, CurrentGridMode );

	return GetRotationSnapSizeText();
}


FText UVREditorQuickMenu::GetRotationSnapSizeText() const
{
	return FText::AsNumber( GEditor->GetRotGridSize().Yaw );
}


FText UVREditorQuickMenu::OnScaleSnapSizeButtonClicked()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>();
	const int32 NumGridSizes = ViewportSettings->ScalingGridSizes.Num();

	const int32 CurrentGridSize = GetDefault<ULevelEditorViewportSettings>()->CurrentScalingGridSize;

	int32 NewGridSize = CurrentGridSize + 1;
	if( NewGridSize >= NumGridSizes )
	{
		NewGridSize = 0;
	}

	GEditor->SetScaleGridSize( NewGridSize );

	return GetScaleSnapSizeText();
}


FText UVREditorQuickMenu::GetScaleSnapSizeText() const
{
	return FText::AsNumber( GEditor->GetScaleGridSize() );
}


bool UVREditorQuickMenu::OnRotationSnapButtonClicked( const bool bIsChecked )
{
	GUnrealEd->Exec( GetWorld(), *FString::Printf( TEXT( "MODE ROTGRID=%d" ), bIsChecked ? 1 : 0 ) );

	return IsRotationSnapEnabled();
}


bool UVREditorQuickMenu::IsRotationSnapEnabled() const
{
	return GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled;
}


bool UVREditorQuickMenu::OnScaleSnapButtonClicked( const bool bIsChecked )
{
	GUnrealEd->Exec( GetWorld(), *FString::Printf( TEXT( "MODE SCALEGRID=%d" ), bIsChecked ? 1 : 0 ) );

	return IsScaleSnapEnabled();
}


bool UVREditorQuickMenu::IsScaleSnapEnabled() const
{
	return GetDefault<ULevelEditorViewportSettings>()->SnapScaleEnabled;
}


FText UVREditorQuickMenu::OnGizmoCoordinateSystemButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().CycleTransformGizmoCoordinateSpace(); 
	return GetGizmoCoordinateSystemText();
}


FText UVREditorQuickMenu::GetGizmoCoordinateSystemText() const
{
	const ECoordSystem CurrentCoordSystem = GetOwner()->GetOwner().GetOwner().GetWorldInteraction().GetTransformGizmoCoordinateSpace(); //@todo VREditor
	return CurrentCoordSystem == COORD_World ? LOCTEXT( "WorldCoordinateSystem", "World Space" ) : LOCTEXT( "LocalCoordinateSystem", "Local Space" );
}


FText UVREditorQuickMenu::OnGizmoModeButtonClicked()
{
	const EGizmoHandleTypes CurrentGizmoMode = GetOwner()->GetOwner().GetOwner().GetWorldInteraction().GetCurrentGizmoType();
	GetOwner()->GetOwner().GetOwner().CycleTransformGizmoHandleType();
	return GetGizmoModeText();
}


FText UVREditorQuickMenu::GetGizmoModeText() const
{
	const EGizmoHandleTypes CurrentGizmoType = GetOwner()->GetOwner().GetOwner().GetWorldInteraction().GetCurrentGizmoType();
	FText GizmoTypeText;
	switch( CurrentGizmoType )
	{
		case EGizmoHandleTypes::All:
			GizmoTypeText = LOCTEXT( "AllGizmoType", "Universal" );
			break;

		case EGizmoHandleTypes::Translate:
			GizmoTypeText = LOCTEXT( "TranslateGizmoType", "Translate" );
			break;

		case EGizmoHandleTypes::Rotate:
			GizmoTypeText = LOCTEXT( "RotationGizmoType", "Rotation" );
			break;

		case EGizmoHandleTypes::Scale:
			GizmoTypeText = LOCTEXT( "ScaleGizmoType", "Scale" );
			break;

		default:
			check( 0 );	// Unrecognized type
			break;
	}

	return GizmoTypeText;
}


bool UVREditorQuickMenu::OnSimulateButtonClicked( const bool bIsChecked )
{
	if( bIsChecked != GEditor->bIsSimulatingInEditor )
	{
		if( GEditor->PlayWorld == nullptr && !GEditor->bIsSimulatingInEditor )
		{
			const bool bSimulateInEditor = false;
			const bool bAtPlayerStart = true;

			// If there is an active level view port, play the game in it.
			TSharedRef<ILevelViewport> LevelViewport = StaticCastSharedRef<SLevelViewport>( GetOwner()->GetOwner().GetOwner().GetLevelViewportPossessedForVR().AsShared() );
			GUnrealEd->RequestPlaySession(
				bAtPlayerStart,
				LevelViewport,
				bSimulateInEditor );
		}
		else if( GEditor->bIsSimulatingInEditor )
		{
			GEditor->RequestEndPlayMap();
		}
		else if( GUnrealEd->PlayWorld != nullptr )
		{
			// There is already a play world active which means simulate in editor is happening
			// Toggle to pie 
			GUnrealEd->RequestToggleBetweenPIEandSIE();
		}
	}

	return IsSimulatingEnabled();
}


bool UVREditorQuickMenu::IsSimulatingEnabled() const
{
	return GEditor->bIsSimulateInEditorQueued || GEditor->bIsSimulatingInEditor;
}


bool UVREditorQuickMenu::OnContentBrowserButtonClicked( const bool bIsChecked )
{
	GetOwner()->GetOwner().TogglePanelVisibility( UVREditorUISystem::EEditorUIPanel::ContentBrowser );
	return IsContentBrowserVisible();
}


bool UVREditorQuickMenu::IsContentBrowserVisible() const
{
	return ( GetOwner()->GetOwner().IsShowingEditorUIPanel( UVREditorUISystem::EEditorUIPanel::ContentBrowser ) );
}


bool UVREditorQuickMenu::OnWorldOutlinerButtonClicked( const bool bIsChecked )
{
	GetOwner()->GetOwner().TogglePanelVisibility( UVREditorUISystem::EEditorUIPanel::WorldOutliner );
	return IsWorldOutlinerVisible();
}


bool UVREditorQuickMenu::IsWorldOutlinerVisible() const
{
	return ( GetOwner()->GetOwner().IsShowingEditorUIPanel( UVREditorUISystem::EEditorUIPanel::WorldOutliner ) );
}


bool UVREditorQuickMenu::OnActorDetailsButtonClicked( const bool bIsChecked )
{
	GetOwner()->GetOwner().TogglePanelVisibility( UVREditorUISystem::EEditorUIPanel::ActorDetails );
	return IsActorDetailsVisible();
}


bool UVREditorQuickMenu::IsActorDetailsVisible() const
{
	return ( GetOwner()->GetOwner().IsShowingEditorUIPanel( UVREditorUISystem::EEditorUIPanel::ActorDetails ) );
}


bool UVREditorQuickMenu::OnModesButtonClicked( const bool bIsChecked )
{
	GetOwner()->GetOwner().TogglePanelVisibility( UVREditorUISystem::EEditorUIPanel::Modes );
	return IsModesVisible();
}


bool UVREditorQuickMenu::IsModesVisible() const
{
	return ( GetOwner()->GetOwner().IsShowingEditorUIPanel( UVREditorUISystem::EEditorUIPanel::Modes ) );
}


bool UVREditorQuickMenu::OnGameModeButtonClicked( const bool bIsChecked )
{
	FEditorViewportClient* ViewportClient = GetOwner()->GetOwner().GetOwner().GetLevelViewportPossessedForVR().GetViewportClient().Get();
	const bool bIsInGameView = ViewportClient->IsInGameView();
	if( bIsChecked != bIsInGameView )
	{
		ViewportClient->SetGameView( bIsChecked );
	}

	return IsGameModeEnabled();
}


bool UVREditorQuickMenu::IsGameModeEnabled() const
{
	const FEditorViewportClient* ViewportClient = GetOwner()->GetOwner().GetOwner().GetLevelViewportPossessedForVR().GetViewportClient().Get();
	return ViewportClient->IsInGameView();
}


bool UVREditorQuickMenu::OnTutorialButtonClicked( const bool bIsChecked )
{
	GetOwner()->GetOwner().TogglePanelVisibility( UVREditorUISystem::EEditorUIPanel::Tutorial );
	return IsTutorialVisible();
}

bool UVREditorQuickMenu::IsTutorialVisible() const
{
	return ( GetOwner()->GetOwner().IsShowingEditorUIPanel( UVREditorUISystem::EEditorUIPanel::Tutorial ) );
}

bool UVREditorQuickMenu::OnLightButtonClicked( const bool bIsChecked ) 
{
	EControllerHand ControllerHand = GetOwner()->GetDockedTo() == AVREditorFloatingUI::EDockedTo::LeftArm ? EControllerHand::Right : EControllerHand::Left;
	UVREditorInteractor* Interactor = GetOwner()->GetOwner().GetOwner().GetHandInteractor( ControllerHand );
	GetOwner()->GetOwner().GetOwner().ToggleFlashlight( Interactor );
	return IsLightVisible();
}

bool UVREditorQuickMenu::IsLightVisible() const
{
	return (GetOwner()->GetOwner().GetOwner().IsFlashlightOn());
}

void UVREditorQuickMenu::OnScreenshotButtonClicked()
{
	// @todo vreditor: update after direct buffer grab changes
	
	FString GeneratedFilename = TEXT("");
	FString Filename = TEXT("");
	FScreenshotRequest::CreateViewportScreenShotFilename(GeneratedFilename);
	const bool bRemovePath = false;
	GeneratedFilename = FPaths::GetBaseFilename(GeneratedFilename, bRemovePath);
	FFileHelper::GenerateNextBitmapFilename(GeneratedFilename, TEXT("png"), Filename);

	FViewport* Viewport = GetOwner()->GetOwner().GetOwner().GetLevelViewportPossessedForVR().GetActiveViewport();
	TSharedRef<SWidget> WindowRef = GetOwner()->GetOwner().GetOwner().GetLevelViewportPossessedForVR().AsWidget();


	TArray<FColor> OutImageData;
	FIntVector OutImageSize;

	if (FSlateApplication::Get().TakeScreenshot(WindowRef, OutImageData, OutImageSize))
	{
		int32 Width = OutImageSize.X;
		int32 Height = OutImageSize.Y;

		TArray<FColor> ScaledBitmap;


		ScaledBitmap = OutImageData;
		//Clear the alpha channel before saving
		for (int32 Index = 0; Index < Width*Height; Index++)
		{
			ScaledBitmap[Index].A = 255;
		}


		TArray<uint8> CompressedBitmap;
		FImageUtils::CompressImageArray(Width, Height, ScaledBitmap, CompressedBitmap);


		//Save locally
		const bool bTree = true;
		const FString FinalFileName = Filename;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FinalFileName), bTree);
		FFileHelper::SaveArrayToFile(CompressedBitmap, *FinalFileName);

	}


}

void UVREditorQuickMenu::OnPlayButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().StartExitingVRMode( EVREditorExitType::PIE_VR );
}

bool UVREditorQuickMenu::OnAssetEditorButtonClicked( const bool bIsChecked )
{
	GetOwner()->GetOwner().TogglePanelVisibility( UVREditorUISystem::EEditorUIPanel::AssetEditor );
	return IsAssetEditorVisible();
}

bool UVREditorQuickMenu::IsAssetEditorVisible() const
{
	return ( GetOwner()->GetOwner().IsShowingEditorUIPanel( UVREditorUISystem::EEditorUIPanel::AssetEditor ) );
}

bool UVREditorQuickMenu::OnWorldSettingsButtonClicked( const bool bIsChecked )
{
	GetOwner()->GetOwner().TogglePanelVisibility( UVREditorUISystem::EEditorUIPanel::WorldSettings);
	return IsWorldSettingsVisible();
}

bool UVREditorQuickMenu::IsWorldSettingsVisible() const
{
	return ( GetOwner()->GetOwner().IsShowingEditorUIPanel( UVREditorUISystem::EEditorUIPanel::WorldSettings ) );
}

#undef LOCTEXT_NAMESPACE