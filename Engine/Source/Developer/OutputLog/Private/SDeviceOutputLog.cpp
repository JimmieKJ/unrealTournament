// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OutputLogPrivatePCH.h"
#include "SDeviceOutputLog.h"
#include "ITargetPlatform.h"
#include "ITargetPlatformManagerModule.h"
#include "PlatformInfo.h"

static bool IsSupportedPlatform(ITargetPlatform* Platform)
{
	static const FName AndroidPlaftomName("Android"); // TODO: currently implemented only for Android 
	
	check(Platform);
	const auto& PlatfromInfo = Platform->GetPlatformInfo();
	return PlatfromInfo.IsVanilla() && PlatfromInfo.VanillaPlatformName == AndroidPlaftomName;
}


void SDeviceOutputLog::Construct( const FArguments& InArgs )
{
	MessagesTextMarshaller = FOutputLogTextLayoutMarshaller::Create(TArray<TSharedPtr<FLogMessage>>(), &Filter);

	MessagesTextBox = SNew(SMultiLineEditableTextBox)
		.Style(FEditorStyle::Get(), "Log.TextBox")
		.TextStyle(FEditorStyle::Get(), "Log.Normal")
		.ForegroundColor(FLinearColor::Gray)
		.Marshaller(MessagesTextMarshaller)
		.IsReadOnly(true)
		.AlwaysShowScrollbars(true)
		.OnVScrollBarUserScrolled(this, &SDeviceOutputLog::OnUserScrolled)
		.ContextMenuExtender(this, &SDeviceOutputLog::ExtendTextBoxMenu);

	ChildSlot
	[
		SNew(SVerticalBox)

			// Output log area
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				MessagesTextBox.ToSharedRef()
			]
			// The console input box
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(TargetDeviceComboBox, SComboBox<FTargetDeviceEntryPtr>)
					.OptionsSource(&DeviceList)
					.ButtonStyle(FEditorStyle::Get(), "ToolBar.Button")
					.OnSelectionChanged(this, &SDeviceOutputLog::OnDeviceSelectionChanged)
					.OnGenerateWidget(this, &SDeviceOutputLog::GenerateWidgetForDeviceComboBox)
					.ContentPadding(FMargin(4.0f, 0.0f))
					.Content()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(16)
							.HeightOverride(16)
							[
								SNew(SImage).Image(this, &SDeviceOutputLog::GetSelectedTargetDeviceBrush)
							]
						]
			
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(this, &SDeviceOutputLog::GetSelectedTargetDeviceText)
						]
					]
				]

				+SHorizontalBox::Slot()
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 4.0f))
				.FillWidth(1)
				[
					SNew(SConsoleInputBox)
					.ConsoleCommandCustomExec(this, &SDeviceOutputLog::ExecuteConsoleCommand)
					.OnConsoleCommandExecuted(this, &SDeviceOutputLog::OnConsoleCommandExecuted)
					// Always place suggestions above the input line for the output log widget
					.SuggestionListPlacement( MenuPlacement_AboveAnchor )
				]
			]
	];

	bIsUserScrolled = false;
	RequestForceScroll();
	
	//
	TArray<ITargetPlatform*> Platforms = GetTargetPlatformManager()->GetTargetPlatforms();
	for (ITargetPlatform* Platform : Platforms)
	{
		if (IsSupportedPlatform(Platform))
		{
			Platform->OnDeviceDiscovered().AddRaw(this, &SDeviceOutputLog::HandleTargetPlatformDeviceDiscovered);
			Platform->OnDeviceLost().AddRaw(this, &SDeviceOutputLog::HandleTargetPlatformDeviceLost);
		}
	}
		
	// Get list of available devices
	for (ITargetPlatform* Platform : Platforms)
	{
		if (IsSupportedPlatform(Platform))
		{
			TArray<ITargetDevicePtr> TargetDevices;
			Platform->GetAllDevices(TargetDevices);

			for (const ITargetDevicePtr& Device : TargetDevices)
			{
				if (Device.IsValid())
				{
					AddDeviceEntry(Device.ToSharedRef());
				}
			}
		}
	}
}

