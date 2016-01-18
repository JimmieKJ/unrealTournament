// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SAddFriendMenu.h"
#include "FriendContainerViewModel.h"
#include "FriendsFontStyleService.h"

#define LOCTEXT_NAMESPACE ""

class SAddFriendMenuImpl : public SAddFriendMenu
{
public:

	void Construct(const FArguments& InArgs, const TSharedRef<FFriendContainerViewModel>& InFriendsViewModel, const TSharedRef<FFriendsFontStyleService> InFontService)
	{
		FontService = InFontService;
		FriendStyle = *InArgs._FriendStyle;
		FriendsViewModel = InFriendsViewModel;
		Opacity = InArgs._Opacity;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.FriendsListStyle.FriendsListBackground)
			.BorderBackgroundColor(this, &SAddFriendMenuImpl::GetBackgroundOpacityFadeColor)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					GetBackButton(InArgs)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
					.ColorAndOpacity(this, &SAddFriendMenuImpl::GetOpacityFadeColor)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.AutoWidth()
					.Padding(FriendStyle.FriendsListStyle.SubMenuSearchIconMargin)
					[
						SNew(SImage)
						.Image(&FriendStyle.FriendsListStyle.SearchBrush)
						.ColorAndOpacity(this, &SAddFriendMenuImpl::GetOpacityFadeColor)
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(FriendStyle.FriendsListStyle.SubMenuSearchTextMargin)
					[
						SAssignNew(FriendNameTextBox, SMultiLineEditableTextBox)
						.BackgroundColor(this, &SAddFriendMenuImpl::GetOpacityFadeColor)
						.ForegroundColor(this, &SAddFriendMenuImpl::GetOpacityFadeColor)
						.Style(&FriendStyle.FriendsListStyle.AddFriendEditableTextStyle)
						.ClearKeyboardFocusOnCommit(false)
						.OnTextCommitted(this, &SAddFriendMenuImpl::HandleFriendEntered)
						.HintText(LOCTEXT("AddFriendHint", "Search by Account Name or Email Address"))
						.OnTextChanged(this, &SAddFriendMenuImpl::HandleTextChanged)
						.ModiferKeyForNewLine(EModifierKey::Shift)
						.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
					.ColorAndOpacity(this, &SAddFriendMenuImpl::GetOpacityFadeColor)
				]
			]
		]);
	}

	virtual void Initialize() override
	{
		FriendNameTextBox->SetText(FText::GetEmpty());
		FSlateApplication::Get().SetKeyboardFocus(FriendNameTextBox, EFocusCause::SetDirectly);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled();
	}

private:

	DECLARE_DERIVED_EVENT(SAddFriendMenu, SAddFriendMenu::FOnCloseEvent, FOnCloseEvent);
	virtual FOnCloseEvent& OnCloseEvent() override
	{
		return OnCloseDelegate;
	}

	FSlateColor GetOpacityFadeColor() const
	{
		return FLinearColor(1, 1, 1, Opacity.Get());
	}

	FSlateColor GetBackgroundOpacityFadeColor() const
	{
		return FLinearColor(1, 1, 1, Opacity.Get() * 0.9f);
	}

	TSharedRef<SWidget> GetBackButton(const FArguments& InArgs)
	{

		TSharedPtr<SImage> BackButtonImage = SNew(SImage)
			.Image(&FriendStyle.FriendsListStyle.BackBrush);

		TSharedPtr<SImage> MenuIcon = SNew(SImage)
			.Image(&FriendStyle.FriendsListStyle.AddFriendButtonContentBrush);

		TSharedPtr<STextBlock> BackButtonText = SNew(STextBlock)
			.Text(LOCTEXT("AddFriend", "ADD A FRIEND"))
			.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont);

		// Header and back button
		TSharedPtr<SButton> Button = SNew(SButton)
			.ButtonStyle(&FriendStyle.FriendsListStyle.HeaderButtonStyle)
			.ButtonColorAndOpacity(this, &SAddFriendMenuImpl::GetOpacityFadeColor)
			.ContentPadding(FriendStyle.FriendsListStyle.SubMenuBackButtonMargin)
			.OnClicked(this, &SAddFriendMenuImpl::HandleBackButtonClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(FriendStyle.FriendsListStyle.SubMenuBackIconMargin)
				[
					BackButtonImage.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					SNew(SSeparator)
					.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
					.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
					.Orientation(EOrientation::Orient_Vertical)
					.ColorAndOpacity(this, &SAddFriendMenuImpl::GetOpacityFadeColor)
				]
				+ SHorizontalBox::Slot()
				.Padding(FriendStyle.FriendsListStyle.SubMenuPageIconMargin)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					MenuIcon.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					BackButtonText.ToSharedRef()
				]
			];

		BackButtonImage->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SAddFriendMenuImpl::GetButtonContentColor, Button)));
		MenuIcon->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SAddFriendMenuImpl::GetButtonContentColor, Button)));
		BackButtonText->SetColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(this, &SAddFriendMenuImpl::GetButtonContentColor, Button)));

		return Button.ToSharedRef();
	}

	FSlateColor GetButtonContentColor(TSharedPtr<SButton> Button)
	{
		FLinearColor ButtonColor = FriendStyle.FriendsListStyle.ButtonContentColor.GetSpecifiedColor();
		if (Button.IsValid() && Button->IsHovered())
		{
			ButtonColor = FriendStyle.FriendsListStyle.ButtonHoverContentColor.GetSpecifiedColor();
		}
		ButtonColor.A = Opacity.Get();
		return ButtonColor;
	}

	void HandleFriendEntered(const FText& CommentText, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter)
		{
			FriendsViewModel->RequestFriend(CommentText);
			HandleBackButtonClicked();
		}
	}

	void HandleTextChanged(const FText& CurrentText)
	{
		FString CurrentTextString = CurrentText.ToString();
		if (CurrentTextString.Len() > 254)
		{
			FriendNameTextBox->SetText(FText::FromString(CurrentTextString.Left(254)));
		}
	}

	FReply HandleBackButtonClicked()
	{
		OnCloseDelegate.Broadcast();
		return FReply::Handled();
	}

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	FFriendsAndChatStyle FriendStyle;
	TSharedPtr<FFriendContainerViewModel> FriendsViewModel;
	FOnCloseEvent OnCloseDelegate;

	// Holds the Friends add text box
	TSharedPtr< SMultiLineEditableTextBox > FriendNameTextBox;

	TAttribute<float> Opacity;
};

TSharedRef<SAddFriendMenu> SAddFriendMenu::New()
{
	return MakeShareable(new SAddFriendMenuImpl());
}

#undef LOCTEXT_NAMESPACE
