// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealSync.h"
#include "SyncingThread.h"

#include "SlateBasics.h"
#include "StandaloneRenderer.h"
#include "LaunchEngineLoop.h"

#include "P4Env.h"

#include "ProcessHelper.h"
#include "SWidgetSwitcher.h"
#include "SDockTab.h"
#include "SlateFwd.h"
#include "SThrobber.h"
#include "SMultiLineEditableTextBox.h"
#include "BaseTextLayoutMarshaller.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "SettingsCache.h"

#define LOCTEXT_NAMESPACE "UnrealSync"

DECLARE_LOG_CATEGORY_CLASS(LogGUI, Log, All);

/**
 * Syncing log.
 */
class SSyncLog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSyncLog) {}
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		MessagesTextMarshaller = MakeShareable(new FMarshaller());
		MessagesTextBox = SNew(SMultiLineEditableTextBox)
			.IsReadOnly(true)
			.AlwaysShowScrollbars(true)
			.Marshaller(MessagesTextMarshaller);

		Clear();

		ChildSlot
			[
				MessagesTextBox.ToSharedRef()
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Adding messages to the log.
	 *
	 * @param Message Message to add.
	 */
	void AddMessage(const FString& Message)
	{
		MessagesTextMarshaller->AddMessage(Message);
		MessagesTextBox->ScrollTo(FTextLocation(NumberOfLines++));
	}

	/**
	 * Clear the log.
	 */	
	void Clear()
	{
		NumberOfLines = 0;
		MessagesTextBox->ScrollTo(FTextLocation(0));
		
		MessagesTextMarshaller->Clear();
		MessagesTextBox->Refresh();
	}

	/**
	 * Gets text version of the log.
	 */
	const FText GetPlainText()
	{
		return MessagesTextBox->GetPlainText();
	}

private:
	/**
	 * Messages marshaller.
	 */
	class FMarshaller : public FBaseTextLayoutMarshaller
	{
	public:
		void AddMessage(const FString& Message)
		{
			auto NormalText = FTextBlockStyle()
				.SetFont("Fonts/Roboto-Regular", 10)
				.SetColorAndOpacity(FSlateColor::UseForeground())
				.SetShadowOffset(FVector2D::ZeroVector)
				.SetShadowColorAndOpacity(FLinearColor::Black);

			TSharedRef<FString> LineText = MakeShareable(new FString(Message));

			TArray<TSharedRef<IRun>> Runs;
			Runs.Add(FSlateTextRun::Create(FRunInfo(), LineText, NormalText));

			if (Layout->IsEmpty())
			{
				Layout->ClearLines();
			}

			Layout->AddLine(FSlateTextLayout::FNewLineData(MoveTemp(LineText), MoveTemp(Runs)));
		}

		/**
		 * Clears messages from the text box.
		 */
		void Clear()
		{
			MakeDirty();
		}

	private:
		// FBaseTextLayoutMarshaller missing virtuals.
		virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
		{
			// Get text layout.
			Layout = &TargetTextLayout;
		}

		virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
		{
			SourceTextLayout.GetAsText(TargetString);
		}

		FTextLayout* Layout;
	};

	/** Converts the array of messages into something the text box understands */
	TSharedPtr<FMarshaller> MessagesTextMarshaller;

	/** The editable text showing all log messages */
	TSharedPtr<SMultiLineEditableTextBox> MessagesTextBox;

	/** Number of currently logged lines. */
	int32 NumberOfLines = 0;
};

/**
 * A checkbox with JSON preservable state.
 */
class SPreservableCheckBox : public SCheckBox
{
public:
	SLATE_BEGIN_ARGS(SPreservableCheckBox)	{}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		SCheckBox::FArguments BaseArgs;
		BaseArgs._Content = InArgs._Content;

		SCheckBox::Construct(BaseArgs);
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Converts current object state to JSON value.
	 *
	 * @returns JSON value object with current state.
	 */
	TSharedRef<FJsonValue> GetStateAsJSON() const
	{
		check(GetCheckedState() != ECheckBoxState::Undetermined);

		return TSharedRef<FJsonValue>(new FJsonValueBoolean(GetCheckedState() == ECheckBoxState::Checked));
	}

	/**
	 * Loads state from JSON value object.
	 *
	 * @param Value JSON value object.
	 */
	void LoadStateFromJSON(const FJsonValue& Value)
	{
		bool BoolValue;
		check(Value.TryGetBool(BoolValue) && GetCheckedState() != ECheckBoxState::Undetermined);

		if ((GetCheckedState() == ECheckBoxState::Checked) != BoolValue)
		{
			ToggleCheckedState();
		}
	}
};

/**
 * Performs a dialog when running editor is detected.
 */
class SEditorRunningDlg : public SCompoundWidget
{
public:
	enum class EResponse
	{
		ForceSync,
		Retry,
		Cancel
	};

	SLATE_BEGIN_ARGS(SEditorRunningDlg)	{}
		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ATTRIBUTE(FText, Message)
		SLATE_ATTRIBUTE(float, WrapMessageAt)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		ParentWindow = InArgs._ParentWindow.Get();
		ParentWindow->SetWidgetToFocusOnActivate(SharedThis(this));

		Response = EResponse::Retry;

		FSlateFontInfo MessageFont(FCoreStyle::Get().GetFontStyle("StandardDialog.LargeFont"));

		this->ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.FillHeight(1.0f)
					.MaxHeight(550)
					.Padding(12.0f)
					[
						SNew(SScrollBox)

						+ SScrollBox::Slot()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("UE4EditorInstanceDetectedDialog", "UE4Editor detected currently running on the system. Please close it before proceeding."))
							.Font(MessageFont)
							.WrapTextAt(InArgs._WrapMessageAt)
						]
					]

					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							+ SHorizontalBox::Slot()
								.AutoWidth()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Bottom)
								.Padding(2.f)
								[
									SNew(SUniformGridPanel)
									.SlotPadding(FCoreStyle::Get().GetMargin("StandardDialog.SlotPadding"))
									.MinDesiredSlotWidth(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
									.MinDesiredSlotHeight(FCoreStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))
									+ SUniformGridPanel::Slot(0, 0)
									[
										SNew(SButton)
										.Text(LOCTEXT("CancelSync", "Cancel sync"))
										.OnClicked(this, &SEditorRunningDlg::HandleButtonClicked, EResponse::Cancel)
										.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
										.HAlign(HAlign_Center)
									]
									+ SUniformGridPanel::Slot(1, 0)
									[
										SNew(SButton)
										.Text(LOCTEXT("RetryEditorRunning", "Retry if editor is running"))
										.OnClicked(this, &SEditorRunningDlg::HandleButtonClicked, EResponse::Retry)
										.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
										.HAlign(HAlign_Center)
									]
									+ SUniformGridPanel::Slot(2, 0)
									[
										SNew(SButton)
										.Text(LOCTEXT("ForceSync", "Force sync"))
										.OnClicked(this, &SEditorRunningDlg::HandleButtonClicked, EResponse::ForceSync)
										.ContentPadding(FCoreStyle::Get().GetMargin("StandardDialog.ContentPadding"))
										.HAlign(HAlign_Center)
									]
								]
						]
				]
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Shows modal dialog window and gets it's response.
	 */
	static EResponse ShowAndGetResponse()
	{
		TSharedPtr<SWindow> OutWindow = SNew(SWindow)
			.Title(LOCTEXT("UE4EditorInstanceDetected", "UE4Editor instance detected!"))
			.SizingRule(ESizingRule::Autosized)
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.SupportsMinimize(false).SupportsMaximize(false);

		TSharedPtr<SEditorRunningDlg> OutDialog = SNew(SEditorRunningDlg)
			.ParentWindow(OutWindow)
			.WrapMessageAt(512.0f);

		OutWindow->SetContent(OutDialog.ToSharedRef());

		FSlateApplication::Get().AddModalWindow(OutWindow.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow());

		return OutDialog->GetResponse();
	}

