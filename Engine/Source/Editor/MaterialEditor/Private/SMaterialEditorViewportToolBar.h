// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __SLevelViewportToolBar_h__
#define __SLevelViewportToolBar_h__

#include "Editor/UnrealEd/Public/SViewportToolBar.h"

/**
 * A material editor viewport toolbar widget that is placed in a viewport
 */
class SMaterialEditorViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SMaterialEditorViewportToolBar ){}
		SLATE_ARGUMENT( TSharedPtr<class SMaterialEditorViewport>, Viewport )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
};

#endif
