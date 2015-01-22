// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SourceControlPrivatePCH.h"
#include "SSourceControlStatus.h"
#include "ISourceControlModule.h"

#if SOURCE_CONTROL_WITH_SLATE

#define LOCTEXT_NAMESPACE "SSourceControlStatus"

void SSourceControlStatus::Construct(const FArguments& InArgs)
{
	QueryState = EQueryState::NotQueried;

	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (SourceControlModule.IsEnabled())
	{
		SourceControlModule.GetProvider().Execute(ISourceControlOperation::Create<FConnect>(), EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &SSourceControlStatus::OnSourceControlOperationComplete));
		QueryState = EQueryState::Querying;
	}

	this->ChildSlot
	[
		SNew(SButton)
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.ContentPadding(0.0f)
		.OnClicked(this, &SSourceControlStatus::OnClicked)
		.ToolTipText(this, &SSourceControlStatus::GetSourceControlProviderStatusText)
		.Content()
		[
			SNew(SImage)
			.Image(this, &SSourceControlStatus::GetSourceControlIconImage)
		]
	];
}

FText SSourceControlStatus::GetSourceControlProviderStatusText() const 
{ 
	if(QueryState == EQueryState::Querying)
	{
		return LOCTEXT("SourceControlUnknown", "Source control status is unknown");
	}
	else
	{
		return ISourceControlModule::Get().GetProvider().GetStatusText();
	}
}

const FSlateBrush* SSourceControlStatus::GetSourceControlIconImage() const 
{ 
	if(QueryState == EQueryState::Querying)
	{
		static FName UnknownIconName("SourceControl.StatusIcon.Unknown");
		return FEditorStyle::GetBrush(UnknownIconName);
	}
	else 
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (SourceControlModule.IsEnabled())
		{
			if (!SourceControlModule.GetProvider().IsAvailable())
			{
				static FName ErrorIconName("SourceControl.StatusIcon.Error");
				return FEditorStyle::GetBrush(ErrorIconName);
			}
			else
			{
				static FName OnIconName("SourceControl.StatusIcon.On");
				return FEditorStyle::GetBrush(OnIconName);
			}
		}
		else
		{
			static FName OffIconName("SourceControl.StatusIcon.Off");
			return FEditorStyle::GetBrush(OffIconName);
		}
	}
}

FReply SSourceControlStatus::OnClicked()
{
	// Show login window regardless of current status - its useful as a shortcut to change settings.
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	SourceControlModule.ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless, EOnLoginWindowStartup::PreserveProvider);
	return FReply::Handled();
}

void SSourceControlStatus::OnSourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	QueryState = EQueryState::Queried;
}

#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE
