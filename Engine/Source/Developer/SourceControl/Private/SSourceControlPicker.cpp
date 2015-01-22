// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SourceControlPrivatePCH.h"
#include "SSourceControlPicker.h"
#include "SourceControlModule.h"
#include "SSourceControlStatus.h"
#include "SSourceControlLogin.h"

#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SSourceControlPicker"

void SSourceControlPicker::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryTop") )
		.Padding( FMargin( 0.0f, 3.0f, 1.0f, 0.0f ) )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			.Padding(2.0f)
			[
				SNew( STextBlock )
				.Text( LOCTEXT("ProviderLabel", "Provider") )
				.Font( FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font")) )
			]
			+SHorizontalBox::Slot()
			.FillWidth(2.0f)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SSourceControlPicker::OnGetMenuContent)
				.ContentPadding(1)
				.ToolTipText( LOCTEXT("ChooseProvider", "Choose the source control provider you want to use before you edit login settings.") )
				.ButtonContent()
				[
					SNew( STextBlock )
					.Text( this, &SSourceControlPicker::OnGetButtonText )
					.Font( FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font")) )
				]
			]
		]
	];
}

void SSourceControlPicker::ChangeSourceControlProvider(int32 ProviderIndex) const
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();
	SourceControlModule.SetCurrentSourceControlProvider(ProviderIndex);

	if(SourceControlModule.GetLoginWidget().IsValid())
	{
		SourceControlModule.GetLoginWidget()->RefreshSettings();
	}
}

TSharedRef<SWidget> SSourceControlPicker::OnGetMenuContent() const
{
	FSourceControlModule& SourceControlModule = FSourceControlModule::Get();

	SourceControlModule.RefreshSourceControlProviders();

	FMenuBuilder MenuBuilder(true, NULL);

	// Get the provider names first so that we can sort them for the UI
	TArray< TPair<FName, int32> > SortedProviderNames;
	const int NumProviders = SourceControlModule.GetNumSourceControlProviders();
	SortedProviderNames.Reserve(NumProviders);
	for(int ProviderIndex = 0; ProviderIndex < NumProviders; ++ProviderIndex)
	{
		const FName ProviderName = SourceControlModule.GetSourceControlProviderName(ProviderIndex);
		SortedProviderNames.Add(TPairInitializer<FName, int32>(ProviderName, ProviderIndex));
	}

	// Sort based on the provider name
	SortedProviderNames.Sort([](const TPair<FName, int32>& One, const TPair<FName, int32>& Two)
	{
		return One.Key < Two.Key;
	});

	for(auto SortedProviderNameIter = SortedProviderNames.CreateConstIterator(); SortedProviderNameIter; ++SortedProviderNameIter)
	{
		const FText ProviderText = GetProviderText(SortedProviderNameIter->Key);
		const int32  ProviderIndex = SortedProviderNameIter->Value;

		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("ProviderName"), ProviderText );
		MenuBuilder.AddMenuEntry(
			ProviderText,
			FText::Format(LOCTEXT("SourceControlProvider_Tooltip", "Use {ProviderName} as source control provider"), Arguments),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SSourceControlPicker::ChangeSourceControlProvider, ProviderIndex ),
				FCanExecuteAction()
				)
			);
	}

	return MenuBuilder.MakeWidget();
}

FText SSourceControlPicker::OnGetButtonText() const
{
	return GetProviderText( ISourceControlModule::Get().GetProvider().GetName() );
}

FText SSourceControlPicker::GetProviderText(const FName& InName) const
{
	if(InName == "None")
	{
		return LOCTEXT("NoProviderDescription", "None (Run Without Source Control)");
	}
	return FText::FromName(InName);
}
#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
