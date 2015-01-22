// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __SoundClassEditorUtilities_h__
#define __SoundClassEditorUtilities_h__

#pragma once

#include "Engine.h"

//////////////////////////////////////////////////////////////////////////
// FSoundClassEditorUtilities

class SOUNDCLASSEDITOR_API FSoundClassEditorUtilities
{
public:

	/**
	 * Create a new SoundClass
	 *
	 * @param	Graph		Graph representing sound classes
	 * @param	FromPin		Pin that was dragged to create sound class
	 * @param	Location	Location for new sound class
	 */
	static void CreateSoundClass(const class UEdGraph* Graph, class UEdGraphPin* FromPin, const FVector2D& Location, FString Name);

private:
	FSoundClassEditorUtilities() {}
};

#endif//__SoundClassEditorUtilities_h__
