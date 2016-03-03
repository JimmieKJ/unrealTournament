// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "SUTXPBar.h"

const float XP_ANIM_TIME = 2.0f;

const FLinearColor XPColor(0.5f, 0.5f, 0.25f);
const FLinearColor BreakdownColor(1.0f, 0.22f, 0.0f);

#if !UE_SERVER

void SUTXPBar::Construct(const SUTXPBar::FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	bItemUnlockToastsProcessed = false;
	RestartLevelAnimation();

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		.Padding(20.0f,20.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				
				//TODO: Unique image per level
				/*+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(30.f)
				[
					SNew(SBox)
					.Content()
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UT.LevelIcon"))
					]
				]*/
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(this, &SUTXPBar::GetLevelText)
							.TextStyle(SUWindowsStyle::Get(), "UT.XPBar.LevelText")
							.ColorAndOpacity(FLinearColor::White)
							.RenderTransform(this, &SUTXPBar::GetLevelTransform)
							.RenderTransformPivot(FVector2D(0.5f,0.5f))
						]
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						+ SHorizontalBox::Slot()
						.Padding(2.f, 0.f)
						.VAlign(VAlign_Bottom)
						.AutoWidth()
						[
							BuildBreakdownWidget(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SUTXPBar::GetCurrentXPText)), XPColor)
						]
						+ SHorizontalBox::Slot()
						.Padding(2.f, 0.f)
						.VAlign(VAlign_Bottom)
						.AutoWidth()
						[
							BuildBreakdownWidget(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SUTXPBar::GetRemainingXPText)), XPColor)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(0.f,10.f)
						.HAlign(HAlign_Fill)
						[
							SAssignNew(BarBox, SBox)
							.MaxDesiredHeight(25.0f)
							.MaxDesiredWidth(400.0f)
							.MinDesiredHeight(25.0f)
							.MinDesiredWidth(400.0f)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						BuildBreakdownList()
					]
				]
			]
		];
}

TSharedRef<SWidget> SUTXPBar::BuildBreakdownList()
{
	FXPBreakdown XPBreakdown = GetXPBreakdown();

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0.f, 2.f, 32.f, 2.f)
		.AutoWidth()
		[
			BuildBreakdownWidget(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SUTXPBar::GetGainedXPText)), XPColor)
		]
		+SHorizontalBox::Slot()
		.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownMargin)))
		.AutoWidth()
		[
			BuildBreakdownWidget(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownText_ScoreXP)), XPColor)
		]
		+SHorizontalBox::Slot()
		.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownMargin)))
		.AutoWidth()
		[
			BuildBreakdownWidget(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownText_KillAwardXP)), XPColor)
		]
		+SHorizontalBox::Slot()
		.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownMargin)))
		.AutoWidth()
		[
			BuildBreakdownWidget(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownText_OffenseXP)), XPColor)
		]
		+SHorizontalBox::Slot()
		.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownMargin)))
		.AutoWidth()
		[
			BuildBreakdownWidget(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownText_DefenseXP)), XPColor)
		]
		+SHorizontalBox::Slot()
		.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownMargin)))
		.AutoWidth()
		[
			BuildBreakdownWidget(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SUTXPBar::GetBreakdownText_ChallengeXP)), XPColor)
		];
}

TSharedRef<SWidget> SUTXPBar::BuildBreakdownWidget(TAttribute<FText> TextAttribute, FLinearColor Color)
{
	return SNew(SBorder)
		.BorderImage(SUWindowsStyle::Get().GetBrush("UT.XPBar.XPBreakdown.BG"))
		.BorderBackgroundColor(Color)
		.Padding(FMargin(0.f, 1.f))
		.Content()
		[
			SNew(STextBlock)
			.Text(TextAttribute)
			.TextStyle(SUWindowsStyle::Get(), "UT.XPBar.XPBreakdown.Text")
			.ColorAndOpacity(FLinearColor::White)
		];
}


