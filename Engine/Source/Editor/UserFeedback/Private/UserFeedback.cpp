// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "IUserFeedbackModule.h"
#include "SlateBasics.h"
#include "SlateStyle.h"
#include "ModuleManager.h"

#include "IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "UnrealEd.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "UserFeedback"


/** Feedback mode - positive or negative */
namespace EFeedbackMode
{
	enum Type { Positive, Negative };
}

/** The feedback widget itself which contains the UI for submitting feedback */
class SUserFeedbackWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUserFeedbackWidget)
		: _Context(), _Mode(EFeedbackMode::Positive){}

		SLATE_ARGUMENT(FText, Context)
		SLATE_ARGUMENT(EFeedbackMode::Type, Mode)
		SLATE_EVENT(FSimpleDelegate, OnFeedbackSent)
		SLATE_EVENT(FSimpleDelegate, OnCloseClicked)
	SLATE_END_ARGS()

	/** Constructor */
	SUserFeedbackWidget()
		: bUserSentFeedback(false)
		, Sequence(0, 0.2f, ECurveEaseFunction::CubicIn)
	{

	}

	/** Destructor - records an analytics event if the user did not send feedback */
	~SUserFeedbackWidget()
	{
		if (!bUserSentFeedback && FEngineAnalytics::IsAvailable())
		{
			FEngineAnalytics::GetProvider().RecordEvent(FString("Editor.Feedback.Canceled"));
		}
	}

	/** Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs)
	{
		// Animate ourselves in if we're running at the target frame rate
		if (FSlateApplication::Get().IsRunningAtTargetFrameRate())
		{
			Sequence.Play( this->AsShared() );
		}
		else
		{
			Sequence.JumpToEnd();
		}

		OnFeedbackSent = InArgs._OnFeedbackSent;
		OnCloseClicked = InArgs._OnCloseClicked;
		Mode = InArgs._Mode;

		PopulateContextNames(InArgs._Context);

		const FMargin DefaultPadding(5, 5, 5, 5);
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[

				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1)
				[

					SNew(SBox)
					.Padding(FMargin(8,5,8,5))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.Padding(DefaultPadding)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &SUserFeedbackWidget::GetDescriptionText)
						]

						+SVerticalBox::Slot()
						.Padding(DefaultPadding)
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							[
								SNew(SComboButton)
								.ContentPadding(FMargin(5,2,5,2))
								.OnGetMenuContent(this, &SUserFeedbackWidget::GetComboBoxMenuContent)
								.ButtonContent()
								[
									SNew(STextBlock)
									.Text(this, &SUserFeedbackWidget::GetComboBoxText)
								]
							]

							+SHorizontalBox::Slot()
							.FillWidth(1)
							.Padding(FMargin(5,0,0,0))
							[
								SAssignNew(TextBox, SEditableTextBox)
								.Padding(2)
								.OnTextChanged(this, &SUserFeedbackWidget::OnTextChanged)
								.HintText(LOCTEXT("FeedbackTextBox_HintText", "Tell us more"))
								.ErrorReporting
								( 
									SNew( SPopupErrorText )
									.ShowInNewWindow( true )
								)
							]
						]
						+SVerticalBox::Slot()
						.Padding(DefaultPadding)
						.AutoHeight()
						.HAlign(HAlign_Right)
						[
							SNew(SUniformGridPanel)
							.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
							.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
							.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
							+SUniformGridPanel::Slot(0,0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
								.OnClicked(this, &SUserFeedbackWidget::OnFeedbackSubmitted)
								[
									SNew(SHorizontalBox)

									+SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(SImage).Image(FEditorStyle::GetBrush(Mode == EFeedbackMode::Positive ? "UserFeedback.PositiveIcon" : "UserFeedback.NegativeIcon"))
									]

									+SHorizontalBox::Slot()
									.Padding(FMargin(0,0,3,0))
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(STextBlock).Text(LOCTEXT("SubmitFeedback", "Send"))
									]
								]
							]
							+SUniformGridPanel::Slot(1,0)
							[
								SNew(SButton)
								.HAlign(HAlign_Center)
								.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
								.OnClicked(this, &SUserFeedbackWidget::OnCloseButtonClicked)
								.Text(LOCTEXT("CancelButton", "Cancel"))
							]
						]
					]
				]
			]
		];

		TextBox->SetError( FText::GetEmpty() );
	}

	/** Called when a key is pressed on the widget */
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCloseButtonClicked();
		}
		else if (InKeyEvent.GetKey() == EKeys::Enter)
		{
			return OnFeedbackSubmitted();
		}

		return FReply::Unhandled();
	}

	/** @return The feedback mode for this widget */
	EFeedbackMode::Type GetFeedbackMode() const
	{
		return Mode;
	}

private:
	
	/** Setup the array of context names */
	void PopulateContextNames(const FText& SuppliedContext)
	{
		// ------------------------------------
		// Fill the context display names array
		ContextDisplayNames.Add(LOCTEXT("LevelEditing", "Level Editing"));
		ContextDisplayNames.Add(LOCTEXT("ContentBrowser", "Content Browser"));
		ContextDisplayNames.Add(LOCTEXT("PlayInEditor", "Play In Editor"));
		ContextDisplayNames.Add(LOCTEXT("AssetCreation", "Asset Creation"));
		ContextDisplayNames.Add(LOCTEXT("DetailsPanel", "Details Panel"));
		ContextDisplayNames.Add(LOCTEXT("Tutorials", "Tutorials"));
		ContextDisplayNames.Add(LOCTEXT("Other", "Other"));

		ContextMarkers.FirstEditor = ContextDisplayNames.Num();
		ContextDisplayNames.Add(NSLOCTEXT("BlueprintEditor", "AppLabel", "Blueprint Editor"));
		ContextDisplayNames.Add(NSLOCTEXT("Matinee", "AppLabel", "Matinee"));
		ContextDisplayNames.Add(NSLOCTEXT("StaticMeshEditor", "AppLabel", "StaticMesh Editor"));
		ContextDisplayNames.Add(NSLOCTEXT("MaterialEditor", "AppLabel", "Material Editor"));
		ContextDisplayNames.Add(NSLOCTEXT("PhAT", "AppLabel", "PhAT"));
		ContextDisplayNames.Add(NSLOCTEXT("Cascade", "AppLabel", "Cascade"));
		ContextDisplayNames.Add(NSLOCTEXT("FPersona", "AppLabel", "Persona"));

		ContextMarkers.Custom = -1;

		// Try and match an existing context before adding our own
		for (auto Iter = ContextDisplayNames.CreateConstIterator(); Iter; ++Iter)
		{
			if (SuppliedContext.EqualTo(*Iter))
			{
				SelectedContextIndex = Iter.GetIndex();
				return;
			}
		}

		// Add the supplied context to the list since it doesn't already exist
		SelectedContextIndex = ContextMarkers.Custom = ContextDisplayNames.Num();
		ContextDisplayNames.Add(SuppliedContext);
	}

	/** Called when the text on the feedback form is changed by the user */
	void OnTextChanged(const FText& InLabel)
	{
		static const int32 MaxSize = 250;
		FString TextString = InLabel.ToString();
		if (TextString.Len() >= MaxSize)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("MaxSize"), MaxSize);
			TextBox->SetError(FText::Format(LOCTEXT("TooLong", "Additional feedback must be fewer than {MaxSize} characters."), Args));
			TextBox->SetText(FText::FromString(TextString.Left(MaxSize)));
		}
		else
		{
			TextBox->SetError(FText::GetEmpty());
		}
	}

	/** @return the description text for the widget */
	FText GetDescriptionText() const
	{
		static const FText ApplicationTitle = NSLOCTEXT("UnrealEditor", "ApplicationTitle", "Unreal Editor");

		FFormatNamedArguments Args;
		Args.Add(TEXT("Tool"), ApplicationTitle);
		if (Mode == EFeedbackMode::Positive)
		{
			return FText::Format(LOCTEXT("Description_Positive", "We value your feedback. What part of {Tool} do you like?"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("Description_Negative", "We value your feedback. What could we improve about {Tool}?"), Args);
		}
	}

	/** @return the combo box menu content */
	TSharedRef<SWidget> GetComboBoxMenuContent()
	{
		const bool bShouldCloseWindowAfterMenuSelection = true, bCloseSelfOnly = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, MakeShareable(new FUICommandList), TSharedPtr<FExtender>(), bCloseSelfOnly);

		int8 MenuIndex = 0;

		// --------------------------------
		// General editing contexts
		MenuBuilder.BeginSection(TEXT("UserFeedbackGeneral"), LOCTEXT("UserFeedbackCombo_General", "General:"));
		{
			for (; MenuIndex < ContextMarkers.FirstEditor; ++MenuIndex)
			{
				MenuBuilder.AddMenuEntry(ContextDisplayNames[MenuIndex], FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &SUserFeedbackWidget::SetCurrentContext, MenuIndex)));
			}
		}
		MenuBuilder.EndSection();

		// --------------------------------
		// Asset editor contexts
		MenuBuilder.BeginSection(TEXT("UserFeedbackAssets"), LOCTEXT("UserFeedbackCombo_Assets", "Asset Editors:"));
		{
			const int8 End = ContextMarkers.Custom == -1 ? ContextDisplayNames.Num() - 1 : ContextMarkers.Custom;
			for (; MenuIndex < End; ++MenuIndex)
			{
				MenuBuilder.AddMenuEntry(ContextDisplayNames[MenuIndex], FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &SUserFeedbackWidget::SetCurrentContext, MenuIndex)));
			}
		}
		MenuBuilder.EndSection();

		// -----------------------------------------------------------------
		// Add the custom context supplied by the client code, if applicable
		if (ContextMarkers.Custom != -1)
		{
			MenuBuilder.BeginSection(TEXT("UserFeedbackCurrent"), LOCTEXT("UserFeedbackCombo_Current", "Current:"));
			{
				MenuBuilder.AddMenuEntry(ContextDisplayNames[ContextMarkers.Custom], FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &SUserFeedbackWidget::SetCurrentContext, ContextMarkers.Custom)));
			}
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	/** @return the text displayed on the combo box */
	FText GetComboBoxText() const
	{
		return ContextDisplayNames[SelectedContextIndex];
	}

	/** Set the current context that the user wants to provide feedback for */
	void SetCurrentContext(const int8 Context)
	{
		SelectedContextIndex = Context;
	}

	/** Called when the user intentionally closes the window */
	FReply OnCloseButtonClicked()
	{
		OnCloseClicked.ExecuteIfBound();
		return FReply::Handled();
	}

	/** Called when the user sends some feedback */
	FReply OnFeedbackSubmitted()
	{
		// Get the current feedback text.  Ensure the user actually typed something
		FText UserFeedbackText = FText::TrimPrecedingAndTrailing( TextBox->GetText() );

		if( !UserFeedbackText.IsEmpty() )
		{
			if (FEngineAnalytics::IsAvailable())
			{
				TArray<FAnalyticsEventAttribute> Attributes;
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Positive"), Mode == EFeedbackMode::Positive));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Context"), ContextDisplayNames[SelectedContextIndex].ToString()));
				Attributes.Add(FAnalyticsEventAttribute(TEXT("Comment"), TextBox->GetText().ToString()));

				FEngineAnalytics::GetProvider().RecordEvent(FString("Editor.Feedback.Submitted"), Attributes);
			}

			FNotificationInfo Info(LOCTEXT("FeedbackSent", "Thank you for sending us your feedback"));
			Info.bUseLargeFont = false;
			Info.bUseThrobber = false;
			Info.bUseSuccessFailIcons = false;
			Info.Image = FEditorStyle::GetBrush("NoBrush");
			Info.ExpireDuration = 3.f;

			FSlateNotificationManager::Get().AddNotification(Info);

			bUserSentFeedback = true;
			OnFeedbackSent.ExecuteIfBound();
		}
		else
		{
			TextBox->SetError( LOCTEXT("FeedbackErrorMessage", "Please provide some feedback") );
		}

		return FReply::Handled();
	}

	/** true when the user sent feedback, false when the popup was closed without sending feedback */
	bool bUserSentFeedback;

	/** Pointer to the editable text box on the widget */
	TSharedPtr<SEditableTextBox> TextBox;

	/** The current feedback mode (positive/negative) */
	EFeedbackMode::Type Mode;

	/** Delegates that are called when feedback is sent and when the close button is clicked */
	FSimpleDelegate OnFeedbackSent, OnCloseClicked;

	/** Animation sequence for loading ourselves */
	FCurveSequence Sequence;

	/** Display names for the context shown in the combo box */
	TArray<FText> ContextDisplayNames;

	/** Markers which specify the indices of the start of particular groups in the display name array */
	struct
	{
		int8 FirstEditor;			// The index of the first editor context
		int8 Custom;				// The index of the context supplied by the client code (-1 if invalid)
	} ContextMarkers;

	/** The currently selected context index */
	int8 SelectedContextIndex;
};

