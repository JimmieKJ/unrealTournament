// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IPropertyUtilities.h"

class IPropertyTableUtilities : public IPropertyUtilities
{
public:

	virtual void RemoveColumn( const TSharedRef< class IPropertyTableColumn >& Column ) = 0;

};