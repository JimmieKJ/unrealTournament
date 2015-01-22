// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Persona.h"

//////////////////////////////////////////////////////////////////////////
// SRetargetManager

class SRetargetManager : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SRetargetManager )
		: _Persona()
	{}
		
		/* The Persona that owns this table */
		SLATE_ARGUMENT( TWeakPtr< FPersona >, Persona )

	SLATE_END_ARGS()

	/**
	* Slate construction function
	*
	* @param InArgs - Arguments passed from Slate
	*
	*/
	void Construct( const FArguments& InArgs );

	/**
	* Destructor - resets the morph targets
	*
	*/
	virtual ~SRetargetManager();

private:
	FReply OnViewRetargetBasePose();
	FReply OnResetRetargetBasePose();
	FReply OnSaveRetargetBasePose();

	/** Pointer back to the Persona that owns us */
	TWeakPtr<FPersona> PersonaPtr;
	/** The skeletal mesh that we grab the morph targets from */
	USkeleton* Skeleton;

	/** Delegate for undo/redo transaction **/
	void PostUndo();
	FText GetToggleRetargetBasePose() const;	
};