// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SSessionInstanceList"


/**
 * Implements a Slate widget that lists session instances.
 */
class SSessionInstanceList
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionInstanceList) { }
		SLATE_ATTRIBUTE(ESelectionMode::Type, SelectionMode)
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SSessionInstanceList()
	{
		if (SessionManager.IsValid())
		{
			SessionManager->OnInstanceSelectionChanged().RemoveAll(this);
			SessionManager->OnSelectedSessionChanged().RemoveAll(this);
		}
	}

public:
	
	/**
	 * Construct this widget.
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InSessionManager The session manager to use.
	 */
	void Construct( const FArguments& InArgs, ISessionManagerRef InSessionManager )
	{
		IgnoreInstanceSelectionChanged = false;
		SessionManager = InSessionManager;

		ChildSlot
		[
			SAssignNew(InstanceListView, SListView<ISessionInstanceInfoPtr>)
				.ItemHeight(24.0f)
				.ListItemsSource(&InstanceList)
				.SelectionMode(InArgs._SelectionMode)
				.OnGenerateRow(this, &SSessionInstanceList::HandleInstanceListViewGenerateRow)
				.OnSelectionChanged(this, &SSessionInstanceList::HandleInstanceListViewSelectionChanged)
				.HeaderRow
				(
					SNew(SHeaderRow)

					+ SHeaderRow::Column("Name")
						.DefaultLabel(LOCTEXT("InstanceListNameColumnHeader", "Instance"))

					+ SHeaderRow::Column("Type")
						.DefaultLabel(LOCTEXT("InstanceListTypeColumnHeader", "Type"))

					+ SHeaderRow::Column("Device")
						.DefaultLabel(LOCTEXT("InstanceListDeviceColumnHeader", "Device"))

					+ SHeaderRow::Column("Platform")
						.DefaultLabel(LOCTEXT("InstanceListPlatformColumnHeader", "Platform"))

					+ SHeaderRow::Column("Status")
						.DefaultLabel(LOCTEXT("InstanceListStatusColumnHeader", "Status"))
						.HAlignCell(HAlign_Right)
						.HAlignHeader(HAlign_Right)
				)
		];

		SessionManager->OnInstanceSelectionChanged().AddSP(this, &SSessionInstanceList::HandleSessionManagerInstanceSelectionChanged);
		SessionManager->OnSelectedSessionChanged().AddSP(this, &SSessionInstanceList::HandleSessionManagerSelectedSessionChanged);
	}

protected:

	/** Updates the list of instances. */
	void UpdateList()
	{
		// populate the instance list
		ISessionInfoPtr SelectedSession = SessionManager->GetSelectedSession();

		if (SelectedSession.IsValid())
		{
			SelectedSession->GetInstances(InstanceList);
		}
		else
		{
			InstanceList.Reset();
		}

		InstanceListView->RequestListRefresh();

		// restore selections
		for (int32 Index = 0; Index < InstanceList.Num(); ++Index)
		{
			ISessionInstanceInfoRef Instance = InstanceList[Index].ToSharedRef();

			InstanceListView->SetItemSelection(Instance, SessionManager->IsInstanceSelected(Instance));
		}
	}

private:

	/** Handles getting the tool tip text for an instance. */
	FText HandleInstanceGetToolTipText( ISessionInstanceInfoPtr Instance ) const
	{
		FTextBuilder ToolTipTextBuilder;

		ToolTipTextBuilder.AppendLineFormat(LOCTEXT("InstanceToolTipBuildDate", "Build Date: {0}"), FText::FromString(Instance->GetBuildDate()));
		ToolTipTextBuilder.AppendLineFormat(LOCTEXT("InstanceToolTipConsoleBuild", "Console Build: {0}"), Instance->IsConsole() ? LOCTEXT("LabelYes", "Yes") : LOCTEXT("LabelNo", "No"));
		ToolTipTextBuilder.AppendLineFormat(LOCTEXT("InstanceToolTipEngineVersion", "Engine Version: {0}"), Instance->GetEngineVersion() == 0 ? LOCTEXT("CustomBuildVersion", "Custom Build") : FText::FromString(FString::FromInt(Instance->GetEngineVersion())));
		ToolTipTextBuilder.AppendLineFormat(LOCTEXT("InstanceToolTipInstanceId", "Instance ID: {0}"), FText::FromString(Instance->GetInstanceId().ToString(EGuidFormats::DigitsWithHyphensInBraces)));

		return ToolTipTextBuilder.ToText();
	}

	/** Handles creating a row widget for the instance list view. */
	TSharedRef<ITableRow> HandleInstanceListViewGenerateRow( ISessionInstanceInfoPtr Item, const TSharedRef<STableViewBase>& OwnerTable )
	{
		return SNew(SSessionBrowserInstanceListRow, OwnerTable)
			.InstanceInfo(Item)
			.ToolTipText(this, &SSessionInstanceList::HandleInstanceGetToolTipText, Item);
	}

	/** Handles changing the instance list selection. */
	void HandleInstanceListViewSelectionChanged( ISessionInstanceInfoPtr Item, ESelectInfo::Type SelectInfo )
	{
		IgnoreInstanceSelectionChanged = true;

		for (int32 Index = 0; Index < InstanceList.Num(); ++Index)
		{
			const ISessionInstanceInfoPtr& Instance = InstanceList[Index];

			SessionManager->SetInstanceSelected(Instance, InstanceListView->IsItemSelected(Instance));
		}

		IgnoreInstanceSelectionChanged = false;
	}

	/** Handles changing the selection state of an instance. */
	void HandleSessionManagerInstanceSelectionChanged()
	{
		if (!IgnoreInstanceSelectionChanged)
		{
			UpdateList();
		}
	}

	/** Handles changing the selected session in the session manager. */
	void HandleSessionManagerSelectedSessionChanged( const ISessionInfoPtr& SelectedSession )
	{
		UpdateList();
	}

private:

	/** Holds a flag indicating whether instance selection changes should be ignored. */
	bool IgnoreInstanceSelectionChanged;

	/** Holds the list of instances. */
	TArray<ISessionInstanceInfoPtr> InstanceList;

	/** Holds the instance list view. */
	TSharedPtr<SListView<ISessionInstanceInfoPtr>> InstanceListView;

	/** Holds a reference to the session manager. */
	ISessionManagerPtr SessionManager;
};


#undef LOCTEXT_NAMESPACE