FText SUTXPBar::GetLevelText() const
{
	int32 CurrentXP = FMath::Max(GetXP() - GetXPBreakdown().Total(), 0.f) + InterpXP;
	int32 Level = GetLevelForXP(CurrentXP);
	return FText::Format(NSLOCTEXT("SUTXPBar", "LevelNum", " Level {0} "), FText::AsNumber(Level));
}

FText SUTXPBar::GetBreakdownText_ScoreXP() const
{
	FXPBreakdown XPBreakdown = GetXPBreakdown();
	if (XPBreakdown.ScoreXP > 0)
	{
		return FText::Format(NSLOCTEXT("SUTXPBar", "ScoreXP", " +{0} Score XP "), FText::AsNumber(XPBreakdown.ScoreXP));
	}
	return FText::GetEmpty();
}

FText SUTXPBar::GetBreakdownText_KillAwardXP() const
{
	FXPBreakdown XPBreakdown = GetXPBreakdown();
	if (XPBreakdown.KillAwardXP > 0)
	{
		return FText::Format(NSLOCTEXT("SUTXPBar", "KillXP", " +{0} Kill XP "), FText::AsNumber(XPBreakdown.KillAwardXP));
	}
	return FText::GetEmpty();
}

FText SUTXPBar::GetBreakdownText_OffenseXP() const
{
	FXPBreakdown XPBreakdown = GetXPBreakdown();
	if (XPBreakdown.OffenseXP > 0)
	{
		return FText::Format(NSLOCTEXT("SUTXPBar", "OffenseXP", " +{0} Offense XP "), FText::AsNumber(XPBreakdown.OffenseXP));
	}
	return FText::GetEmpty();
}

FText SUTXPBar::GetBreakdownText_DefenseXP() const
{
	FXPBreakdown XPBreakdown = GetXPBreakdown();
	if (XPBreakdown.DefenseXP > 0)
	{
		return FText::Format(NSLOCTEXT("SUTXPBar", "DefenseXP", " +{0} Defense XP "), FText::AsNumber(XPBreakdown.DefenseXP));
	}
	return FText::GetEmpty();
}

FText SUTXPBar::GetBreakdownText_ChallengeXP() const
{
	FXPBreakdown XPBreakdown = GetXPBreakdown();
	if (XPBreakdown.ChallengeXP > 0)
	{
		return FText::Format(NSLOCTEXT("SUTXPBar", "ChallengeXP", " +{0} Challenge Star Bonus XP "), FText::AsNumber(XPBreakdown.ChallengeXP));
	}
	return FText::GetEmpty();
}

FText SUTXPBar::GetGainedXPText() const
{
	if (InterpXP > 0.f)
	{
		return FText::Format(NSLOCTEXT("SUTXPBar", "XPGained", " +{0} XP Gained "), FText::AsNumber(int32(InterpXP)));
	}
	return FText::GetEmpty();
}

FText SUTXPBar::GetCurrentXPText() const
{
	int32 CurrentXP = FMath::Max(GetXP() - GetXPBreakdown().Total(), 0.f) + InterpXP;
	int32 NextLevelXP = GetXPForLevel(GetLevelForXP(CurrentXP) + 1) - CurrentXP;
	return FText::Format(NSLOCTEXT("SUTXPBar", "CurrentXP", " {0} XP "), (NextLevelXP > 0) ? FText::AsNumber(CurrentXP) : FText::AsNumber(GetXP()));
}

FText SUTXPBar::GetRemainingXPText() const
{
	int32 CurrentXP = FMath::Max(GetXP() - GetXPBreakdown().Total(), 0.f) + InterpXP;
	int32 NextLevelXP = GetXPForLevel(GetLevelForXP(CurrentXP) + 1) - CurrentXP;
	if (NextLevelXP > 0)
	{
		return FText::Format(NSLOCTEXT("SUTXPBar", "XPToNextLevel", " {0} XP To Next Level "), FText::AsNumber(NextLevelXP));
	}
	return FText::GetEmpty();
}