/** A menu item in the feedback widget's menu */
class SUserFeedbackMenuItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SUserFeedbackMenuItem )
		: _MinWidth(0)
	{}
	SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ATTRIBUTE(float, MinWidth)
		SLATE_ATTRIBUTE(FText, Text)
		SLATE_ARGUMENT(FName, Icon)
		SLATE_ATTRIBUTE(EFeedbackMode::Type, Mode)
		SLATE_ATTRIBUTE(const FSlateBrush*, ArrowBrush)
	SLATE_END_ARGS()

	/** Construct this menu item */
	void Construct( const FArguments& InArgs )
	{
		MinWidth = InArgs._MinWidth;
		
		TSharedRef<SWidget> IconWidget = SNullWidget::NullWidget;
		if ( InArgs._Icon != NAME_None )
		{
			const FSlateBrush* IconBrush = FEditorStyle::GetOptionalBrush(InArgs._Icon);
			if (IconBrush->GetResourceName() != NAME_None)
			{
				IconWidget = 
					SNew( SImage )
					.Image(IconBrush);
			}
		}

		ChildSlot
		[
			SNew( SHorizontalBox )
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2, 0, 2, 0))
			[
				SNew( SBox )
				.WidthOverride(20)
				.HeightOverride(20.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					IconWidget
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.Padding(FMargin(2, 0, 6, 0))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(InArgs._Text)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2, 0, 6, 0))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SImage).Image(InArgs._ArrowBrush)
			]
		];
	}

	/** Compute the desired size for this widget */
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		const float MinWidthVal = MinWidth.Get();

		if (MinWidthVal == 0.0f)
		{
			return SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
		}
		else
		{
			FVector2D ChildSize = ChildSlot.GetWidget()->GetDesiredSize();
			return FVector2D(FMath::Max(MinWidthVal, ChildSize.X), ChildSize.Y );
		}
	}

