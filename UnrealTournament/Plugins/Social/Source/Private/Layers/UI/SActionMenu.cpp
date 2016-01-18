// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SocialPrivatePCH.h"
#include "SActionMenu.h"
#include "FriendsFontStyleService.h"
#include "FriendViewModel.h"
#include "FriendContainerViewModel.h"

#define LOCTEXT_NAMESPACE ""

class SActionMenuImpl : public SActionMenu
{
public:

	const float MenuWidth = 150;

	virtual void Construct(const FArguments& InArgs, const TSharedRef<FFriendsFontStyleService> InFontService, const TSharedRef<FFriendContainerViewModel>& InContainerViewModel)
	{
		FontService = InFontService;
		FriendStyle = *InArgs._FriendStyle;
		ContainerViewModel = InContainerViewModel;

		SUserWidget::Construct(SUserWidget::FArguments()
		[
			SNew(SBorder)
			.BorderImage(&FriendStyle.FriendsListStyle.FriendsContainerBackground)
			.BorderBackgroundColor(FLinearColor(1, 1, 1, 0.95f))
			[
				SAssignNew(ActionsContainer, SVerticalBox)
			]
		]);

		GenerateActions();
	}

	virtual void SetFriendViewModel(const TSharedRef<FFriendViewModel>& InFriendViewModel) override
	{
		FriendViewModel = InFriendViewModel;

		// Clean up
		ActionsContainer->ClearChildren();
		CurveHandles.Empty();
		ActionItems.Empty();

		// Get actions for friend
		TArray<EFriendActionType::Type> Actions;
		FriendViewModel->EnumerateActions(Actions);
		for (const auto& FriendAction : Actions)
		{
			// Add Item
			ActionItems.AddItem(FriendAction, EFriendActionType::ToText(FriendAction), nullptr, FName(*EFriendActionType::ToText(FriendAction).ToString()), FriendViewModel->CanPerformAction(FriendAction));
		}

		// Generate slate
		GenerateActions();
	}

	virtual void PlayIntroAnim() override
	{
		CurveSequence.Play(AsShared());
	}

	virtual float GetMenuWidth() const override
	{
		return MenuWidth;
	}

private:

	void GenerateActions()
	{
		const float AnimTimePerItem = 0.1f;
		const float AnimStartDelay = 0.02f;
		int AnimIndex = 0;
		ActionsContainer->ClearChildren();

		for (SActionMenu::FActionData ActionData : ActionItems)
		{
			// Setup intro anim For item
			FCurveHandle CurveHandle = CurveSequence.AddCurve((AnimIndex + 1) * AnimStartDelay, AnimTimePerItem);
			CurveHandles.Add(CurveHandle);

			// Create Button
			TSharedRef<SButton> ActionButton = SNew(SButton)
			.ContentPadding(FMargin(40, 30))
			.IsEnabled(ActionData.bIsEnabled)
			.ButtonStyle(&FriendStyle.FriendsListStyle.FriendListActionButtonStyle)
			.RenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::Create(TAttribute<TOptional<FSlateRenderTransform>>::FGetter::CreateSP(this, &SActionMenuImpl::GetRenderTransform, AnimIndex)))
			.OnClicked(FOnClicked::CreateSP(this, &SActionMenuImpl::OnActionClicked, ActionData))
			.ButtonColorAndOpacity(this, &SActionMenuImpl::GetContentColor, AnimIndex)
			[
				SNew(STextBlock)
				.ColorAndOpacity(this, &SActionMenuImpl::GetContentColor, AnimIndex)
				.Font(FontService.ToSharedRef(), &FFriendsFontStyleService::GetNormalFont)
				.Text(ActionData.EntryText)
			];

			// Add button to container list
			ActionsContainer->AddSlot()
			.AutoHeight()
			[
				ActionButton
			];

			// Increment anim index for next item
			AnimIndex++;
		}
	}

	TOptional<FSlateRenderTransform> GetRenderTransform(int32 AnimIndex) const
	{
		TOptional<FSlateRenderTransform> RenderTransform;
		float Lerp = CurveHandles[AnimIndex].GetLerp();
		const float AnimOffset = 100;
		if (Lerp < 1.0f)
		{
			float YOffset = AnimOffset - (AnimOffset * Lerp);
			RenderTransform = TransformCast<FSlateRenderTransform>(FTransform2D(FVector2D(0.f, YOffset)));
		}
		return RenderTransform;
	}

	FSlateColor GetContentColor(int32 AnimIndex) const
	{
		FLinearColor BorderColor = FLinearColor::White;
		float Lerp = CurveHandles[AnimIndex].GetLerp();
		if (Lerp < 1.0f)
		{
			BorderColor.A = 1.0f * Lerp;
		}
		return BorderColor;
	}

	FReply OnActionClicked(SActionMenu::FActionData ActionData)
	{
		if (ActionData.ActionType == EFriendActionType::RemoveFriend || (ActionData.ActionType == EFriendActionType::JoinGame && FriendViewModel->IsInGameSession()))
		{
			FriendViewModel->SetPendingAction(ActionData.ActionType);

			FText Description = LOCTEXT("Sure", "Are you sure?");
			if (ActionData.ActionType == EFriendActionType::RemoveFriend)
			{
				Description = FText::Format(LOCTEXT("RemoveFriend", "Remove {0} from friends list."), FText::FromString(FriendViewModel->GetName()));
			}
			if (ActionData.ActionType == EFriendActionType::JoinGame)
			{
				Description = FText::Format(LOCTEXT("Join", "Join {0}."), FText::FromString(FriendViewModel->GetName()));
			}

			ContainerViewModel->OpenConfirmDialog(Description, FriendViewModel);
		}
		else
		{ 
			FriendViewModel->PerformAction(ActionData.ActionType);
		}
		FSlateApplication::Get().DismissMenuByWidget(AsShared());
		return FReply::Handled();
	}

	// Holds the Font Service
	TSharedPtr<FFriendsFontStyleService> FontService;

	TSharedPtr<FFriendViewModel> FriendViewModel;
	TSharedPtr<FFriendContainerViewModel> ContainerViewModel;
	TSharedPtr<SVerticalBox> ActionsContainer;
	SActionMenu::FActionsArray ActionItems;
	FFriendsAndChatStyle FriendStyle;

	FCurveSequence CurveSequence;
	TArray<FCurveHandle> CurveHandles;
};

TSharedRef<SActionMenu> SActionMenu::New()
{
	return MakeShareable(new SActionMenuImpl());
}

#undef LOCTEXT_NAMESPACE
