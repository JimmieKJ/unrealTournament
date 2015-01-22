// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlProvider.h"

namespace EQueryState
{
	enum Type
	{
		NotQueried,
		Querying,
		Queried,
	};
}

#if SOURCE_CONTROL_WITH_SLATE

class SSourceControlStatus : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSourceControlStatus){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/**
	 * Delegate used to set the tooltip text of the source control status icon.
	 * @return Source control status text
	 */
	FText GetSourceControlProviderStatusText() const;

	/**
	 * Delegate used to set the image brush for the source control status icon.
	 * @return Source control status image brush
	 */
	const FSlateBrush* GetSourceControlIconImage() const;

	/**
	 * Delegate for when the button is clicked
	 */
	FReply OnClicked();

	/**
	 * Delegate called when a operation is done executing
	 * @param InCommand - The Command passed from source control subsystem that has completed it's work
	 */
	void OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

private:
	/** The state of the query */
	EQueryState::Type QueryState;
};

#endif // SOURCE_CONTROL_WITH_SLATE
