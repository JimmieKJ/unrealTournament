// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "VREditorBaseUserWidget.h"
#include "VREditorQuickMenu.generated.h"


/**
 * A simple quick-access menu for VR editing, with frequently-used features
 */
UCLASS( Blueprintable )
class UVREditorQuickMenu : public UVREditorBaseUserWidget
{
	GENERATED_BODY()

public:
	
	/** Default constructor that sets up CDO properties */
	UVREditorQuickMenu( const FObjectInitializer& ObjectInitializer );

	/** 
	 * Event handlers.  These are called from the Blueprint when buttons are clicked. 
	 */

	UFUNCTION( BlueprintCallable, Category="Default" )
	void OnExitVRButtonClicked();

	UFUNCTION( BlueprintCallable, Category="Default" )
	void OnDeleteButtonClicked();

	UFUNCTION( BlueprintCallable, Category = "Default" )
	void OnUndoButtonClicked();

	UFUNCTION( BlueprintCallable, Category = "Default" )
	void OnRedoButtonClicked();

	UFUNCTION( BlueprintCallable, Category = "Default" )
	void OnCopyButtonClicked();

	UFUNCTION( BlueprintCallable, Category = "Default" )
	void OnPasteButtonClicked();

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnTranslationSnapButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsTranslationSnapEnabled() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	FText OnTranslationSnapSizeButtonClicked();

	UFUNCTION( BlueprintPure, Category = "Default" )
	FText GetTranslationSnapSizeText() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnRotationSnapButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsRotationSnapEnabled() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	FText OnRotationSnapSizeButtonClicked();

	UFUNCTION( BlueprintPure, Category = "Default" )
	FText GetRotationSnapSizeText() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnScaleSnapButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsScaleSnapEnabled() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	FText OnScaleSnapSizeButtonClicked();

	UFUNCTION( BlueprintPure, Category = "Default" )
	FText GetScaleSnapSizeText() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	FText OnGizmoCoordinateSystemButtonClicked();

	UFUNCTION( BlueprintPure, Category = "Default" )
	FText GetGizmoCoordinateSystemText() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	FText OnGizmoModeButtonClicked();

	UFUNCTION( BlueprintPure, Category = "Default" )
	FText GetGizmoModeText() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnSimulateButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsSimulatingEnabled() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnContentBrowserButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsContentBrowserVisible() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnWorldOutlinerButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsWorldOutlinerVisible() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnActorDetailsButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsActorDetailsVisible() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnModesButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsModesVisible() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnGameModeButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsGameModeEnabled() const;

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnTutorialButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsTutorialVisible() const;

	UFUNCTION(BlueprintCallable, Category = "Default")
	bool OnLightButtonClicked(const bool bIsChecked);

	UFUNCTION(BlueprintPure, Category = "Default")
	bool IsLightVisible() const;

	UFUNCTION(BlueprintCallable, Category = "Default")
	void OnScreenshotButtonClicked();

	UFUNCTION( BlueprintCallable, Category = "Default" )
	void OnPlayButtonClicked();

	UFUNCTION( BlueprintCallable, Category = "Default" )
	bool OnAssetEditorButtonClicked( const bool bIsChecked );

	UFUNCTION( BlueprintPure, Category = "Default" )
	bool IsAssetEditorVisible() const;

	UFUNCTION(BlueprintCallable, Category = "Default")
	bool OnWorldSettingsButtonClicked(const bool bIsChecked);

	UFUNCTION(BlueprintPure, Category = "Default")
	bool IsWorldSettingsVisible() const;

	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Refresh"))
	void RefreshUI();

};