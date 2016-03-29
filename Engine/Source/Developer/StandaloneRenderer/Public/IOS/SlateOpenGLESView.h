// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <GLKit/GLKit.h>

@interface SlateOpenGLESView : GLKView

@property(retain) EAGLContext* Context;

@end

@interface SlateOpenGLESViewController : UIViewController

@end
