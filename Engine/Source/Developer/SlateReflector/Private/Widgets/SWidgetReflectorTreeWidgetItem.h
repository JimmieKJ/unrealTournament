// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Widget that visualizes the contents of a FReflectorNode.
 */
class SLATEREFLECTOR_API SReflectorTreeWidgetItem
	: public SMultiColumnTableRow<TSharedPtr<FReflectorNode>>
{
public:

	SLATE_BEGIN_ARGS(SReflectorTreeWidgetItem)
		: _WidgetInfoToVisualize()
		, _SourceCodeAccessor()
		, _AssetAccessor()
	{ }

		SLATE_ARGUMENT(TSharedPtr<FReflectorNode>, WidgetInfoToVisualize)
		SLATE_ARGUMENT(FAccessSourceCode, SourceCodeAccessor)
		SLATE_ARGUMENT(FAccessAsset, AssetAccessor)

	SLATE_END_ARGS()

public:

	/**
	 * Construct child widgets that comprise this widget.
	 *
	 * @param InArgs Declaration from which to construct this widget.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		this->WidgetInfo = InArgs._WidgetInfoToVisualize;
		this->OnAccessSourceCode = InArgs._SourceCodeAccessor;
		this->OnAccessAsset = InArgs._AssetAccessor;

		TSharedPtr<SWidget> Widget = WidgetInfo.Get()->Widget.Pin();
		if ( Widget.IsValid() )
		{
			CachedWidgetType = FText::FromString(Widget->GetTypeAsString());
			CachedWidgetFile = Widget->GetCreatedInLocation().GetPlainNameString();
			CachedWidgetLineNumber = Widget->GetCreatedInLocation().GetNumber();
			CachedWidgetVisibility = FText::FromString(Widget->GetVisibility().ToString());

			CachedReadableLocation = Widget->GetReadableLocation();
		}

		SMultiColumnTableRow< TSharedPtr<FReflectorNode> >::Construct( SMultiColumnTableRow< TSharedPtr<FReflectorNode> >::FArguments().Padding(1), InOwnerTableView );
	}

public:

	// SMultiColumnTableRow overrides

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

protected:

	/** @return String representation of the widget we are visualizing */
	FText GetWidgetType() const
	{
		return CachedWidgetType;
	}
	
	virtual FString GetReadableLocation() const override;

	FText GetReadableLocationAsText() const
	{
		return FText::FromString(GetReadableLocation());
	}

	FString GetWidgetFile() const
	{
		return CachedWidgetFile;
	}

	int32 GetWidgetLineNumber() const
	{
		return CachedWidgetLineNumber;
	}

	FText GetVisibilityAsString() const
	{
		return CachedWidgetVisibility;
	}

	/** @return The tint of the reflector node */
	FSlateColor GetTint() const
	{
		return WidgetInfo.Get()->Tint;
	}

	void HandleHyperlinkNavigate();

private:

	/** The info about the widget that we are visualizing. */
	TAttribute< TSharedPtr<FReflectorNode> > WidgetInfo;

	FText CachedWidgetType;
	FString CachedWidgetFile;
	int32 CachedWidgetLineNumber;
	FText CachedWidgetVisibility;

	FString CachedReadableLocation;

	FAccessSourceCode OnAccessSourceCode;

	FAccessAsset OnAccessAsset;
};
