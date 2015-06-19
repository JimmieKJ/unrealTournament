// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPlatformTextField.h"

#import <UIKit/UIKit.h>

@class SlateTextField;

class FIOSPlatformTextField : public IPlatformTextField
{
public:
	FIOSPlatformTextField();
	virtual ~FIOSPlatformTextField();

	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;

private:
	SlateTextField* TextField;
};

typedef FIOSPlatformTextField FPlatformTextField;


@interface SlateTextField : NSObject<UIAlertViewDelegate>
{
	TSharedPtr<IVirtualKeyboardEntry> TextWidget;
}

-(void)show:(TSharedPtr<IVirtualKeyboardEntry>)InTextWidget;

@end
