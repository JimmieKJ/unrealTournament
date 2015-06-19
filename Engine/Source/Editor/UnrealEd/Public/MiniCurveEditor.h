// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorManager.h"

class UNREALED_API SMiniCurveEditor :  public SCompoundWidget,public IAssetEditorInstance
{
public:
	SLATE_BEGIN_ARGS( SMiniCurveEditor )
		: _CurveOwner(nullptr)
		, _OwnerObject(nullptr)
		{}

	SLATE_ARGUMENT( FCurveOwnerInterface*, CurveOwner )
	SLATE_ARGUMENT( UObject*, OwnerObject )
	SLATE_ARGUMENT( TWeakPtr<SWindow>, ParentWindow )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMiniCurveEditor();

	// IAssetEditorInstance interface
	virtual FName GetEditorName() const override;
	virtual void FocusWindow(UObject* ObjectToFocusOn) override;
	virtual bool CloseWindow() override;
	virtual bool IsPrimaryEditor() const override { return true; };
	virtual void InvokeTab(const struct FTabId& TabId) override {}
	virtual TSharedPtr<class FTabManager> GetAssociatedTabManager() override;
	virtual double GetLastActivationTime() override;

private:

	float ViewMinInput;
	float ViewMaxInput;

	TSharedPtr<class SCurveEditor> TrackWidget;

	float GetViewMinInput() const { return ViewMinInput; }
	float GetViewMaxInput() const { return ViewMaxInput; }
	/** Return length of timeline */
	float GetTimelineLength() const;

	void SetInputViewRange(float InViewMinInput, float InViewMaxInput);


protected:
	TWeakPtr<SWindow> WidgetWindow;
};