private:
	/**
	 * Gets response from this dialog widget.
	 *
	 * @returns Response.
	 */
	EResponse GetResponse() const
	{
		return Response;
	}

	/**
	 * Handles button click on any dialog button.
	 */
	FReply HandleButtonClicked(EResponse InResponse)
	{
		Response = InResponse;

		ParentWindow->RequestDestroyWindow();

		return FReply::Handled();
	}

	/** Parent window pointer */
	TSharedPtr<SWindow> ParentWindow;

	/** Saved response */
	EResponse Response;
};

/**
 * Simple text combo box widget.
 */
class SSimpleTextComboBox : public SCompoundWidget
{
	/* Delegate for selection changed event. */
	DECLARE_DELEGATE_TwoParams(FOnSelectionChanged, int32, ESelectInfo::Type);

	SLATE_BEGIN_ARGS(SSimpleTextComboBox){}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

public:
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		OnSelectionChanged = InArgs._OnSelectionChanged;

		this->ChildSlot
			[
				SNew(SComboBox<TSharedPtr<FString> >)
				.OptionsSource(&OptionsSource)
				.OnSelectionChanged(this, &SSimpleTextComboBox::ComboBoxSelectionChanged)
				.OnGenerateWidget(this, &SSimpleTextComboBox::MakeWidgetFromOption)
				[
					SNew(STextBlock).Text(this, &SSimpleTextComboBox::GetCurrentFTextOption)
				]
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Adds value to the options source.
	 *
	 * @param Value Value to add.
	 */
	void Add(const FString& Value)
	{
		CurrentOption = 0;
		OptionsSource.Add(MakeShareable(new FString(Value)));
	}

	/**
	 * Gets current option value.
	 *
	 * @returns Reference to current option value.
	 */
	const FString& GetCurrentOption() const
	{
		return *OptionsSource[CurrentOption];
	}

	/**
	 * Gets current option index.
	 *
	 * @returns Index of current option.
	 */
	int32 GetCurrentOptionIndex() const
	{
		return CurrentOption;
	}

	/**
	 * Sets current option for this combo box.
	 *
	 * @param Value Value to set combo box to.
	 */
	void SetCurrentOption(const FString& Value)
	{
		for (int32 OptionId = 0; OptionId < OptionsSource.Num(); ++OptionId)
		{
			if (OptionsSource[OptionId]->Equals(Value))
			{
				CurrentOption = OptionId;
			}
		}
	}

	/**
	 * Checks if options source is empty.
	 *
	 * @returns True if options source is empty. False otherwise.
	 */
	bool IsEmpty()
	{
		return OptionsSource.Num() == 0;
	}

	/**
	 * Clears the options source.
	 */
	void Clear()
	{
		CurrentOption = -1;
		OptionsSource.Empty();
	}

private:
	/**
	 * Combo box selection changed event handling.
	 *
	 * @param Value Value of the picked selection.
	 * @param SelectInfo Selection info enum.
	 */
	void ComboBoxSelectionChanged(TSharedPtr<FString> Value, ESelectInfo::Type SelectInfo)
	{
		CurrentOption = -1;
		SetCurrentOption(*Value);

		OnSelectionChanged.ExecuteIfBound(CurrentOption, SelectInfo);
	}

	/**
	 * Coverts given value into widget.
	 *
	 * @param Value Value of the option.
	 *
	 * @return Widget.
	 */
	TSharedRef<SWidget> MakeWidgetFromOption(TSharedPtr<FString> Value)
	{
		return SNew(STextBlock).Text(FText::FromString(*Value));
	}

	/**
	 * Gets current option converted to FText class.
	 *
	 * @returns Current option converted to FText class.
	 */
	FText GetCurrentFTextOption() const
	{
		return FText::FromString(OptionsSource.Num() == 0 ? "" : GetCurrentOption());
	}

	/* Currently selected option. */
	int32 CurrentOption = 0;
	/* Array of options. */
	TArray<TSharedPtr<FString> > OptionsSource;
	/* Delegate that will be called when selection had changed. */
	FOnSelectionChanged OnSelectionChanged;
};

/**
 * Widget to store and select enabled widgets by radio buttons.
 */
class SRadioContentSelection : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSelectionChanged, int32);

	/**
	 * Radio content selection item.
	 */
	class FItem : public TSharedFromThis<FItem>
	{
	public:
		/**
		 * Constructor
		 *
		 * @param InParent Parent SRadioContentSelection widget reference.
		 * @param InId Id of the item.
		 * @param InName Name of the item.
		 * @param InContent Content widget to store by this item.
		 */
		FItem(SRadioContentSelection& InParent, int32 InId, FText InName, TSharedRef<SWidget> InContent)
			: Id(InId), Name(InName), Content(InContent), Parent(InParent)
		{

		}

		/**
		 * Item initialization method.
		 */
		void Init()
		{
			Disable();

			WholeItemWidget = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), "RadioButton")
					.OnCheckStateChanged(this, &FItem::OnCheckStateChange)
					.IsChecked(this, &FItem::IsCheckboxChecked)
					[
						SNew(STextBlock)
						.Text(Name)
					]
				]
				+ SVerticalBox::Slot().VAlign(VAlign_Fill).Padding(19.0f, 2.0f, 2.0f, 2.0f)
				[
					GetContent()
				];
		}

		/**
		 * Gets this item content widget.
		 *
		 * @returns Content widget.
		 */
		TSharedRef<SWidget> GetContent()
		{
			return Content;
		}
		
		/**
		 * Gets widget that represents this item.
		 *
		 * @returns Widget that represents this item.
		 */
		TSharedRef<SWidget> GetWidget()
		{
			return WholeItemWidget.ToSharedRef();
		}

		/**
		 * Enables this item.
		 */
		void Enable()
		{
			GetContent()->SetEnabled(true);
		}

		/**
		 * Disables this item.
		 */
		void Disable()
		{
			GetContent()->SetEnabled(false);
		}

		/**
		 * Tells if check box needs to be in checked state.
		 *
		 * @returns State of the check box enum.
		 */
		ECheckBoxState IsCheckboxChecked() const
		{
			return Parent.GetChosen() == Id
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
		}

	private:
		/**
		 * Function that is called when check box state is changed.
		 *
		 * @parent InNewState New state enum.
		 */
		void OnCheckStateChange(ECheckBoxState InNewState)
		{
			Parent.ChooseEnabledItem(Id);
		}

		/* Item id. */
		int32 Id;
		/* Item name. */
		FText Name;

		/* Outer item widget. */
		TSharedPtr<SWidget> WholeItemWidget;
		/* Stored widget. */
		TSharedRef<SWidget> Content;

		/* Parent reference. */
		SRadioContentSelection& Parent;
	};

	SLATE_BEGIN_ARGS(SRadioContentSelection){}
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
			OnSelectionChanged = InArgs._OnSelectionChanged;

			MainBox = SNew(SVerticalBox);

			this->ChildSlot
				[
					MainBox.ToSharedRef()
				];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Adds widget to this selection with given name.
	 *
	 * @param Name Name of the widget in this selection.
	 * @param Content The widget itself.
	 */
	void Add(FText Name, TSharedRef<SWidget> Content)
	{
		TSharedRef<FItem> Item = MakeShareable(new FItem(*this, Items.Num(), Name, Content));
		Item->Init();
		Items.Add(Item);
		RebuildMainBox();
	}

	/**
	 * Enables chosen item and disable others.
	 *
	 * @param ItemId Chosen item id.
	 */
	void ChooseEnabledItem(int32 ItemId)
	{
		Chosen = ItemId;

		for (int32 CurrentItemId = 0; CurrentItemId < Items.Num(); ++CurrentItemId)
		{
			if (CurrentItemId == ItemId)
			{
				Items[CurrentItemId]->Enable();
			}
			else
			{
				Items[CurrentItemId]->Disable();
			}
		}

		OnSelectionChanged.ExecuteIfBound(Chosen);
	}

	/**
	 * Gets currently chosen item from this widget.
	 *
	 * @returns Chosen item id.
	 */
	int32 GetChosen() const
	{
		return Chosen;
	}

	/**
	 * Gets widget based on item id.
	 *
	 * @param ItemId Widget item id.
	 *
	 * @returns Reference to requested widget.
	 */
	TSharedRef<SWidget> GetContentWidget(int32 ItemId)
	{
		return Items[ItemId]->GetContent();
	}

	/**
	 * Converts current object state to JSON value.
	 *
	 * @returns JSON value object with current state.
	 */
	TSharedRef<FJsonValue> GetStateAsJSON() const
	{
		return MakeShareable(new FJsonValueNumber(GetChosen()));
	}

	/**
	 * Loads state from JSON value object.
	 *
	 * @param Value JSON value object.
	 */
	void LoadStateFromJSON(const FJsonValue& Value)
	{
		int32 ChosenWidget = 0;

		if (Value.TryGetNumber(ChosenWidget) && ChosenWidget < Items.Num())
		{
			ChooseEnabledItem(ChosenWidget);
		}
	}

private:
	/**
	 * Function to rebuild main and fill it with items provided.
	 */
	void RebuildMainBox()
	{
		MainBox->ClearChildren();

		for (auto Item : Items)
		{
			MainBox->AddSlot().AutoHeight().Padding(FMargin(0, 20, 0, 0))
				[
					Item->GetWidget()
				];
		}

		if (Items.Num() > 0)
		{
			ChooseEnabledItem(0);
		}
	}

	/* Delegate that will be called when the selection changed. */
	FOnSelectionChanged OnSelectionChanged;

	/* Main box ptr. */
	TSharedPtr<SVerticalBox> MainBox;
	/* Array of items/widgets. */
	TArray<TSharedRef<FItem> > Items;
	/* Chosen widget. */
	int32 Chosen;
};

/**
 * Sync latest promoted widget.
 */
class SLatestPromoted : public SCompoundWidget, public ILabelNameProvider
{
public:
	SLATE_BEGIN_ARGS(SLatestPromoted) {}
	SLATE_END_ARGS()

	SLatestPromoted()
	{
		DataReady = MakeShareable(FPlatformProcess::GetSynchEventFromPool(true));
	}

	~SLatestPromoted()
	{
		FPlatformProcess::ReturnSynchEventToPool(DataReady.Get());
	}

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs, const TSharedRef<FP4DataProxy>& InData)
	{
		Data = InData;

		this->ChildSlot
			[
				SNew(STextBlock).Text(FText::FromString("This option will sync to the latest promoted label for given game."))
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Gets command line from current options picked by user.
	 *
	 * @returns UAT UnrealSync command line.
	 */
	FString GetLabelName() const override
	{
		DataReady->Wait();

		return CurrentLabelName;
	}

	/**
	 * Gets message to display when sync task has started.
	 */
	virtual FString GetStartedMessage() const
	{
			return "Sync to latest started. Pending label data. Please wait.";
	}

	/**
	 * Gets message to display when sync task has finished.
	 */
	virtual FString GetFinishedMessage() const
	{
		return "Sync to latest finished.";
	}

	FString GetGameName() const override
	{
		DataReady->Wait();

		return ILabelNameProvider::GetGameName();
	}

	/**
	 * Refresh data.
	 *
	 * @param GameName Current game name.
	 */
	void RefreshData(const FString& GameName) override
	{
		ILabelNameProvider::RefreshData(GameName);

		auto Labels = Data.Pin()->GetPromotedLabelsForGame(GameName);
		if (Labels->Num() != 0)
		{
			CurrentLabelName = (*Labels)[0];
		}

		DataReady->Trigger();
	}

	/**
	 * Reset data.
	 *
	 * @param GameName Current game name.
	 */
	void ResetData(const FString& GameName) override
	{
		ILabelNameProvider::ResetData(GameName);

		DataReady->Reset();
	}

	/**
	 * Tells if this particular widget has data ready for sync.
	 *
	 * @returns True if ready. False otherwise.
	 */
	bool IsReadyForSync() const override
	{
		return true;
	}

	/**
	 * Converts current object state to JSON value.
	 *
	 * @returns JSON value object with current state.
	 */
	TSharedPtr<FJsonValue> GetStateAsJSON() const
	{
		return nullptr;
	}

	/**
	 * Loads state from JSON value object.
	 *
	 * @param Value JSON value object.
	 */
	void LoadStateFromJSON(const FJsonValue& Value)
	{

	}

private:
	/* Currently picked game name. */
	FString CurrentGameName;

	/* Current label name. */
	FString CurrentLabelName;

	/* Event that will trigger when data is ready. */
	TSharedPtr<FEvent> DataReady;

	/* Reference to P4 data proxy. */
	TWeakPtr<FP4DataProxy> Data;
};

/**
 * Base class for pick label widgets.
 */
class SPickLabel : public SCompoundWidget, public ILabelNameProvider
{
public:
	SLATE_BEGIN_ARGS(SPickLabel) {}
		SLATE_ATTRIBUTE(FText, Title)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs, const TSharedRef<FP4DataProxy>& InData)
	{
		Data = InData;
		LabelsCombo = SNew(SSimpleTextComboBox)
			.OnSelectionChanged(this, &SPickLabel::OnSelectionChanged);
		LabelsCombo->Add("Needs refreshing");

		this->ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(InArgs._Title)
				]
				+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
				[
					LabelsCombo.ToSharedRef()
				]
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Gets command line from current options picked by user.
	 *
	 * @returns UAT UnrealSync command line.
	 */
	FString GetLabelName() const override
	{
		return LabelsCombo->GetCurrentOption();
	}

	/**
	 * Tells if this particular widget has data ready for sync.
	 *
	 * @returns True if ready. False otherwise.
	 */
	bool IsReadyForSync() const override
	{
		return Data.Pin()->HasValidData() && !LabelsCombo->IsEmpty();
	}

	/**
	 * Reset data.
	 *
	 * @param GameName Current game name.
	 */
	void ResetData(const FString& GameName) override
	{
		ILabelNameProvider::ResetData(GameName);

		LabelsCombo->Clear();
		LabelsCombo->Add("Loading P4 labels data...");
		LabelsCombo->SetEnabled(false);
	}

	/**
	 * Converts current object state to JSON value.
	 *
	 * @returns JSON value object with current state.
	 */
	TSharedPtr<FJsonValue> GetStateAsJSON() const
	{
		if (!LabelsCombo->IsEnabled() || LabelsCombo->GetCurrentOptionIndex() < 0)
		{
			return nullptr;
		}

		return MakeShareable(new FJsonValueString(GetLabelName()));
	}

	/**
	 * Loads state from JSON value object.
	 *
	 * @param Value JSON value object.
	 */
	void LoadStateFromJSON(const FJsonValue& Value)
	{
		Value.TryGetString(LastPickedValue);
	}

protected:
	/**
	 * Reset current labels combo options.
	 *
	 * @param LabelOptions Option to fill labels combo.
	 */
	void SetLabelOptions(const TArray<FString> &LabelOptions)
	{
		LabelsCombo->Clear();

		if (Data.Pin()->HasValidData())
		{
			for (auto LabelName : LabelOptions)
			{
				LabelsCombo->Add(LabelName);
			}

			LabelsCombo->SetEnabled(true);

			if (!LastPickedValue.IsEmpty())
			{
				LabelsCombo->SetCurrentOption(LastPickedValue);
			}
		}
		else
		{
			LabelsCombo->Add("P4 data loading failed.");
			LabelsCombo->SetEnabled(false);
		}
	}

	/**
	 * Gets current data proxy.
	 */
	FP4DataProxy& GetData() { return *Data.Pin().Get(); }

private:
	/**
	 * Function is called when combo box selection is changed.
	 * It called OnGamePicked event.
	 */
	void OnSelectionChanged(int32 SelectionId, ESelectInfo::Type SelectionInfo)
	{
		LastPickedValue = LabelsCombo->GetCurrentOption();
	}

	/* Labels combo widget. */
	TSharedPtr<SSimpleTextComboBox> LabelsCombo;

	/* Reference to P4 data proxy. */
	TWeakPtr<FP4DataProxy> Data;

	/* Last picked value. */
	FString LastPickedValue;
};

/**
 * Pick promoted label widget.
 */
class SPickPromoted : public SPickLabel
{
public:
	/**
	 * Refresh widget method.
	 *
	 * @param GameName Game name to refresh for.
	 */
	void RefreshData(const FString& GameName) override
	{
		SetLabelOptions(*GetData().GetPromotedLabelsForGame(GameName));

		SPickLabel::RefreshData(GameName);
	}
};

/**
 * Pick promotable label widget.
 */
class SPickPromotable : public SPickLabel
{
public:
	/**
	 * Refresh widget method.
	 *
	 * @param GameName Game name to refresh for.
	 */
	void RefreshData(const FString& GameName) override
	{
		SetLabelOptions(*GetData().GetPromotableLabelsForGame(GameName));

		SPickLabel::RefreshData(GameName);
	}
};

/**
 * Pick any label widget.
 */
class SPickAny : public SPickLabel
{
public:
	/**
	 * Refresh widget method.
	 *
	 * @param GameName Game name to refresh for.
	 */
	void RefreshData(const FString& GameName) override
	{
		SetLabelOptions(*GetData().GetAllLabels());

		SPickLabel::RefreshData(GameName);
	}
};

/**
 * Pick game widget.
 */
class SPickGameWidget : public SCompoundWidget
{
public:
	/* Delegate for game picked event. */
	DECLARE_DELEGATE_OneParam(FOnGamePicked, const FString&);

	SLATE_BEGIN_ARGS(SPickGameWidget) {}
		SLATE_EVENT(FOnGamePicked, OnGamePicked)
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs, const TSharedRef<FP4DataProxy>& InData)
	{
		Data = InData;
		OnGamePicked = InArgs._OnGamePicked;

		GamesOptions = SNew(SSimpleTextComboBox)
			.OnSelectionChanged(this, &SPickGameWidget::OnComboBoxSelectionChanged);

		auto PossibleGameNames = Data.Pin()->GetPossibleGameNames();

		if (PossibleGameNames->Remove(FUnrealSync::GetSharedPromotableP4FolderName()))
		{
			GamesOptions->Add(FUnrealSync::GetSharedPromotableDisplayName());
		}

		for (auto PossibleGameName : *PossibleGameNames)
		{
			GamesOptions->Add(PossibleGameName);
		}

		this->ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString("Pick game to sync: "))
				]
				+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
					[
						GamesOptions.ToSharedRef()
					]
			];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Gets current game selected.
	 *
	 * @returns Currently selected game.
	 */
	const FString& GetCurrentGame() const
	{
		return GamesOptions->GetCurrentOption();
	}

	/**
	 * Converts current object state to JSON value.
	 *
	 * @returns JSON value object with current state.
	 */
	TSharedRef<FJsonValue> GetStateAsJSON() const
	{
		return TSharedRef<FJsonValueString>(new FJsonValueString(GetCurrentGame()));
	}

	/**
	 * Loads state from JSON value object.
	 *
	 * @param Value JSON value object.
	 */
	void LoadStateFromJSON(const FJsonValue& Value)
	{
		FString StrVal;
		check(Value.TryGetString(StrVal));

		GamesOptions->SetCurrentOption(StrVal);
	}

private:
	/**
	 * Function is called when combo box selection is changed.
	 * It called OnGamePicked event.
	 */
	void OnComboBoxSelectionChanged(int32 SelectionId, ESelectInfo::Type SelectionInfo)
	{
		OnGamePicked.ExecuteIfBound(GamesOptions->GetCurrentOption());
	}

	/* Game options widget. */
	TSharedPtr<SSimpleTextComboBox> GamesOptions;
	/* Delegate that is going to be called when game was picked by the user. */
	FOnGamePicked OnGamePicked;

	/* Reference to P4 data proxy. */
	TWeakPtr<FP4DataProxy> Data;
};

/**
 * Main tab widget.
 */
class SMainTabWidget : public SCompoundWidget
{
	/**
	 * External threads requests dispatcher.
	 */
	class FExternalThreadsDispatcher : public TSharedFromThis<FExternalThreadsDispatcher, ESPMode::ThreadSafe>
	{
	public:
		/** Delegate that represents rendering thread request. */
		DECLARE_DELEGATE(FRenderingThreadRequest);

		/**
		 * Executes all pending requests.
		 *
		 * Some Slate methods are restricted for either game or Slate rendering
		 * thread. If external thread tries to execute them assert raises, which is
		 * telling that it's unsafe.
		 *
		 * This method executes all pending requests on Slate thread, so you can
		 * use those to execute some restricted methods.
		 */
		EActiveTimerReturnType ExecuteRequests(double CurrentTime, float DeltaTime)
		{
			FScopeLock Lock(&RequestsMutex);
			for (int32 ReqId = 0; ReqId < Requests.Num(); ++ReqId)
			{
				Requests[ReqId].Execute();
			}

			Requests.Empty(4);

			return EActiveTimerReturnType::Continue;
		}

		/**
		 * Adds delegate to pending requests list.
		 */
		void AddRenderingThreadRequest(FRenderingThreadRequest Request)
		{
			FScopeLock Lock(&RequestsMutex);
			Requests.Add(MoveTemp(Request));
		}

	private:
		/** Mutex for pending requests list. */
		FCriticalSection RequestsMutex;

		/** Pending requests list. */
		TArray<FRenderingThreadRequest> Requests;
	};

public:
	/* On data loaded event delegate. */
	DECLARE_DELEGATE(FOnDataLoaded);

	/* On data reset event delegate. */
	DECLARE_DELEGATE(FOnDataReset);

	SMainTabWidget()
		: ExternalThreadsDispatcher(MakeShareable(new FExternalThreadsDispatcher())), P4Data(new FP4DataProxy)
	{ }

	/**
	 * Gets instance of this class.
	 */
	static TSharedPtr<SMainTabWidget> GetInstance()
	{
		return GetSingletonPtr();
	}

	/**
	 * Creates instance of this class.
	 */
	static TSharedRef<SMainTabWidget> CreateInstance()
	{
		return SAssignNew(GetSingletonPtr(), SMainTabWidget);
	}

