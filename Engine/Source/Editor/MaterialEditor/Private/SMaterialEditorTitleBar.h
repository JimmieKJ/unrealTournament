// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// SMaterialEditorTitleBar

class SMaterialEditorTitleBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SMaterialEditorTitleBar )
		: _TitleText()
		, _MaterialInfoList(NULL)
	{}

		SLATE_ATTRIBUTE( FText, TitleText )

		SLATE_ARGUMENT( const TArray<TSharedPtr<FMaterialInfo>>*, MaterialInfoList )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Callback used to populate SListView */
	TSharedRef<ITableRow> MakeMaterialInfoWidget(TSharedPtr<FMaterialInfo> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Request a refresh of the list view */
	void RequestRefresh();

protected:
	TSharedPtr<SListView<TSharedPtr<FMaterialInfo>>>	MaterialInfoList;
};
