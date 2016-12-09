// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * A static helper class which is used to easily construct various types of AutomationDriver specific SlateMetaData.
 */
class FDriverMetaData
{
public:

	/**
	 * @return the automation driver specific metadata type for specifying an Id for a widget
	 */
	static TSharedRef<ISlateMetaData> Id(FName Tag);
};