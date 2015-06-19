// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"

#include "KismetEditorUtilities.h"
#include "DlgPickAssetPath.h"

#include "DesktopPlatformModule.h"
#include "ModuleDescriptor.h"
#include "PluginDescriptor.h"
#include "ISourceControlModule.h"
#include "IPluginManager.h"

#include "PluginHelpers.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"

#include "Editor/PropertyEditor/Public/PropertyEditing.h"


struct FPluginTemplateDescription
{
	// Name of this template in the GUI
	FText Name;

	// Description of this template in the GUI
	FText Description;

	// Name of the picture for this template in the style
	FName ImageStylePath;

	// Name of the directory containing template files
	FString OnDiskPath;

	// Whether this template needs the UI files generated (commands, etc...)
	bool bIncludeUI;

	FPluginTemplateDescription(FText InName, FText InDescription, FName InStylePath, FString InOnDiskPath, bool bInIncludeUI)
		: Name(InName)
		, Description(InDescription)
		, ImageStylePath(InStylePath)
		, OnDiskPath(InOnDiskPath)
		, bIncludeUI(bInIncludeUI)
	{
	}
};

class SPluginCreatorTabContent : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SPluginCreatorTabContent)
	{}
		SLATE_EVENT(FSimpleDelegate, OnPluginCreated)
	SLATE_END_ARGS()

	SPluginCreatorTabContent();
	~SPluginCreatorTabContent();

	void Construct(const FArguments& InArgs, TSharedPtr<SDockTab> InOwnerTab);
	
	/** Get a widget that will be displayed under Blank template tab*/
	TSharedRef<SWidget> GetTemplateDescriptionWidget(const FText& TemplateDescription, const FSlateBrush* TemplateImage);

	bool IsPluginNameValid() const { return bPluginNameIsValid; }

	/** Getter for Editor icon brush*/
	const FSlateBrush* GetPluginEditorImage() const;

	/** Getter for toolbar button icon brush*/
	const FSlateBrush* GetPluginButtonImage() const;

	/** Called upon Editor icon click*/
	FReply OnChangePluginEditorIcon();

	/** Called upon Toolbar button icon click*/
	FReply OnChangePluginButtonIcon();

	/** Bring up a dialog that will let us choose a png file
	*	@param OutFiles - files that were selected
	*	@return wether files were opened or dialog was closed*/
	bool OpenImageDialog(TArray<FString>& OutFiles);

	/** Called when user switches templates tab
	*	@param InNewTab - selected tab name*/
	void OnTemplatesTabSwitched(const FText& InNewTab);

	/** Init property panel that will display plugin descriptor details*/
	void InitDetailsView();

	void OnUsePrivatePublicSplitChanged(ECheckBoxState InState);

	void OnIsEnginePluginChanged(ECheckBoxState InState);

	TSharedRef<SWidget> GetPathTextWidget();

	FText GetPluginDestinationPath() const;

	/** Called when Plugin Name textbox changes value*/
	void OnPluginNameTextChanged(const FText& InText);

	/** Helper function to create directory under given path
	*	@param InPath - path to a directory that you want to create*/
	bool MakeDirectory(const FString& InPath);

	bool WritePluginDescriptor(const FString& PluginModuleName, const FString& UPluginFilePath);

	/** Helper function to delete given directory
	* @param InPath - full path to directory that you want to remove*/
	void DeletePluginDirectory(const FString& InPath);

	/** This is where all the magic happens.
	*	Create actual plugin using parameters collected from other widgets*/
	FReply OnCreatePluginClicked();

	bool CopyFile(const FString& DestinationFile, const FString& SourceFile, TArray<FString>& InOutCreatedFiles);

	void PopErrorNotification(const FText& ErrorMessage);

private:
	/** Name of the plugin you want to create*/
	FText PluginNameText;

	/** Property panel used to display plugin descriptor parameters*/
	TSharedPtr<class IDetailsView> PropertyView;

	/** We need UObject based class to display it in property panel*/
	class UPluginDescriptorObject* DescriptorObject;

	/** Do you want to use Private/Public source folder split? 
	 *	If so, headers will go to Public folder and source files will be put to Private.
	 *	True by default
	 */
	bool bUsePublicPrivateSplit;

	/** Should new plugin be considered as 'Engine' plugin?
	 *	If so, it will be put under Engine/Plugins directory.
	 *	False by default
	 */
	bool bIsEnginePlugin;

	/** Textbox widget that user will put plugin name in*/
	TSharedPtr<SEditableTextBox> PluginNameTextBox;

	/** Widget switcher used to switch between different templates tabs*/
	TSharedPtr<class SWidgetSwitcher> TemplateWidgetSwitcher;

	bool bPluginNameIsValid;

	/** Index of selected template. */
	int32 SelectedTemplateIndex;

	/** List of known templates */
	TArray<FPluginTemplateDescription> Templates;

	/** Full path to plugin editor icon*/
	FString PluginEditorIconPath;

	/** Full path to toolbar button icon*/
	FString PluginButtonIconPath;

	/** Delegate that will be called after plugin creation*/
	FSimpleDelegate OnPluginCreated;

	TWeakPtr<SDockTab> OwnerTab;

	TSharedPtr<FSlateDynamicImageBrush> PluginEditorIconBrush;
	TSharedPtr<FSlateDynamicImageBrush> PluginButtonIconBrush;
};