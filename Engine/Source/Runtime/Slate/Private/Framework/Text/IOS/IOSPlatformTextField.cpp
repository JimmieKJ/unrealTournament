// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


FIOSPlatformTextField::FIOSPlatformTextField()
	: TextField( nullptr )
{
}

FIOSPlatformTextField::~FIOSPlatformTextField()
{
	if(TextField != nullptr)
	{
		[TextField release];
		TextField = nullptr;
	}
}

void FIOSPlatformTextField::ShowVirtualKeyboard(bool bShow, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
{
	if(TextField == nullptr)
	{
		TextField = [SlateTextField alloc];
	}

	if(bShow)
	{
		// these functions must be run on the main thread
		dispatch_async(dispatch_get_main_queue(),^ {
			[TextField show: TextEntryWidget];
		});
	}
}


@implementation SlateTextField

-(void)show:(TSharedPtr<IVirtualKeyboardEntry>)InTextWidget
{
	TextWidget = InTextWidget;

	UIAlertView* AlertView = [[UIAlertView alloc] initWithTitle:@""
													message:@""
													delegate:self
													cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
													otherButtonTitles:NSLocalizedString(@"OK", nil), nil];

	// give the UIAlertView a style so a UITextField is created
	switch(TextWidget->GetVirtualKeyboardType())
	{
		case EKeyboardType::Keyboard_Password:
			AlertView.alertViewStyle = UIAlertViewStyleSecureTextInput;
			break;
		case EKeyboardType::Keyboard_Default:
		case EKeyboardType::Keyboard_Email:
		case EKeyboardType::Keyboard_Number:
		case EKeyboardType::Keyboard_Web:
		default:
			AlertView.alertViewStyle = UIAlertViewStylePlainTextInput;
			break;
	}

	UITextField* AlertTextField = [AlertView textFieldAtIndex: 0];
	AlertTextField.clearsOnBeginEditing = NO;
	AlertTextField.clearsOnInsertion = NO;
	AlertTextField.autocorrectionType = UITextAutocorrectionTypeNo;
	AlertTextField.autocapitalizationType = UITextAutocapitalizationTypeNone;
	AlertTextField.text = [NSString stringWithFString : TextWidget->GetText().ToString()];
	AlertTextField.placeholder = [NSString stringWithFString : TextWidget->GetHintText().ToString()];

	// set up the keyboard styles not supported in the AlertViewStyle styles
	switch (TextWidget->GetVirtualKeyboardType())
	{
	case EKeyboardType::Keyboard_Email:
		AlertTextField.keyboardType = UIKeyboardTypeEmailAddress;
		break;
	case EKeyboardType::Keyboard_Number:
		AlertTextField.keyboardType = UIKeyboardTypeDecimalPad;
		break;
	case EKeyboardType::Keyboard_Web:
		AlertTextField.keyboardType = UIKeyboardTypeURL;
		break;
	case EKeyboardType::Keyboard_Default:
	case EKeyboardType::Keyboard_Password:
	default:
		// nothing to do, UIAlertView style handles these keyboard types
		break;
	}

	[AlertView show];

	[AlertView release];
}

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
	// index 1 is the OK button
	if(buttonIndex == 1)
	{
		UITextField* AlertTextField = [alertView textFieldAtIndex: 0];
		TextWidget->SetTextFromVirtualKeyboard(FText::FromString(AlertTextField.text));
	}
    
    FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
    AsyncTask.GameThreadCallback = ^ bool(void)
    {
        // clear the TextWidget
        TextWidget = nullptr;
        return true;
    };
    [AsyncTask FinishedTask];
}

@end

