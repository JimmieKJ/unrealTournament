// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_DELEGATE_OneParam( FOnSceneDepthLocationSelected, FVector );

/**
 * Scene depth picker mode module
 */
class SCENEDEPTHPICKERMODE_API FSceneDepthPickerModeModule : public IModuleInterface
{
public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End of IModuleInterface

	/** 
	 * Enter scene depth picking mode (note: will cancel any current scene depth picking)
	 * @param	InOnDepthSelected		Delegate to call when user has selected a location to sample the scene depth
	 */
	void BeginSceneDepthPickingMode(FOnSceneDepthLocationSelected InOnSceneDepthLocationSelected);

	/** Exit scene depth picking mode */
	void EndSceneDepthPickingMode();

	/** @return Whether or not scene depth picking mode is currently active */
	bool IsInSceneDepthPickingMode() const;
};
