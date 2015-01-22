// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once


#include "LevelEditor.h"


/**
 * Level editor menu
 */
class FLevelEditorMenu
{

public:

	/**
	 * Static: Creates a widget for the level editor's menu
	 *
	 * @return	New widget
	 */
	static TSharedRef< SWidget > MakeLevelEditorMenu( const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor );

	static TSharedRef< SWidget > MakeNotificationBar( const TSharedPtr<FUICommandList>& CommandList, TSharedPtr<class SLevelEditor> LevelEditor );
};
