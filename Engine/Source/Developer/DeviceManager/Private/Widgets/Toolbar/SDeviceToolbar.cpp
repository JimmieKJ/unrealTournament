// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DeviceManagerPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SDeviceToolbar"


/* SDeviceToolbar interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDeviceToolbar::Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel, const TSharedPtr<FUICommandList>& InUICommandList )
{
	Model = InModel;

	// create the toolbar
	FToolBarBuilder Toolbar(InUICommandList, FMultiBoxCustomization::None);
	{
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().Claim);
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().Release);
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().Share);
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().Remove);

		Toolbar.AddSeparator();
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().Connect);
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().Disconnect);

		Toolbar.AddSeparator();
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().PowerOn);
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().PowerOff);
		Toolbar.AddToolBarButton(FDeviceDetailsCommands::Get().Reboot);
	}

	ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.IsEnabled(this, &SDeviceToolbar::HandleToolbarIsEnabled)
			.Padding(0.0f)
			[
				Toolbar.MakeWidget()
			]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

/* SDeviceToolbar callbacks
 *****************************************************************************/

bool SDeviceToolbar::HandleToolbarIsEnabled( ) const
{
	return Model->GetSelectedDeviceService().IsValid();
}


#undef LOCTEXT_NAMESPACE
