// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __LevelViewportLayoutThreePanes_h__
#define __LevelViewportLayoutThreePanes_h__

#pragma once

#include "LevelViewportLayout.h"

class FLevelViewportLayoutThreePanes : public FLevelViewportLayout
{
public:
	/**
	 * Saves viewport layout information between editor sessions
	 */
	virtual void SaveLayoutString(const FString& LayoutString) const override;
protected:
	/**
	 * Creates the viewports and splitter for the two panes vertical layout                   
	 */
	virtual TSharedRef<SWidget> MakeViewportLayout(const FString& LayoutString) override;

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) = 0;

	/** Overridden from FLevelViewportLayout */
	virtual void ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement ) override;


protected:
	/** The splitter widgets */
	TSharedPtr< class SSplitter > PrimarySplitterWidget;
	TSharedPtr< class SSplitter > SecondarySplitterWidget;
};

// FLevelViewportLayoutThreePanesLeft /////////////////////////////

class FLevelViewportLayoutThreePanesLeft : public FLevelViewportLayoutThreePanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::ThreePanesLeft; }

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) override;
};


// FLevelViewportLayoutThreePanesRight /////////////////////////////

class FLevelViewportLayoutThreePanesRight : public FLevelViewportLayoutThreePanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::ThreePanesRight; }

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) override;
};


// FLevelViewportLayoutThreePanesTop /////////////////////////////

class FLevelViewportLayoutThreePanesTop : public FLevelViewportLayoutThreePanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::ThreePanesTop; }

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) override;
};


// FLevelViewportLayoutThreePanesBottom /////////////////////////////

class FLevelViewportLayoutThreePanesBottom : public FLevelViewportLayoutThreePanes
{
public:
	virtual const FName& GetLayoutTypeName() const override { return LevelViewportConfigurationNames::ThreePanesBottom; }

	virtual TSharedRef<SWidget> MakeThreePanelWidget(
		TArray<TSharedPtr< SLevelViewport >>& ViewportWidgets,
		const FString& ViewportKey0, const FString& ViewportKey1, const FString& ViewportKey2,
		float PrimarySplitterPercentage, float SecondarySplitterPercentage) override;
};

#endif