	SLATE_BEGIN_ARGS(SMainTabWidget) {}
	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct(const FArguments& InArgs)
	{
		RadioSelection = SNew(SRadioContentSelection);

		AddToRadioSelection(LOCTEXT("SyncToLatestPromoted", "Sync to the latest promoted"), SAssignNew(LatestPromoted, SLatestPromoted, P4Data));
		AddToRadioSelection(LOCTEXT("SyncToChosenPromoted", "Sync to chosen promoted label"), SAssignNew(ChosenPromoted, SPickPromoted, P4Data).Title(LOCTEXT("PickPromotedLabel", "Pick promoted label: ")));
		AddToRadioSelection(LOCTEXT("SyncToChosenPromotable", "Sync to chosen promotable label since last promoted"), SAssignNew(ChosenPromotable, SPickPromotable, P4Data).Title(LOCTEXT("PickPromotableLabel", "Pick promotable label: ")));
		AddToRadioSelection(LOCTEXT("SyncToAnyChosen", "Sync to any chosen label"), SAssignNew(ChosenLabel, SPickAny, P4Data).Title(LOCTEXT("PickLabel", "Pick label: ")));

		TSharedPtr<SVerticalBox> MainBox;
		
		this->ChildSlot
		[
			SAssignNew(Switcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(MainBox, SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(5.0f)
				[
					SAssignNew(PickGameWidget, SPickGameWidget, P4Data)
					.OnGamePicked(this, &SMainTabWidget::OnCurrentGameChanged)
				]
				+ SVerticalBox::Slot().VAlign(VAlign_Fill)
				[
					RadioSelection.ToSharedRef()
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).HAlign(HAlign_Right)
						.OnClicked(this, &SMainTabWidget::OnReloadLabels)
						.IsEnabled(this, &SMainTabWidget::IsReloadLabelsReady)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(FText::FromString("Reload Labels"))
						]
					]
					+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						[
							SAssignNew(DeleteStaleBinariesOnSuccessfulSync, SPreservableCheckBox)
							[
								SNew(STextBlock).Text(LOCTEXT("DeleteStaleBinariesOnSuccessfulSync", "Delete stale binaries?"))
							]
						]
						+ SVerticalBox::Slot().AutoHeight()
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						[
							SAssignNew(AutoClobberSyncCheckBox, SPreservableCheckBox)
							[
								SNew(STextBlock).Text(LOCTEXT("AutoClobberSync", "Auto-clobber sync?"))
							]
						]
						+ SVerticalBox::Slot()
						[
							SAssignNew(RunUE4AfterSyncCheckBox, SPreservableCheckBox)
							[
								SNew(STextBlock).Text(LOCTEXT("RunUE4AfterSync", "Run UE4 after sync?"))
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						[
							SAssignNew(ArtistSyncCheckBox, SPreservableCheckBox)
							[
								SNew(STextBlock).Text(LOCTEXT("ArtistSync", "Artist sync?"))
							]
						]
						+ SVerticalBox::Slot()
						[
							SAssignNew(PreviewSyncCheckBox, SPreservableCheckBox)
							[
								SNew(STextBlock).Text(LOCTEXT("PreviewSync", "Preview sync?"))
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(30, 0, 0, 0)
						[
							SNew(SButton).HAlign(HAlign_Right)
							.OnClicked(this, &SMainTabWidget::OnSync)
							.IsEnabled(this, &SMainTabWidget::IsSyncReady)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("Sync!"))
							]
						]
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).HAlign(HAlign_Right)
							.OnClicked(this, &SMainTabWidget::RunUE4)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("Run UE4"))
							]
						]
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton).HAlign(HAlign_Right)
							.OnClicked(this, &SMainTabWidget::RunP4V)
							.IsEnabled(CheckIfP4VExists())
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock).Text(FText::FromString("Run P4V"))
							]
						]
				]
			]
			+ SWidgetSwitcher::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SAssignNew(Log, SSyncLog)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SAssignNew(Throbber, SThrobber)
						.Animate(SThrobber::VerticalAndOpacity).NumPieces(5)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(CancelButton, SButton)
						.OnClicked(this, &SMainTabWidget::OnCancelButtonClick)
						[
							SNew(STextBlock).Text(LOCTEXT("Cancel", "Cancel"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.OnClicked(this, &SMainTabWidget::OnSaveLogButtonClick)
						[
							SNew(STextBlock).Text(LOCTEXT("SaveLog", "Save Log..."))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(GoBackButton, SButton)
						.IsEnabled(false)
						.OnClicked(this, &SMainTabWidget::OnGoBackButtonClick)
						[
							SNew(STextBlock).Text(LOCTEXT("GoBack", "Go back"))
						]
					]
				]
			]
		];

		if (!FUnrealSync::IsDebugParameterSet())
		{
			// Checked by default.
			ArtistSyncCheckBox->ToggleCheckedState();
		}

		if (FUnrealSync::IsDebugParameterSet())
		{
			MainBox->InsertSlot(2).AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("OverrideSyncStepSpec", "Override sync step specification"))
					]
					+ SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox).OnTextCommitted(this, &SMainTabWidget::OnOverrideSyncStepText)
					]
				];
		}

		OnDataReset = FOnDataReset::CreateRaw(this, &SMainTabWidget::DataReset);
		OnDataLoaded = FOnDataLoaded::CreateRaw(this, &SMainTabWidget::DataLoaded);

		RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateThreadSafeSP(&ExternalThreadsDispatcher.Get(), &FExternalThreadsDispatcher::ExecuteRequests));

		OnReloadLabels();

		TSharedPtr<FJsonValue> State;
		if (FSettingsCache::Get().GetSetting(State, TEXT("GUI")))
		{
			LoadStateFromJSON(*State.Get());
		}
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * JSON serializer for SMainTabWidget.
	 */
	template <bool bIsConst>
	struct FJsonSerializer
	{
		/* Const-correct parameter type depending on template parameter. */
		typedef typename FSettingsCache::TAddConstIf<bIsConst, SMainTabWidget&>::Result ConstCorrectType;

		/**
		 * Serializes SMainTabWidget object for JSON using given TSerializationActionPolicy.
		 *
		 * @param Action Action object to perform serialization on.
		 * @param Object Object to serialize.
		 */
		template <class TSerializationActionPolicy>
		static void Serialize(TSerializationActionPolicy& Action, ConstCorrectType Object)
		{
			Action.Serialize("PickedGame", Object.PickGameWidget);
			Action.Serialize("IsArtist", Object.ArtistSyncCheckBox);
			Action.Serialize("IsPreview", Object.PreviewSyncCheckBox);
			Action.Serialize("IsAutoClobber", Object.AutoClobberSyncCheckBox);
			Action.Serialize("ShouldDeleteStaleBinariesOnSuccessfulSync", Object.DeleteStaleBinariesOnSuccessfulSync);
			Action.Serialize("ShouldRunUE4AfterSync", Object.RunUE4AfterSyncCheckBox);
			Action.Serialize("RadioSelection", Object.RadioSelection);
			Action.Serialize("LatestPromoted", Object.LatestPromoted);
			Action.Serialize("ChosenPromoted", Object.ChosenPromoted);
			Action.Serialize("ChosenPromotable", Object.ChosenPromotable);
			Action.Serialize("ChosenLabel", Object.ChosenLabel);
		}
	};

	/**
	 * Converts current object state to JSON value.
	 *
	 * @returns JSON value object with current state.
	 */
	TSharedRef<FJsonValue> GetStateAsJSON() const
	{
		TSharedPtr<FJsonValue> PrevGUIValuePtr;
		TSharedPtr<FJsonObject> PrevStatePtr;

		if (FSettingsCache::Get().GetSetting(PrevGUIValuePtr, TEXT("GUI"))
			&& PrevGUIValuePtr->Type == EJson::Object)
		{
			PrevStatePtr = PrevGUIValuePtr->AsObject();
		}
		else
		{
			PrevStatePtr = TSharedPtr<FJsonObject>(new FJsonObject());
		}

		TSharedRef<FJsonObject> PrevState = PrevStatePtr.ToSharedRef();

		FSettingsCache::SerializeToJSON<FJsonSerializer>(PrevState, *this);
		
		return MakeShareable(new FJsonValueObject(PrevStatePtr));
	}

	/**
	 * Loads state from JSON value object.
	 *
	 * @param Value JSON value object.
	 */
	void LoadStateFromJSON(const FJsonValue& State)
	{
		if (State.Type == EJson::Object)
		{
			FSettingsCache::SerializeFromJSON<FJsonSerializer>(State.AsObject().ToSharedRef(), *this);
		}
	}

	/**
	 * Tells that labels names are currently being loaded.
	 *
	 * @returns True if labels names are currently being loaded. False otherwise.
	 */
	bool IsLoadingInProgress() const
	{
		return LoaderThread.IsValid() && LoaderThread->IsInProgress();
	}

	/**
	 * Start async loading of the P4 label data in case user wants it.
	 */
	void StartLoadingData()
	{
		P4Data->Reset();
		LoaderThread.Reset();

		OnDataReset.ExecuteIfBound();

		LoaderThread = MakeShareable(new FP4DataLoader(OnDataLoaded, P4Data.Get()));
	}
	
	/**
	 * Terminates background P4 data loading process.
	 */
	void TerminateLoadingProcess()
	{
		if (LoaderThread.IsValid())
		{
			LoaderThread->Terminate();
		}
	}

	/**
	 * Destructor.
	 */
	virtual ~SMainTabWidget()
	{
		TerminateLoadingProcess();
		TerminateSyncingProcess();
	}

