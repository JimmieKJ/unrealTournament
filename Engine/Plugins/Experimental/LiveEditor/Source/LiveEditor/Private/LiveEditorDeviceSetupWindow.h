// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// SLiveEditorWizardWindow

class SLiveEditorWizardWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveEditorWizardWindow){}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	void Activated();
	
private:
	void GenerateStateContent();

	FText GetTitle() const;
	FText GetButtonText() const;

	EVisibility ShowCancelButton() const;
	bool ButtonEnabled() const;

	FReply OnCancelClicked();
	FReply OnButtonClicked();

	TSharedPtr< class SVerticalBox > Root;
	TSharedPtr< class SBorder > DynamicContentPane;
};