private:
	/** The minimum width of the menu item */
	TAttribute<float> MinWidth;
};

/** Feedback widget button that is shown on tab bars */
class SUserFeedbackButtonWidget
	: public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SUserFeedbackButtonWidget)
		: _Context(){}

		SLATE_ARGUMENT(FText, Context)

	SLATE_END_ARGS()

	/** Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs)
	{
		Context = InArgs._Context;

		ChildSlot
		[
			SAssignNew(ComboButton, SComboButton)
			.ButtonStyle(FEditorStyle::Get(), "UserFeedback.Button")
			.ToolTipText(LOCTEXT("UserFeedbackToolTip", "Send Quick Feedback"))
			.HasDownArrow(false)
			.MenuPlacement(EMenuPlacement::MenuPlacement_BelowAnchor)
			.OnGetMenuContent(this, &SUserFeedbackButtonWidget::PopulateMenu)
			.ContentPadding(0)
			.ButtonContent()
			[
				SNew(SBox)
				.WidthOverride(16)
				.HeightOverride(16)
			]
		];
	}

private:

	void OpenFeedbackPopup(const EFeedbackMode::Type Mode)
	{
		auto FeedbackWidget = SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Window.Border"))
			.Padding(3)
			[
				SNew(SBox)
				.WidthOverride(520.f)
				[
					SNew(SUserFeedbackWidget)
						.Context(Context)
						.Mode(Mode)
						.OnFeedbackSent(this, &SUserFeedbackButtonWidget::ClosePopupWindow)
						.OnCloseClicked(this, &SUserFeedbackButtonWidget::ClosePopupWindow)
				]
			]
			;

		auto ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared()).ToSharedRef();

		// Center ourselves in the parent window
		auto PopupWindow = SAssignNew(PopupWindowPtr, SWindow)
			.IsPopupWindow(false)
			.SizingRule(ESizingRule::Autosized)
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.FocusWhenFirstShown(true)
			.ActivateWhenFirstShown(true)
			.Content()
			[
				FeedbackWidget
			];

		FSlateApplication::Get().AddWindowAsNativeChild(PopupWindow, ParentWindow, true );
	}


	/** Close the popup window if it still exists */
	void ClosePopupWindow()
	{
		auto PopupWindow = PopupWindowPtr.Pin();
		if (PopupWindow.IsValid())
		{
			PopupWindow->RequestDestroyWindow();
		}
	}


	/** Populate the menu */
	TSharedRef<SWidget> PopulateMenu()
	{
		const bool bShouldCloseWindowAfterMenuSelection = false;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, MakeShareable(new FUICommandList));

		MenuBuilder.AddMenuEntry(LOCTEXT("SendPositiveFeedback", "Send Positive Feedback"), FText(), FSlateIcon(FEditorStyle::GetStyleSetName(), "UserFeedback.PositiveIcon"), FUIAction(FExecuteAction::CreateSP(this, &SUserFeedbackButtonWidget::OpenFeedbackPopup, EFeedbackMode::Positive)));
		MenuBuilder.AddMenuEntry(LOCTEXT("SendNegativeFeedback", "Send Negative Feedback"), FText(), FSlateIcon(FEditorStyle::GetStyleSetName(), "UserFeedback.NegativeIcon"), FUIAction(FExecuteAction::CreateSP(this, &SUserFeedbackButtonWidget::OpenFeedbackPopup, EFeedbackMode::Negative)));
		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddMenuEntry(LOCTEXT("AskOnUDN", "Ask a question..."), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SUserFeedbackButtonWidget::VisitSupportSite)));

		return MenuBuilder.MakeWidget();
	}

	/** Visit UDN */
	void VisitSupportSite()
	{
		FString SupportWebsiteURL;
		if(FUnrealEdMisc::Get().GetURL( TEXT("AskAQuestionURL"), SupportWebsiteURL, true ))
		{
			FPlatformProcess::LaunchURL( *SupportWebsiteURL, NULL, NULL );
		}
	}

	/** The context to open the popup in (supplied by the client code) */
	FText Context;
	/** The combo button that opens up the main menu */
	TSharedPtr<SComboButton> ComboButton;

	/** Weak ptr to the popup window */
	TWeakPtr<SWindow> PopupWindowPtr;
};

/** User feedback module, loaded dynamically at startup */
class FUserFeedbackModuleImpl : public IUserFeedbackModule
{
public:

	/** Create a widget which allows the user to send positive or negative feedback about a feature */
	virtual TSharedRef<SWidget> CreateFeedbackWidget(FText Context) const override
	{
		if (FEngineAnalytics::IsAvailable() )
		{
			return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.Padding(0.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SUserFeedbackButtonWidget).Context(Context)
				];
		}	
		else
		{
			return SNullWidget::NullWidget;
		}
	}
};

IMPLEMENT_MODULE( FUserFeedbackModuleImpl, UserFeedback )

#undef LOCTEXT_NAMESPACE
