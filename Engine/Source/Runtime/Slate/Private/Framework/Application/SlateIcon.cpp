// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


/* FSlateIcon structors
 *****************************************************************************/

FSlateIcon::FSlateIcon( )
	: StyleSetName(NAME_None)
	, StyleName(NAME_None)
	, SmallStyleName(NAME_None)
	, bIsSet(false)
{ }


FSlateIcon::FSlateIcon( const FName& InStyleSetName, const FName& InStyleName )
	: StyleSetName(InStyleSetName)
	, StyleName(InStyleName)
	, SmallStyleName(ISlateStyle::Join(InStyleName, ".Small"))
	, bIsSet(true)
{ }


FSlateIcon::FSlateIcon( const FName& InStyleSetName, const FName& InStyleName, const FName& InSmallStyleName )
	: StyleSetName(InStyleSetName)
	, StyleName(InStyleName)
	, SmallStyleName(InSmallStyleName)
	, bIsSet(true)
{ }


/* FSlateIcon interface
 *****************************************************************************/

const FSlateBrush* FSlateIcon::GetSmallIcon( ) const
{
	const ISlateStyle* StyleSet = GetStyleSet();
	
	if (StyleSet)
	{
		return StyleSet->GetOptionalBrush(SmallStyleName);
	}

	return FStyleDefaults::GetNoBrush();
}


const ISlateStyle* FSlateIcon::GetStyleSet( ) const
{
	return FSlateStyleRegistry::FindSlateStyle(StyleSetName);
}


const FSlateBrush* FSlateIcon::GetIcon( ) const
{
	const ISlateStyle* StyleSet = GetStyleSet();
	
	if(StyleSet)
	{
		return StyleSet->GetOptionalBrush(StyleName);
	}

	return FStyleDefaults::GetNoBrush();
}
