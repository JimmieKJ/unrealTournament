// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


/* FArrangedChildren interface
 *****************************************************************************/

void FArrangedChildren::AddWidget(const FArrangedWidget& InWidgetGeometry)
{
	AddWidget(InWidgetGeometry.Widget->GetVisibility(), InWidgetGeometry);
}
