// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PropertyEditorConstants.h"

class SPropertyTreeCategoryRow : public STableRow< TSharedPtr< class FPropertyNode* > >
{
public:

	SLATE_BEGIN_ARGS( SPropertyTreeCategoryRow )
		: _DisplayName()
	{}
		SLATE_ARGUMENT( FText, DisplayName )

	SLATE_END_ARGS()


	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable )
	{
		STableRow< TSharedPtr< class FPropertyNode* > >::Construct(
			STableRow< TSharedPtr< class FPropertyNode* > >::FArguments()
				.Padding(3)
				[
					SNew(SBorder)
					.VAlign(VAlign_Center)
					.BorderImage(FEditorStyle::GetBrush("PropertyWindow.CategoryBackground"))
					.Padding(FMargin(5,1,5,0))
					.ForegroundColor(FEditorStyle::GetColor("PropertyWindow.CategoryForeground"))
					[
						SNew( STextBlock )
						.Text( InArgs._DisplayName )
						.Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::CategoryFontStyle ) )
					]
				]
			, InOwnerTable );
	}
};