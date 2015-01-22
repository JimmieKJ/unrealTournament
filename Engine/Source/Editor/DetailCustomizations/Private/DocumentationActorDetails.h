// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class ADocumentationActor;

class FDocumentationActorDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	
	/* Handler for clicking the help button */
	FReply OnHelpButtonClicked();

	/* Handler to get the text for the button */
	FText OnGetButtonText() const;
	
	/* Handler to get the text for the button tooltip  */
	FText OnGetButtonTooltipText() const;

	/* Handler to determine if the button is enabled (link is valid)  */
	bool IsButtonEnabled() const;

private:
	/* The first documentation actor that we are showing in the details panel */
	TWeakObjectPtr<ADocumentationActor>	SelectedDocumentationActor;

	/* Handle to the string property */
	TSharedPtr<IPropertyHandle> PropertyHandle;
};
