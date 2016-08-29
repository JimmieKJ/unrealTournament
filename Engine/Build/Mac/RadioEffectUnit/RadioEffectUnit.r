/**
 * Copyright 1998-2012 Epic Games, Inc. All Rights Reserved.
 */

#include <AudioUnit/AudioUnit.r>

#include "RadioEffectUnitVersion.h"

// Note that resource IDs must be spaced 2 apart for the 'STR ' name and description
#define kAudioUnitResID_RadioEffectUnit				10000

// So you need to define these appropriately for your audio unit.
// For the name the convention is to provide your company name and end it with a ':',
// then provide the name of the AudioUnit.
// The Description can be whatever you want.
// For an effect unit the Type and SubType should be left the way they are defined here...
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// RadioEffectUnit
#define RES_ID			kAudioUnitResID_RadioEffectUnit
#define COMP_TYPE		kAudioUnitType_Effect
#define COMP_SUBTYPE	'Rdio'
#define COMP_MANUF		'Epic'	// note that Apple has reserved all all-lower-case 4-char codes for its own use.
                                // Be sure to include at least one upper-case character in each of your codes.
#define VERSION			kRadioEffectUnitVersion
#define NAME			"Epic Games: RadioEffectUnit"
#define DESCRIPTION		"Radio effect for Unreal Engine"
#define ENTRY_POINT		"RadioEffectUnitEntry"

#include "AUResources.r"
