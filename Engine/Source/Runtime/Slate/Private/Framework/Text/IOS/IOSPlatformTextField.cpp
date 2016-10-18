// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "IOSAppDelegate.h"
#include "IOSView.h"


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

void FIOSPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
{
#if !PLATFORM_TVOS
	
	FIOSView* View = [IOSAppDelegate GetDelegate].IOSView;
	if (View->bIsUsingIntegratedKeyboard)
	{
		if (bShow)
		{
			dispatch_async(dispatch_get_main_queue(),^ {
				[View ActivateKeyboard:false];
			});
		}
	}
	else
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
#endif
}

#if !PLATFORM_TVOS

@implementation SlateTextField

-(void)show:(TSharedPtr<IVirtualKeyboardEntry>)InTextWidget
{
	TextWidget = InTextWidget;
	TextEntry = FText::FromString(TEXT(""));

#ifdef __IPHONE_8_0
	if ([UIAlertController class])
	{
		UIAlertController* AlertController = [UIAlertController alertControllerWithTitle : @"" message:@"" preferredStyle:UIAlertControllerStyleAlert];
		UIAlertAction* okAction = [UIAlertAction 
										actionWithTitle:NSLocalizedString(@"OK", nil)
										style:UIAlertActionStyleDefault
										handler:^(UIAlertAction* action)
										{
											[AlertController dismissViewControllerAnimated : YES completion : nil];

											UITextField* AlertTextField = AlertController.textFields.firstObject;
											TextEntry = FText::FromString(AlertTextField.text);

											FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
											AsyncTask.GameThreadCallback = ^ bool(void)
											{
												TextWidget->SetTextFromVirtualKeyboard(TextEntry, ESetTextType::Commited, ETextCommit::OnUserMovedFocus);

												// clear the TextWidget
												TextWidget = nullptr;
												return true;
											};
											[AsyncTask FinishedTask];
										}
		];
		UIAlertAction* cancelAction = [UIAlertAction
										actionWithTitle: NSLocalizedString(@"Cancel", nil)
										style:UIAlertActionStyleDefault
										handler:^(UIAlertAction* action)
										{
											[AlertController dismissViewControllerAnimated : YES completion : nil];

											FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
											AsyncTask.GameThreadCallback = ^ bool(void)
											{
												// clear the TextWidget
												TextWidget = nullptr;
												return true;
											};
											[AsyncTask FinishedTask];
										}
		];

		[AlertController addAction: okAction];
		[AlertController addAction: cancelAction];
		[AlertController
						addTextFieldWithConfigurationHandler:^(UITextField* AlertTextField)
						{
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
							case EKeyboardType::Keyboard_AlphaNumeric:
								AlertTextField.keyboardType = UIKeyboardTypeASCIICapable;
								break;
							case EKeyboardType::Keyboard_Password:
								AlertTextField.secureTextEntry = YES;
							case EKeyboardType::Keyboard_Default:
							default:
								AlertTextField.keyboardType = UIKeyboardTypeDefault;
								break;
							}
						}
		];
		[[IOSAppDelegate GetDelegate].IOSController presentViewController : AlertController animated : YES completion : nil];
	}
	else
#endif
	{
		UIAlertView* AlertView = [[UIAlertView alloc] initWithTitle:@""
									message:@""
									delegate:self
									cancelButtonTitle:NSLocalizedString(@"Cancel", nil)
									otherButtonTitles:NSLocalizedString(@"OK", nil), nil];

		// give the UIAlertView a style so a UITextField is created
		switch (TextWidget->GetVirtualKeyboardType())
		{
		case EKeyboardType::Keyboard_Password:
			AlertView.alertViewStyle = UIAlertViewStyleSecureTextInput;
			break;
		case EKeyboardType::Keyboard_AlphaNumeric:
		case EKeyboardType::Keyboard_Default:
		case EKeyboardType::Keyboard_Email:
		case EKeyboardType::Keyboard_Number:
		case EKeyboardType::Keyboard_Web:
		default:
			AlertView.alertViewStyle = UIAlertViewStylePlainTextInput;
			break;
		}

		UITextField* AlertTextField = [AlertView textFieldAtIndex : 0];
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
		case EKeyboardType::Keyboard_AlphaNumeric:
			AlertTextField.keyboardType = UIKeyboardTypeASCIICapable;
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
}

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
	UITextField* AlertTextField = [alertView textFieldAtIndex : 0];
	TextEntry = FText::FromString(AlertTextField.text);

	FIOSAsyncTask* AsyncTask = [[FIOSAsyncTask alloc] init];
    AsyncTask.GameThreadCallback = ^ bool(void)
    {
		// index 1 is the OK button
		if(buttonIndex == 1)
		{
			TextWidget->SetTextFromVirtualKeyboard(TextEntry, ESetTextType::Commited, ETextCommit::OnUserMovedFocus);
		}
    
        // clear the TextWidget
        TextWidget = nullptr;
        return true;
    };
    [AsyncTask FinishedTask];
}

@end

#endif

