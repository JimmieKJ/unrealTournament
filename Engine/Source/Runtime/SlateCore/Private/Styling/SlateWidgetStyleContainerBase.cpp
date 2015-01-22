// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "SlateWidgetStyleContainerBase.h"


DEFINE_LOG_CATEGORY(LogSlateStyle);


USlateWidgetStyleContainerBase::USlateWidgetStyleContainerBase( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


const struct FSlateWidgetStyle* const USlateWidgetStyleContainerBase::GetStyle() const
{
	return nullptr;
}
