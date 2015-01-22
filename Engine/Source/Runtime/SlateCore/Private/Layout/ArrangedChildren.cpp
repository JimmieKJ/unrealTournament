// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


/* FArrangedChildren interface
 *****************************************************************************/

void FArrangedChildren::AddWidget( const FArrangedWidget& InWidgetGeometry )
{
	AddWidget(InWidgetGeometry.Widget->GetVisibility(), InWidgetGeometry);
}


void FArrangedChildren::AddWidget( EVisibility VisibilityOverride, const FArrangedWidget& InWidgetGeometry )
{
	if (this->Accepts(VisibilityOverride))
	{
		Array.Add(InWidgetGeometry);
	}
}
