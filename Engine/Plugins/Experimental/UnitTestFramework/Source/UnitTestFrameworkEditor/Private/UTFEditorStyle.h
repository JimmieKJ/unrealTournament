// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class ISlateStyle;

/** 
 * 
 */
class FUTFEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static TSharedPtr<ISlateStyle> Get();
	static FName GetStyleSetName();
};