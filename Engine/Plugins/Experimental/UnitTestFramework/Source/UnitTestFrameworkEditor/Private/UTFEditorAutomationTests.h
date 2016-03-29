// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * 
 */
struct FUTFEditorAutomationTests
{
	static const FName InEditorTestTag;
	static const FName UnitTestLevelTag;

	/**  */
	static bool IsClassSuitableForInEditorTesting(UClass* Class);
};