TOptional<FSlateRenderTransform> SUTXPBar::GetLevelTransform() const
{
	float Scale = 1.0f;
	if (PlayerOwner.IsValid())
	{
		Scale = 1.2 * (1 - FMath::Clamp((PlayerOwner->GetWorld()->TimeSeconds - LevelStartTime) * 2.0f, 0.0f, 1.0f)) + 1;
	}
	return TransformCast<FSlateRenderTransform>(FScale2D(Scale));
}

FMargin SUTXPBar::GetBreakdownMargin() const
{
	return FMargin(4.f + ((1-InterpAlpha) * 1000.0f), 2.f);
}

float SUTXPBar::GetXP() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->IsEarningXP())
	{
		return PlayerOwner->GetOnlineXP();
	}
	return 0.f;
}

FXPBreakdown SUTXPBar::GetXPBreakdown() const
{
	AUTPlayerController* UTPC = PlayerOwner.IsValid() ? Cast<AUTPlayerController>(PlayerOwner->PlayerController) : nullptr;
	if (UTPC != nullptr)
	{
		return UTPC->XPBreakdown;
	}
	return FXPBreakdown();
}

void SUTXPBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (PlayerOwner.IsValid() && PlayerOwner->GetWorld() != nullptr)
	{
		FXPBreakdown XPBreakdown = GetXPBreakdown();
		float OldXP = FMath::Max(GetXP() - XPBreakdown.Total(), 0.f);
		int32 OldLevel = GetLevelForXP(OldXP + InterpXP);

		//Easeout interp xp bar fillup
		InterpTime += InDeltaTime;
		InterpAlpha = FMath::Clamp(InterpTime / XP_ANIM_TIME, 0.0f, 1.0f);
		InterpAlpha = 1 - (1 - InterpAlpha) * (1 - InterpAlpha) * (1 - InterpAlpha);

		InterpXP = XPBreakdown.Total() * InterpAlpha;
		int32 NewLevel = GetLevelForXP(OldXP + InterpXP);

		//New level gained
		if (OldLevel < NewLevel)
		{
			OnLevelUp(NewLevel);
		}
	}
}

void SUTXPBar::OnLevelUp(int32 NewLevel)
{
	if (PlayerOwner.IsValid())
	{
		LevelStartTime = PlayerOwner->GetWorld()->TimeSeconds;

		AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
		if (UTPC != nullptr)
		{
			//Play the level up sound
			USoundBase* LevelUpSound = LoadObject<USoundBase>(NULL, TEXT("/Game/RestrictedAssets/Audio/Gameplay/A_Gameplay_CTF_CaptureSound02.A_Gameplay_CTF_CaptureSound02"), NULL, LOAD_NoWarn | LOAD_Quiet);
			if (LevelUpSound != nullptr)
			{
				UTPC->ClientPlaySound(LevelUpSound, 0.5f);
			}

			if (!bItemUnlockToastsProcessed)
			{
				bItemUnlockToastsProcessed = true;

				for (int32 i = 0; i < UTPC->LevelRewards.Num(); i++ )
				{
					//show the toast
					if (UTPC->LevelRewards.IsValidIndex(i) && UTPC->LevelRewards[i] != nullptr && PlayerOwner->IsEarningXP())
					{
						PlayerOwner->ShowToast(FText::Format(NSLOCTEXT("UT", "ItemReward", "You earned {0} for reaching level {1}!"), UTPC->LevelRewards[i]->DisplayName, FText::AsNumber(NewLevel)));
						UTPC->LevelRewards[NewLevel] = nullptr;
					}
				}
			}
		}
	}
}