private:
	/**
	 * Gets static shared ptr to the object of this class.
	 */
	static TSharedPtr<SMainTabWidget>& GetSingletonPtr()
	{
		static TSharedPtr<SMainTabWidget> Ptr = nullptr;
		return Ptr;
	}

	/**
	 * Terminates P4 syncing process.
	 */
	void TerminateSyncingProcess()
	{
		if (SyncingThread.IsValid())
		{
			SyncingThread->Terminate();
		}
	}

	/**
	 * Launches P4 sync command with given command line and options.
	 *
	 * @param Settings Sync settings.
	 * @param LabelNameProvider Object that will provide label name to syncing thread.
	 * @param OnSyncFinished Delegate to run when syncing is finished.
	 * @param OnSyncProgress Delegate to run when syncing has made progress.
	 */
	void LaunchSync(FSyncSettings Settings, ILabelNameProvider& LabelNameProvider, const FSyncingThread::FOnSyncFinished& OnSyncFinished, const FSyncingThread::FOnSyncProgress& OnSyncProgress)
	{
		SyncingThread = MakeShareable(new FSyncingThread(MoveTemp(Settings), LabelNameProvider, OnSyncFinished, OnSyncProgress));
	}

	/**
	 * Queues adding lines to the log for execution on Slate rendering thread.
	 */
	void ExternalThreadAddLinesToLog(TSharedPtr<TArray<TSharedPtr<FString> > > Lines)
	{
		ExternalThreadsDispatcher->AddRenderingThreadRequest(FExternalThreadsDispatcher::FRenderingThreadRequest::CreateRaw(this, &SMainTabWidget::AddLinesToLog, Lines));
	}

	/**
	 * Adds lines to the log.
	 *
	 * @param Lines Lines to add.
	 */
	void AddLinesToLog(TSharedPtr<TArray<TSharedPtr<FString> > > Lines)
	{
		for (const auto& Line : *Lines)
		{
			Log->AddMessage(*Line.Get());
		}
	}

	/**
	 * Generates list view item from FString.
	 *
	 * @param Value Value to convert.
	 * @param OwnerTable List view that will contain this item.
	 *
	 * @returns Converted STableRow object.
	 */
	TSharedRef<ITableRow> GenerateLogItem(TSharedPtr<FString> Value, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
			[
				SNew(STextBlock).Text(FText::FromString(*Value))
			];
	}

	/**
	 * Gets current sync command line provider.
	 *
	 * @returns Current sync cmd line provider.
	 */
	ILabelNameProvider& GetCurrentSyncCmdLineProvider()
	{
		return *SyncCommandLineProviders[RadioSelection->GetChosen()];
	}

	/**
	 * Gets current sync command line provider.
	 *
	 * @returns Current sync cmd line provider.
	 */
	const ILabelNameProvider& GetCurrentSyncCmdLineProvider() const
	{
		return *SyncCommandLineProviders[RadioSelection->GetChosen()];
	}

	/**
	 * Saves the log to the user-pointed location.
	 */
	void SaveLog()
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

		if (DesktopPlatform != NULL)
		{
			TArray<FString> Filenames;

			TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
			void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

			if (DesktopPlatform->SaveFileDialog(
				ParentWindowHandle,
				LOCTEXT("SaveLogDialogTitle", "Save Log As...").ToString(),
				FPaths::Combine(*FPaths::EngineSavedDir(), TEXT("Logs")),
				TEXT("Syncing.log"),
				TEXT("Log Files (*.log)|*.log"),
				EFileDialogFlags::None,
				Filenames))
			{
				if (Filenames.Num() > 0)
				{
					FString Filename = Filenames[0];

					// add a file extension if none was provided
					if (FPaths::GetExtension(Filename).IsEmpty())
					{
						Filename += Filename + TEXT(".log");
					}

					// save file
					FArchive* LogFile = IFileManager::Get().CreateFileWriter(*Filename);

					if (LogFile != nullptr)
					{
						FString LogText = Log->GetPlainText().ToString() + LINE_TERMINATOR;
						LogFile->Serialize(TCHAR_TO_ANSI(*LogText), LogText.Len());
						LogFile->Close();

						delete LogFile;
					}
					else
					{
						FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveLogDialogFileError", "Failed to open the specified file for saving!"));
					}
				}
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SaveLogDialogUnsupportedError", "Saving is not supported on this platform!"));
		}
	}

	/**
	 * Performs a procedure that needs to be done when GoBack button is clicked.
	 * It tells the app to go back to the beginning.
	 */
	void GoBack()
	{
		Switcher->SetActiveWidgetIndex(0);
		Log->Clear();
		OnReloadLabels();
	}

	/**
	 * Function that is called on when go back button is clicked.
	 * It tells the app to go back to the beginning.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnGoBackButtonClick()
	{
		GoBack();

		return FReply::Handled();
	}

	/**
	 * Function that is called on when save log button is clicked.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnSaveLogButtonClick()
	{
		SaveLog();

		return FReply::Handled();
	}

	/**
	 * Function that is called on when cancel button is clicked.
	 * It tells the app to cancel the sync and go back to the beginning.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnCancelButtonClick()
	{
		TerminateLoadingProcess();
		TerminateSyncingProcess();

		ExternalThreadsDispatcher->AddRenderingThreadRequest(FExternalThreadsDispatcher::FRenderingThreadRequest::CreateRaw(this, &SMainTabWidget::GoBack));

		return FReply::Handled();
	}

	/**
	 * Tells Sync button if it should be enabled. It depends on the combination
	 * of user choices and if P4 data is currently ready.
	 *
	 * @returns True if Sync button should be enabled. False otherwise.
	 */
	bool IsSyncReady() const
	{
		return GetCurrentSyncCmdLineProvider().IsReadyForSync();
	}

	/**
	 * Tells reload labels button if it should be enabled.
	 *
	 * @returns True if reload labels button should be enabled. False otherwise.
	 */
	bool IsReloadLabelsReady() const
	{
		return !IsLoadingInProgress();
	}

	/**
	 * Launches reload labels background process.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnReloadLabels()
	{
		StartLoadingData();

		return FReply::Handled();
	}

	/**
	 * Executes UE4Editor.exe for current branch.
	 */
	FReply RunUE4()
	{
		auto ProcessPath = FPaths::Combine(*FPaths::GetPath(FPlatformProcess::GetApplicationName(FPlatformProcess::GetCurrentProcessId())), TEXT("UE4Editor"));
	  
#if PLATFORM_WINDOWS
		auto* ProcessPathPostfix = TEXT(".exe");
#elif PLATFORM_MAC
		auto* ProcessPathPostfix = TEXT(".app");
#elif PLATFORM_LINUX
		auto* ProcessPathPostfix = TEXT("");
#endif

		RunProcess(ProcessPath + ProcessPathPostfix);

		return FReply::Handled();
	}

	/**
	 * Fires up P4V for current branch.
	 */
	FReply RunP4V()
	{
		FP4Env& Env = FP4Env::Get();
		RunProcess(Env.GetP4VPath(), FString::Printf(TEXT("-p %s -u %s -c %s"), *Env.GetPort(), *Env.GetUser(), *Env.GetClient()));
		return FReply::Handled();
	}

	/**
	 * Checks if P4V exists for current P4 environment.
	 *
	 * @returns True if P4V exists, false otherwise.
	 */
	bool CheckIfP4VExists()
	{
#if PLATFORM_MAC
		return FPaths::DirectoryExists(FP4Env::Get().GetP4VPath());
#else
		return FPaths::FileExists(FP4Env::Get().GetP4VPath());
#endif
	}

	/**
	 * Sync button click function. Gets all widget data, constructs command
	 * line and launches syncing.
	 *
	 * @returns Tells that this event was handled.
	 */
	FReply OnSync()
	{
		if (CheckIfEditorIsRunning())
		{
			return FReply::Handled();
		}

		FSyncSettings Settings(
			ArtistSyncCheckBox->IsChecked(),
			PreviewSyncCheckBox->IsChecked(),
			AutoClobberSyncCheckBox->IsChecked(),
			DeleteStaleBinariesOnSuccessfulSync->IsChecked(),
			OverrideSyncStep);

		Switcher->SetActiveWidgetIndex(1);
		LaunchSync(MoveTemp(Settings), GetCurrentSyncCmdLineProvider(),
			FSyncingThread::FOnSyncFinished::CreateRaw(this, &SMainTabWidget::SyncingFinished),
			FSyncingThread::FOnSyncProgress::CreateRaw(this, &SMainTabWidget::SyncingProgress));

		GoBackButton->SetEnabled(false);
		CancelButton->SetEnabled(true);
		Throbber->SetAnimate(SThrobber::EAnimation::VerticalAndOpacity);

		return FReply::Handled();
	}

	/**
	 * Function that is called whenever monitoring thread will update its log.
	 *
	 * @param InLog Log of the operation up to this moment.
	 *
	 * @returns True if process should continue, false otherwise.
	 */
	bool SyncingProgress(const FString& InLog)
	{
		if (InLog.IsEmpty())
		{
			return true;
		}

		static FString Buffer;

		Buffer += InLog.Replace(TEXT("\r"), TEXT(""));

		FString Line;
		FString Rest;

		TSharedPtr<TArray<TSharedPtr<FString> > > Lines = MakeShareable(new TArray<TSharedPtr<FString> >());

		while (Buffer.Split("\n", &Line, &Rest))
		{
			Lines->Add(MakeShareable(new FString(Line)));

			Buffer = Rest;
		}

		ExternalThreadAddLinesToLog(Lines);

		return true;
	}

	/**
	 * Function that is called when P4 syncing is finished.
	 *
	 * @param bSuccess True if syncing finished. Otherwise false.
	 */
	void SyncingFinished(bool bSuccess)
	{
		Throbber->SetAnimate(SThrobber::EAnimation::None);
		CancelButton->SetEnabled(false);
		GoBackButton->SetEnabled(true);

		if (bSuccess && RunUE4AfterSyncCheckBox->IsChecked())
		{
			RunUE4();
		}
	}

	/**
	 * Function to call when current game changed. Is refreshing
	 * label lists in the widget.
	 *
	 * @param CurrentGameName Game name chosen.
	 */
	void OnCurrentGameChanged(const FString& CurrentGameName)
	{
		if (!P4Data->HasValidData())
		{
			return;
		}

		for (auto SyncCommandLineProvider : SyncCommandLineProviders)
		{
			SyncCommandLineProvider->RefreshData(
				CurrentGameName.Equals(FUnrealSync::GetSharedPromotableDisplayName())
				? ""
				: CurrentGameName);
		}
	}

	/**
	 * Function to call when P4 cached data is reset.
	 */
	void DataReset()
	{
		for (auto SyncCommandLineProvider : SyncCommandLineProviders)
		{
			SyncCommandLineProvider->ResetData(
				PickGameWidget->GetCurrentGame().Equals(FUnrealSync::GetSharedPromotableDisplayName())
				? ""
				: PickGameWidget->GetCurrentGame());
		}
	}

	/**
	 * Function to call when P4 cached data is loaded.
	 */
	void DataLoaded()
	{
		const FString& GameName = PickGameWidget->GetCurrentGame();

		for (auto SyncCommandLineProvider : SyncCommandLineProviders)
		{
			SyncCommandLineProvider->RefreshData(
				GameName.Equals(FUnrealSync::GetSharedPromotableDisplayName())
				? ""
				: GameName);
		}
	}

	/**
	 * Happens when someone edits the override sync step text box.
	 */
	void OnOverrideSyncStepText(const FText& CommittedText, ETextCommit::Type Type);

	/**
	 * Method to add command line provider/widget to correct arrays.
	 *
	 * @param Name Name of the radio selection widget item.
	 * @param Widget Widget to add.
	 */
	template <typename TWidgetType>
	void AddToRadioSelection(FText Name, TSharedRef<TWidgetType> Widget)
	{
		RadioSelection->Add(Name, Widget);
		SyncCommandLineProviders.Add(Widget);
	}

	/**
	 * Checks if the editor is currently running on the system and asks user to
	 * close it.
	 *
	 * @returns False if editor is not opened or user forced to continue.
	 *          Otherwise true.
	 */
	bool CheckIfEditorIsRunning()
	{
		auto Response = SEditorRunningDlg::EResponse::Retry;
		bool bEditorIsRunning = false;
			 
		while (Response == SEditorRunningDlg::EResponse::Retry && ((bEditorIsRunning = CheckForEditorProcess()) == true))
		{
			Response = SEditorRunningDlg::ShowAndGetResponse();
		}

		return bEditorIsRunning && Response != SEditorRunningDlg::EResponse::ForceSync;
	}

	/**
	 * Checks if the editor is currently running on the system.
	 *
	 * @returns False if editor is not opened. Otherwise true.
	 */
	bool CheckForEditorProcess()
	{
		TArray<FString> PossibleNormalizedPaths;
		for (const auto& PossibleExecutablePath : GetPossibleExecutablePaths())
		{
#if PLATFORM_WINDOWS
			auto PossibleProcImagePath = PossibleExecutablePath.ToUpper();
#else
			auto PossibleProcImagePath = PossibleExecutablePath;
#endif
			FPaths::NormalizeFilename(PossibleProcImagePath);

			PossibleNormalizedPaths.Add(MoveTemp(PossibleProcImagePath));
		}

		FPlatformProcess::FProcEnumerator ProcessEnumerator;

		while (ProcessEnumerator.MoveNext())
		{
			auto ProcInfo = ProcessEnumerator.GetCurrent();

#if PLATFORM_WINDOWS
			FString ProcFullImagePath = ProcInfo.GetFullPath().ToUpper();
#elif PLATFORM_MAC
			FString ProcFullImagePath = FPaths::GetPath(FPaths::GetPath(FPaths::GetPath(ProcInfo.GetFullPath())));
#elif PLATFORM_LINUX
			FString ProcFullImagePath = ProcInfo.GetFullPath();
#endif

			if (ProcFullImagePath.IsEmpty())
			{
				continue;
			}

			FPaths::NormalizeFilename(ProcFullImagePath);

			for (auto PossibleNormalizedPath : PossibleNormalizedPaths)
			{
				if (PossibleNormalizedPath.Equals(ProcFullImagePath))
				{
					return true;
				}
			}
		}

		return false;
	}

	/**
	 * Returns possible colliding UE4Editor executables.
	 *
	 * @returns Array of possible executables.
	 */
	static TArray<FString> GetPossibleExecutablePaths()
	{
		TArray<FString> Out;

		FString BasePath = FPaths::GetPath(FPlatformProcess::GetApplicationName(FPlatformProcess::GetCurrentProcessId()));

#if PLATFORM_WINDOWS
		Out.Add(FPaths::Combine(*BasePath, TEXT("UE4Editor.exe")));
		Out.Add(FPaths::Combine(*BasePath, TEXT("UE4Editor-Win32-Debug.exe")));
		Out.Add(FPaths::Combine(*BasePath, TEXT("UE4Editor-Win64-Debug.exe")));
#elif PLATFORM_MAC
		BasePath = FPaths::GetPath(FPaths::GetPath(FPaths::GetPath(BasePath)));

		Out.Add(FPaths::Combine(*BasePath, TEXT("UE4Editor.app")));
		Out.Add(FPaths::Combine(*BasePath, TEXT("UE4Editor-Mac-Debug.app")));
#elif PLATFORM_LINUX
		Out.Add(FPaths::Combine(*BasePath, TEXT("UE4Editor")));
		Out.Add(FPaths::Combine(*BasePath, TEXT("UE4Editor-Linux-Debug")));
#else
		static_assert(false, "Not implemented yet for this platform. Please fix it.");
#endif

		return Out;
	}

	/* Main widget switcher. */
	TSharedPtr<SWidgetSwitcher> Switcher;

	/* Radio selection used to chose method to sync. */
	TSharedPtr<SRadioContentSelection> RadioSelection;

	/* Latest promoted widget. */
	TSharedPtr<SLatestPromoted> LatestPromoted;
	/* Chosen promoted widget. */
	TSharedPtr<SPickPromoted> ChosenPromoted;
	/* Chosen promotable widget. */
	TSharedPtr<SPickPromotable> ChosenPromotable;
	/* Chosen label widget. */
	TSharedPtr<SPickAny> ChosenLabel;

	/* Widget to pick game used to sync. */
	TSharedPtr<SPickGameWidget> PickGameWidget;
	/* Check box to tell if this should be an artist sync. */
	TSharedPtr<SPreservableCheckBox> ArtistSyncCheckBox;
	/* Check box to tell if this should be a preview sync. */
	TSharedPtr<SPreservableCheckBox> PreviewSyncCheckBox;
	/* Check box to tell if this should be a auto-clobber sync. */
	TSharedPtr<SPreservableCheckBox> AutoClobberSyncCheckBox;
	/* Check box to tell if UnrealSync should run UE4 after sync. */
	TSharedPtr<SPreservableCheckBox> RunUE4AfterSyncCheckBox;
	/* Check box to tell if UnrealSync should delete stale binaries on successful sync. */
	TSharedPtr<SPreservableCheckBox> DeleteStaleBinariesOnSuccessfulSync;

	/* External thread requests dispatcher. */
	TSharedRef<FExternalThreadsDispatcher, ESPMode::ThreadSafe> ExternalThreadsDispatcher;

	/* Syncing log. */
	TSharedPtr<SSyncLog> Log;

	/* Go back button reference. */
	TSharedPtr<SButton> GoBackButton;
	/* Cancel button reference. */
	TSharedPtr<SButton> CancelButton;
	/* Throbber to indicate work-in-progress. */
	TSharedPtr<SThrobber> Throbber;

	/* Array of sync command line providers. */
	TArray<TSharedRef<ILabelNameProvider> > SyncCommandLineProviders;

	/* Override sync step value. */
	FString OverrideSyncStep;

	/* Data loaded event. */
	FOnDataLoaded OnDataLoaded;

	/* Data reset event. */
	FOnDataReset OnDataReset;

	/* Reference to P4 data proxy. */
	TSharedRef<FP4DataProxy> P4Data;

	/* Background loading process monitoring thread. */
	TSharedPtr<FP4DataLoader> LoaderThread;

	/* Background syncing process monitoring thread. */
	TSharedPtr<FSyncingThread> SyncingThread;
};

