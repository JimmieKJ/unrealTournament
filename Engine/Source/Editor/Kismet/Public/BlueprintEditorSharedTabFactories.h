// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#define LOCTEXT_NAMESPACE "BlueprintEditor"

//////////////////////////////////////////////////////////////////////////
// FCompilerResultsSummoner

struct KISMET_API FCompilerResultsSummoner : public FWorkflowTabFactory
{
public:
	FCompilerResultsSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return LOCTEXT("CompilerResultsTooltip", "The compiler results tab shows any errors or warnings generated when compiling this Blueprint.");
	}
};

#undef LOCTEXT_NAMESPACE
