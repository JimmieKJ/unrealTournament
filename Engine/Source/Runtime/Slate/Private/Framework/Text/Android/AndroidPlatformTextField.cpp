// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


// Java InputType class
#define TYPE_CLASS_TEXT						0x00000001
#define TYPE_CLASS_NUMBER					0x00000002

// Java InputType text variation flags
#define TYPE_TEXT_VARIATION_EMAIL_ADDRESS	0x00000020
#define TYPE_TEXT_VARIATION_NORMAL			0x00000000
#define TYPE_TEXT_VARIATION_PASSWORD		0x00000080
#define TYPE_TEXT_VARIATION_URI				0x00000010

// Java InputType text flags
#define TYPE_TEXT_FLAG_NO_SUGGESTIONS		0x00080000


void FAndroidPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
{
	if(bShow)
	{
		// Set the EditBox inputType based on keyboard type
		int32 InputType;
		switch (TextEntryWidget->GetVirtualKeyboardType())
		{
		case EKeyboardType::Keyboard_Number:
			InputType = TYPE_CLASS_NUMBER | TYPE_TEXT_VARIATION_NORMAL;
			break;
		case EKeyboardType::Keyboard_Web:
			InputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_URI;
			break;
		case EKeyboardType::Keyboard_Email:
			InputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
			break;
		case EKeyboardType::Keyboard_Password:
			InputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_PASSWORD;
			break;
		case EKeyboardType::Keyboard_AlphaNumeric:
		case EKeyboardType::Keyboard_Default:
		default:
			InputType = TYPE_CLASS_TEXT | TYPE_TEXT_VARIATION_NORMAL;
			break;
		}

		// Do not make suggestions as user types
		InputType |= TYPE_TEXT_FLAG_NO_SUGGESTIONS;

		// Show alert for input
		extern void AndroidThunkCpp_ShowVirtualKeyboardInput(TSharedPtr<IVirtualKeyboardEntry>, int32, const FString&, const FString&);
		AndroidThunkCpp_ShowVirtualKeyboardInput(TextEntryWidget, InputType, TextEntryWidget->GetHintText().ToString(), TextEntryWidget->GetText().ToString());
	}
}