SDeviceOutputLog::~SDeviceOutputLog()
{
	ITargetPlatformManagerModule* Module = FModuleManager::GetModulePtr<ITargetPlatformManagerModule>("TargetPlatform");
	if (Module)
	{
		TArray<ITargetPlatform*> Platforms = Module->GetTargetPlatforms();
		for (ITargetPlatform* Platform : Platforms)
		{
			Platform->OnDeviceDiscovered().RemoveAll(this);
			Platform->OnDeviceLost().RemoveAll(this);
		}
	}
}

void SDeviceOutputLog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	FScopeLock ScopeLock(&BufferedLinesSynch);
	if (BufferedLines.Num() > 0)
	{
		for (const FBufferedLine& Line : BufferedLines)
		{
			MessagesTextMarshaller->AppendMessage(*Line.Data, Line.Verbosity, Line.Category);
		}

		// Don't scroll to the bottom automatically when the user is scrolling the view or has scrolled it away from the bottom.
		if (!bIsUserScrolled)
		{
			MessagesTextBox->ScrollTo(FTextLocation(MessagesTextMarshaller->GetNumMessages() - 1));
		}

		BufferedLines.Empty(32);
	}
}

void SDeviceOutputLog::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	FScopeLock ScopeLock(&BufferedLinesSynch);
	BufferedLines.Add(FBufferedLine(V, Category, Verbosity));
}

bool SDeviceOutputLog::CanBeUsedOnAnyThread() const
{
	return true;
}

void SDeviceOutputLog::ExecuteConsoleCommand(const FString& ExecCommand)
{
	FTargetDeviceEntryPtr SelectedDeviceEntry = TargetDeviceComboBox->GetSelectedItem();
	if (SelectedDeviceEntry.IsValid())
	{
		ITargetDevicePtr PinnedPtr = SelectedDeviceEntry->DeviceWeakPtr.Pin();
		if (PinnedPtr.IsValid())
		{
			PinnedPtr->ExecuteConsoleCommand(ExecCommand);
		}
	}
}

void SDeviceOutputLog::HandleTargetPlatformDeviceLost(ITargetDeviceRef LostDevice)
{
	auto LostDeviceId = LostDevice->GetId();
	FTargetDeviceEntryPtr SelectedDeviceEntry = TargetDeviceComboBox->GetSelectedItem();

	if (SelectedDeviceEntry.IsValid() && SelectedDeviceEntry->DeviceId == LostDeviceId)
	{
		// Kill device output object, but do not clean up output in the window
		CurrentDeviceOutputPtr.Reset();
	}
	
	// Should not do it, but what if someone somewhere holds strong reference to a lost device?
	for (const TSharedPtr<FTargetDeviceEntry>& EntryPtr : DeviceList)
	{
		if (EntryPtr->DeviceId == LostDeviceId)
		{
			EntryPtr->DeviceWeakPtr = nullptr;
		}
	}
}

void SDeviceOutputLog::HandleTargetPlatformDeviceDiscovered(ITargetDeviceRef DiscoveredDevice)
{
	FTargetDeviceId DiscoveredDeviceId = DiscoveredDevice->GetId();

	int32 ExistingEntryIdx = DeviceList.IndexOfByPredicate([&](const TSharedPtr<FTargetDeviceEntry>& Other) {
		return (Other->DeviceId == DiscoveredDeviceId);
	});

	if (DeviceList.IsValidIndex(ExistingEntryIdx))
	{
		DeviceList[ExistingEntryIdx]->DeviceWeakPtr = DiscoveredDevice;
		
		if (TargetDeviceComboBox->GetSelectedItem() == DeviceList[ExistingEntryIdx])
		{
			CurrentDeviceOutputPtr = DiscoveredDevice->CreateDeviceOutputRouter(this);
		}
	}
	else
	{
		AddDeviceEntry(DiscoveredDevice);
		TargetDeviceComboBox->RefreshOptions();
	}
}

