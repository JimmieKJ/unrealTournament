// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SFriendItemToolTip.h"

#include "FriendsFontStyleService.h"
#include "FriendViewModel.h"

#define LOCTEXT_NAMESPACE ""

class SFriendItemToolTipImpl : public SFriendItemToolTip
{
public:


	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService> InFontService, const TSharedRef<FFriendViewModel>& InFriendViewModel)
	{
		FontService = InFontService;
		FriendStyle = *InArgs._FriendStyle;
		FriendViewModel = InFriendViewModel;

		const float AnimTimePerItem = 0.3f;
		const float AnimStartDelay = 0.25f;

		// Setup intro anim For item
		CurveHandle = CurveSequence.AddCurve(AnimStartDelay, AnimTimePerItem);

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(20, 20, 10, 20))
				.BorderImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground)
				.BorderBackgroundColor(this, &SFriendItemToolTipImpl::GetContentColor)
				[
					SNew(STextBlock)
					.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
					.ColorAndOpacity(this, &SFriendItemToolTipImpl::GetContentColor)
					.Text(FText::FromString(FriendViewModel->GetAlternateName()))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(&FriendStyle.FriendsListStyle.ActionMenuArrowBrush)
				.ColorAndOpacity(this, &SFriendItemToolTipImpl::GetContentColor)
			]
		]);
	}

	virtual void PlayIntroAnim() override
	{
		if (!FriendViewModel->GetAlternateName().IsEmpty())
		{
			CurveSequence.Play(AsShared());
		}
		else
		{
			CurveSequence.JumpToStart();
			CurveSequence.Pause();
		}
	}

private:

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

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;
	TSharedPtr<FFriendViewModel> FriendViewModel;
	FFriendsAndChatStyle FriendStyle;
	FCurveSequence CurveSequence;
	FCurveHandle CurveHandle;
};

TSharedRef<SFriendItemToolTip> SFriendItemToolTip::New()
{
	return MakeShareable(new SFriendItemToolTipImpl());
}

#undef LOCTEXT_NAMESPACE