int32 SUTXPBar::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
	const uint32 DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	FGeometry BarGeo = FindChildGeometry(AllottedGeometry, BarBox.ToSharedRef());

	const float AllottedWidth = BarGeo.GetLocalSize().X;
	const float AllottedHeight = BarGeo.GetLocalSize().Y;
	
	//Draw background manually so it stays behind everything
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(),
			SUWindowsStyle::Get().GetBrush("UT.XPBar.BG"),
			MyClippingRect,
			DrawEffects,
			FLinearColor::White
			);
	}

	//Draw Bar background
	{
		TArray<FSlateGradientStop> GradientStops;
		GradientStops.Add(FSlateGradientStop(FVector2D::ZeroVector, FLinearColor(0.02f, 0.02f, 0.02f)));
		GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize() * 0.4f, FLinearColor::Black));
		GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize() * 0.6f, FLinearColor::Black));
		GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize(), FLinearColor(0.02f, 0.02f, 0.02f)));

		FVector2D Size(AllottedWidth, AllottedHeight);
		FVector2D TopLeft = FVector2D(0.0f, 0.0f);
		FSlateDrawElement::MakeGradient(
			OutDrawElements,
			LayerId++,
			BarGeo.ToPaintGeometry(TopLeft, Size),
			GradientStops,
			Orient_Horizontal,
			MyClippingRect,
			DrawEffects);
	}

	//This assumes that the new XP has already been added to the local player
	FXPBreakdown XPBreakdown = GetXPBreakdown();
	float OldXP = FMath::Max(GetXP() - XPBreakdown.Total(), 0.f);
	float CurrentXP = OldXP + InterpXP;
	int32 Level = GetLevelForXP(CurrentXP);
	int32 NextLevelXP = GetXPForLevel(Level+1) - CurrentXP;
	if (NextLevelXP > 0)
	{
		//Draw the XP that was there before match end
		float LastXP = 0;
		DrawBar(OldXP, Level, XPColor, LastXP, BarGeo, LayerId, MyClippingRect, OutDrawElements, DrawEffects);

		//Draw the new XP
		DrawBar(CurrentXP, Level, BreakdownColor, LastXP, BarGeo, LayerId, MyClippingRect, OutDrawElements, DrawEffects);
	}
	else
	{
		//Max level, just draw a filled bar
		TArray<FSlateGradientStop> GradientStops;
		GradientStops.Add(FSlateGradientStop(FVector2D::ZeroVector, XPColor * 0.5f));
		GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize() * 0.2f, XPColor));
		GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize() * 0.8f, XPColor));
		GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize(), XPColor * 0.5f));

		FSlateDrawElement::MakeGradient(
			OutDrawElements,
			LayerId++,
			BarGeo.ToPaintGeometry(),
			GradientStops,
			Orient_Horizontal,
			MyClippingRect,
			DrawEffects
			);
	}
	
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void SUTXPBar::DrawBar(float XP, int32 Level, FLinearColor Color, float& LastXP, FGeometry& BarGeo, int32& LayerId, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, uint32 DrawEffects) const
{
	const float AllottedWidth = BarGeo.GetLocalSize().X;
	const float AllottedHeight = BarGeo.GetLocalSize().Y;

	//Get the xp for this level only
	int32 LevelXPStart = GetXPForLevel(Level);
	int32 LevelXPEnd = GetXPForLevel(Level + 1);
	int32 LevelXP = LevelXPEnd - LevelXPStart;
	int32 CurrentXP = XP - LevelXPStart;

	int32 StartXp = ConvertToLevelXP(LastXP, Level);
	int32 EndXp = ConvertToLevelXP(XP, Level);
	
	float XOffset = (float)StartXp / (float)LevelXP * AllottedWidth;
	float Width = (float)EndXp / (float)LevelXP * AllottedWidth - XOffset;

	//Make sure it is atleast 1 thickness
	if (Width < 1.0f)
	{
		Width = 1.0f;
	}

	TArray<FSlateGradientStop> GradientStops;
	GradientStops.Add(FSlateGradientStop(FVector2D::ZeroVector, Color * 0.5f));
	GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize() * 0.2f, Color));
	GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize() * 0.8f, Color));
	GradientStops.Add(FSlateGradientStop(BarGeo.GetLocalSize() , Color * 0.5f));

	FSlateDrawElement::MakeGradient(
		OutDrawElements,
		LayerId++,
		BarGeo.ToPaintGeometry(FVector2D(XOffset, 0.0f), FVector2D(Width, AllottedHeight)),
		GradientStops,
		Orient_Horizontal,
		MyClippingRect,
		DrawEffects
		);

	XOffset += Width;
	LastXP = XP;
}

#endif