void SMainTabWidget::OnOverrideSyncStepText(const FText& CommittedText, ETextCommit::Type Type)
{
	OverrideSyncStep = CommittedText.ToString();
}

/**
 * Creates and returns main tab.
 *
 * @param Args Args used to spawn tab.
 *
 * @returns Main UnrealSync tab.
 */
TSharedRef<SDockTab> GetMainTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> MainTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(FText::FromString("Syncing"))
		.ToolTipText(FText::FromString("Sync Unreal Engine tool."));

	MainTab->SetContent(
		SMainTabWidget::CreateInstance()
	);

	FGlobalTabmanager::Get()->SetActiveTab(MainTab);

	return MainTab;
}

/**
 * Creates and returns P4 settings tab.
 *
 * @param Args Args used to spawn tab.
 *
 * @returns P4 settings UnrealSync tab.
 */
TSharedRef<SDockTab> GetP4EnvTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> P4EnvTab =
		SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(FText::FromString("Perforce Settings"))
		.ToolTipText(FText::FromString("Perforce settings."));

	P4EnvTab->SetContent(
		SNew(SP4EnvTabWidget)
		);

	return P4EnvTab;
}

void InitGUI(const TCHAR* CommandLine, bool bP4EnvTabOnly)
{
	// crank up a normal Slate application using the platform's standalone renderer
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// set the application name
	FGlobalTabmanager::Get()->SetApplicationTitle(NSLOCTEXT("UnrealSync", "AppTitle", "Unreal Sync"));

	auto TabStack = FTabManager::NewStack();

	if (!bP4EnvTabOnly)
	{
		TabStack->AddTab("MainTab", ETabState::OpenedTab);
		FGlobalTabmanager::Get()->RegisterTabSpawner("MainTab", FOnSpawnTab::CreateStatic(&GetMainTab));
		TabStack->SetForegroundTab(FTabId("MainTab"));
	}

	FGlobalTabmanager::Get()->RegisterTabSpawner("P4EnvTab", FOnSpawnTab::CreateStatic(&GetP4EnvTab));
	TabStack->AddTab("P4EnvTab", ETabState::OpenedTab);

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("UnrealSyncLayout")
		->AddArea(
			FTabManager::NewArea(720, 370)
			->SetWindow(FVector2D(720, 370), false)
			->Split(TabStack)
		);

	FGlobalTabmanager::Get()->RestoreFrom(Layout, TSharedPtr<SWindow>());

	// enter main loop
	double DeltaTime = 0.0;
	double LastTime = FPlatformTime::Seconds();
	const float IdealFrameTime = 1.0f / 60;

	while (!GIsRequestingExit)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();

		// throttle frame rate
		FPlatformProcess::Sleep(FMath::Max<float>(0.0f, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

		double CurrentTime = FPlatformTime::Seconds();
		DeltaTime = CurrentTime - LastTime;
		LastTime = CurrentTime;

		FStats::AdvanceFrame(false);

		GLog->FlushThreadedLogs();
	}

	FUnrealSync::SaveSettingsAndClose();

	FSlateApplication::Shutdown();
}

void SaveGUISettings()
{
	TSharedPtr<SMainTabWidget> Ptr = SMainTabWidget::GetInstance();
	if (!Ptr.IsValid())
	{
		return;
	}

	FSettingsCache::Get().SetSetting(TEXT("GUI"), Ptr->GetStateAsJSON());
}

#undef LOCTEXT_NAMESPACE
