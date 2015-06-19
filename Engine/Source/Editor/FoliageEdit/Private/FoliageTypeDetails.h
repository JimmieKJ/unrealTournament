// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFoliageTypeDetails : public IDetailCustomization
{
public:
	virtual ~FFoliageTypeDetails(){};

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder );

};
