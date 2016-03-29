// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Widget that visualizes the contents of a FReflectorNode.
 */
class SLATEREFLECTOR_API SReflectorTreeWidgetItem
	: public SMultiColumnTableRow<TSharedRef<FWidgetReflectorNodeBase>>
{
public:

	SLATE_BEGIN_ARGS(SReflectorTreeWidgetItem)
		: _WidgetInfoToVisualize()
		, _SourceCodeAccessor()
		, _AssetAccessor()
	{ }

		SLATE_ARGUMENT(TSharedPtr<FWidgetReflectorNodeBase>, WidgetInfoToVisualize)
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

		check(WidgetInfo.IsValid());
		CachedWidgetType = WidgetInfo->GetWidgetType();
		CachedWidgetVisibility = WidgetInfo->GetWidgetVisibilityText();
		CachedReadableLocation = WidgetInfo->GetWidgetReadableLocation();
		CachedWidgetFile = WidgetInfo->GetWidgetFile();
		CachedWidgetLineNumber = WidgetInfo->GetWidgetLineNumber();

		SMultiColumnTableRow< TSharedRef<FWidgetReflectorNodeBase> >::Construct( SMultiColumnTableRow< TSharedRef<FWidgetReflectorNodeBase> >::FArguments().Padding(1), InOwnerTableView );
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
	
	virtual FString GetReadableLocation() const override
	{
		return CachedReadableLocation.ToString();
	}

	FText GetReadableLocationAsText() const
	{
		return CachedReadableLocation;
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
		return WidgetInfo->GetTint();
	}

	void HandleHyperlinkNavigate();

private:

	/** The info about the widget that we are visualizing. */
	TSharedPtr<FWidgetReflectorNodeBase> WidgetInfo;

	FText CachedWidgetType;
	FText CachedWidgetVisibility;
	FText CachedReadableLocation;
	FString CachedWidgetFile;
	int32 CachedWidgetLineNumber;

	FAccessSourceCode OnAccessSourceCode;

	FAccessAsset OnAccessAsset;
};
