// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __AnimViewportMenuCommands_h_
#define __AnimViewportMenuCommands_h_

/**
 * Class containing commands for viewport menu actions
 */
class FAnimViewportMenuCommands : public TCommands<FAnimViewportMenuCommands>
{
public:
	FAnimViewportMenuCommands() 
		: TCommands<FAnimViewportMenuCommands>
		(
			TEXT("AnimViewportMenu"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "AnimViewportMenu", "Animation Viewport Menu"), // Localized context name for displaying
			NAME_None, // Parent context name.  
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{
	}

	/** Preview what is active on the current window */
	TSharedPtr< FUICommandInfo > Auto;

	/** Lock to current preview */
	TSharedPtr< FUICommandInfo > Lock;

	/** Select camera follow */
	TSharedPtr< FUICommandInfo > CameraFollow;

	/** Use in-game Bound of preview mesh */
	TSharedPtr< FUICommandInfo > UseInGameBound;

	/** Show vertex normals */
	TSharedPtr< FUICommandInfo > SetShowNormals;

	/** Show vertex tangents */
	TSharedPtr< FUICommandInfo > SetShowTangents;

	/** Show vertex binormals */
	TSharedPtr< FUICommandInfo > SetShowBinormals;

	/** Draw UV mapping to viewport */
	TSharedPtr< FUICommandInfo > AnimSetDrawUVs;
public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;
};


#endif //__AnimViewportMenuCommands_h_
