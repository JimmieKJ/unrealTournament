// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SHeaderButtonToolTip.h"

#include "FriendsFontStyleService.h"
#include "FriendContainerViewModel.h"
#include "FriendsUserViewModel.h"

#define LOCTEXT_NAMESPACE ""

class SHeaderButtonToolTipImpl : public SHeaderButtonToolTip
{
public:


	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService> InFontService, const TSharedRef<class FFriendContainerViewModel>& InViewModel)
	{
		FontService = InFontService;
		FriendStyle = *InArgs._FriendStyle;
		ViewModel = InViewModel;

		const float AnimTimePerItem = 0.3f;
		const float AnimStartDelay = 0.25f;

		// Setup intro anim For item
		CurveHandle = CurveSequence.AddCurve(AnimStartDelay, AnimTimePerItem);


		TSharedPtr<SVerticalBox> VerticalBox = SNew(SVerticalBox);

		if (InArgs._ShowStatus)
		{
			VerticalBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(FriendStyle.FriendsListStyle.ToolTipMargin))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(FriendStyle.FriendsListStyle.TipStatusMargin)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SImage)
					.ColorAndOpacity(this, &SHeaderButtonToolTipImpl::GetContentColor)
					.Image(this, &SHeaderButtonToolTipImpl::GetStatusBrush)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Visibility(InArgs._ShowStatus ? EVisibility::Visible : EVisibility::Collapsed)
					.ColorAndOpacity(this, &SHeaderButtonToolTipImpl::GetContentColor)
					.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
					.Text(FText::FromString(ViewModel->GetUserViewModel()->GetName()))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Visibility(InArgs._ShowStatus ? EVisibility::Visible : EVisibility::Collapsed)
					.ColorAndOpacity(this, &SHeaderButtonToolTipImpl::GetDullContentColor)
					.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetSmallFont)
					.Text(FText::Format(LOCTEXT("StatusStringFormat", " - {0}"), FText::FromString(EOnlinePresenceState::ToString(ViewModel->GetUserViewModel()->GetOnlineStatus()))))
				]
			];

			VerticalBox->AddSlot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Visibility(InArgs._ShowStatus ? EVisibility::Visible : EVisibility::Collapsed)
				.SeparatorImage(&FriendStyle.FriendsListStyle.SeperatorBrush)
				.Thickness(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness)
				.ColorAndOpacity(this, &SHeaderButtonToolTipImpl::GetContentColor)
			];
		}
		
		
		VerticalBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(FriendStyle.FriendsListStyle.ToolTipMargin))
		[
			SNew(STextBlock)
			.Text(InArgs._Tip)
			.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
			.ColorAndOpacity(this, &SHeaderButtonToolTipImpl::GetContentColor)
		];

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.Image(&FriendStyle.FriendsListStyle.ToolTipArrowBrush)
				.ColorAndOpacity(this, &SHeaderButtonToolTipImpl::GetContentColor)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(FMargin(FriendStyle.FriendsListStyle.SubMenuSeperatorThickness))
				.BorderImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground)
				.BorderBackgroundColor(this, &SHeaderButtonToolTipImpl::GetContentColor)
				[
					SNew(SBorder)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.BorderImage(&FriendStyle.FriendsListStyle.FriendsListBackground)
					.BorderBackgroundColor(this, &SHeaderButtonToolTipImpl::GetContentColor)
					.Padding(0)
					[
						VerticalBox.ToSharedRef()
					]
				]
			]
		]);
	}

	virtual void PlayIntroAnim() override
	{
		CurveSequence.Play(AsShared());
	}

private:

	const FSlateBrush* GetStatusBrush() const
	{
		EOnlinePresenceState::Type PresenceType = ViewModel->GetUserViewModel()->GetOnlineStatus();

		switch (PresenceType)
		{
		case EOnlinePresenceState::Away:
		case EOnlinePresenceState::ExtendedAway:
			return &FriendStyle.FriendsListStyle.AwayBrush;
		case EOnlinePresenceState::Chat:
		case EOnlinePresenceState::DoNotDisturb:
		case EOnlinePresenceState::Online:
			return &FriendStyle.FriendsListStyle.OnlineBrush;
		case EOnlinePresenceState::Offline:
		default:
			return &FriendStyle.FriendsListStyle.OfflineBrush;
		};
	}

	FSlateColor GetContentColor() const
	{
		FLinearColor BorderColor = FLinearColor::White;
		float Lerp = CurveHandle.GetLerp();
		if (Lerp < 1.0f)
		{
			BorderColor.A = 1.0f * Lerp;
		}
		return BorderColor;
	}

	FSlateColor GetDullContentColor() const
	{
		FLinearColor BorderColor = FLinearColor::Gray;
		float Lerp = CurveHandle.GetLerp();
		if (Lerp < 1.0f)
		{
			BorderColor.A = 1.0f * Lerp;
		}
		return BorderColor;
	}

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;
	FFriendsAndChatStyle FriendStyle;
	FCurveSequence CurveSequence;
	FCurveHandle CurveHandle;
	TSharedPtr<FFriendContainerViewModel> ViewModel;
};

TSharedRef<SHeaderButtonToolTip> SHeaderButtonToolTip::New()
{
	return MakeShareable(new SHeaderButtonToolTipImpl());
}

#undef LOCTEXT_NAMESPACE
