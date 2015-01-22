// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MacGraphicsSwitchingModule.h"
#include "MacGraphicsSwitchingSettings.h"
#include "MacGraphicsSwitchingSettingsDetails.h"
#include "MacGraphicsSwitchingWidget.h"

#define LOCTEXT_NAMESPACE "MacGraphicsSwitchingSettingsDetails"

TSharedRef<IDetailCustomization> FMacGraphicsSwitchingSettingsDetails::MakeInstance()
{
	return MakeShareable(new FMacGraphicsSwitchingSettingsDetails());
}

void FMacGraphicsSwitchingSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	TSharedRef<IPropertyHandle> PreferredRendererPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMacGraphicsSwitchingSettings, RendererID));
	DetailLayout.HideProperty("RendererID");

	bool bAllowMultiGPUs = IMacGraphicsSwitchingModule::Get().AllowMultipleGPUs();
	bool bAllowAutomaticGraphicsSwitching = IMacGraphicsSwitchingModule::Get().AllowAutomaticGraphicsSwitching();
	
	TSharedRef<IPropertyHandle> MultiGPUPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMacGraphicsSwitchingSettings, bUseMultipleRenderers));
	if (!bAllowMultiGPUs)
	{
		MultiGPUPropertyHandle->SetValue(false);
		DetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UMacGraphicsSwitchingSettings, bUseMultipleRenderers));
	}
	
	TSharedRef<IPropertyHandle> SwitchingPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMacGraphicsSwitchingSettings, bAllowAutomaticGraphicsSwitching));
	if (!bAllowAutomaticGraphicsSwitching)
	{
		SwitchingPropertyHandle->SetValue(false);
		DetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UMacGraphicsSwitchingSettings, bAllowAutomaticGraphicsSwitching));
	}
	
	IDetailCategoryBuilder& AccessorCategory = DetailLayout.EditCategory( "OpenGL" );
	AccessorCategory.AddCustomRow( LOCTEXT("PreferredRenderer", "Preferred Renderer") )
	.NameContent()
	[
		PreferredRendererPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(113)
	.MaxDesiredWidth(113)
	[
		SNew(SMacGraphicsSwitchingWidget)
		.bLiveSwitching(false)
		.PreferredRendererPropertyHandle(PreferredRendererPropertyHandle)
	];
}

#undef LOCTEXT_NAMESPACE