void SDeviceOutputLog::AddDeviceEntry(ITargetDeviceRef TargetDevice)
{
	using namespace PlatformInfo;
	FName DeviceIconStyleName = TargetDevice->GetTargetPlatform().GetPlatformInfo().GetIconStyleName(EPlatformIconSize::Normal);
	
	TSharedPtr<FTargetDeviceEntry> DeviceEntry = MakeShareable(new FTargetDeviceEntry());
	
	DeviceEntry->DeviceId = TargetDevice->GetId();
	DeviceEntry->DeviceName = TargetDevice->GetName();
	DeviceEntry->DeviceIconBrush = FEditorStyle::GetBrush(DeviceIconStyleName);
	DeviceEntry->DeviceWeakPtr = TargetDevice;
	
	DeviceList.Add(DeviceEntry);
}

void SDeviceOutputLog::OnDeviceSelectionChanged(FTargetDeviceEntryPtr DeviceEntry, ESelectInfo::Type SelectInfo)
{
	CurrentDeviceOutputPtr.Reset();
	OnClearLog();
	
	if (DeviceEntry.IsValid())
	{
		ITargetDevicePtr PinnedPtr = DeviceEntry->DeviceWeakPtr.Pin();
		if (PinnedPtr.IsValid() && PinnedPtr->IsConnected())
		{
			CurrentDeviceOutputPtr = PinnedPtr->CreateDeviceOutputRouter(this);
		}
	}
}

TSharedRef<SWidget> SDeviceOutputLog::GenerateWidgetForDeviceComboBox(FTargetDeviceEntryPtr DeviceEntry) const
{
	return 
		SNew(SBox)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(24)
				.HeightOverride(24)
				[
					SNew(SImage).Image(GetTargetDeviceBrush(DeviceEntry))
				]
			]
			
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4.0f, 0.0f))
			[
				SNew(STextBlock).Text(this, &SDeviceOutputLog::GetTargetDeviceText, DeviceEntry)
			]
		];
}

const FSlateBrush* SDeviceOutputLog::GetTargetDeviceBrush(FTargetDeviceEntryPtr DeviceEntry) const
{
	if (DeviceEntry.IsValid())
	{
		return DeviceEntry->DeviceIconBrush;
	}
	else
	{
		return FEditorStyle::GetBrush("Launcher.Instance_Unknown");
	}
}

const FSlateBrush* SDeviceOutputLog::GetSelectedTargetDeviceBrush() const
{
	FTargetDeviceEntryPtr DeviceEntry = TargetDeviceComboBox->GetSelectedItem();
	return GetTargetDeviceBrush(DeviceEntry);
}

FText SDeviceOutputLog::GetTargetDeviceText(FTargetDeviceEntryPtr DeviceEntry) const
{
	if (DeviceEntry.IsValid())
	{
		ITargetDevicePtr PinnedPtr = DeviceEntry->DeviceWeakPtr.Pin();
		if (PinnedPtr.IsValid() && PinnedPtr->IsConnected())
		{
			return FText::FromString(DeviceEntry->DeviceName);
		}
		else
		{
			return FText::Format(NSLOCTEXT("OutputLog", "TargetDeviceOffline", "{0} (Offline)"), FText::FromString(DeviceEntry->DeviceName));
		}
	}
	else
	{
		return NSLOCTEXT("OutputLog", "UnknownTargetDevice", "<Unknown device>");
	}
}

FText SDeviceOutputLog::GetSelectedTargetDeviceText() const
{
	FTargetDeviceEntryPtr DeviceEntry = TargetDeviceComboBox->GetSelectedItem();
	return GetTargetDeviceText(DeviceEntry);
}
