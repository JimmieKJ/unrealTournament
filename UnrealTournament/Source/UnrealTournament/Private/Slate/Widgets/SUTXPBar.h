// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"

#if !UE_SERVER

class SUTXPBar : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUTXPBar) {}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)
	SLATE_END_ARGS()

	/**
	* Construct the widget.
	*
	* @param InDeclaration A declaration from which to construct the widget.
	*/
	void Construct(const SUTXPBar::FArguments& InDeclaration);

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	void RestartLevelAnimation()
	{
		InterpXP = 0.0f;
		InterpTime = 0.0f;
		InterpAlpha = 0.0f;
		LevelStartTime = 0.0f;
	}

protected:

	virtual TSharedRef<SWidget> BuildBreakdownList();
	virtual TSharedRef<SWidget> BuildBreakdownWidget(TAttribute<FText> TextAttribute, FLinearColor Color);

	FText GetLevelText() const;

	FText GetBreakdownText_ScoreXP() const;
	FText GetBreakdownText_KillAwardXP() const;
	FText GetBreakdownText_OffenseXP() const;
	FText GetBreakdownText_DefenseXP() const;
	FText GetBreakdownText_ChallengeXP() const;
	FText GetGainedXPText() const;
	FText GetCurrentXPText() const;
	FText GetRemainingXPText() const;

	FMargin GetBreakdownMargin() const;
	TOptional<FSlateRenderTransform> GetLevelTransform() const;
	float GetXP() const;
	FXPBreakdown GetXPBreakdown() const;

	virtual void OnLevelUp(int32 NewLevel);

	TSharedPtr<SBox> BarBox;

	/**The value of XP animating*/
	float InterpXP;
	/**The Start time of the XP bar animation*/
	float InterpTime;
	float InterpAlpha;

	/**The time when a level is gained*/
	float LevelStartTime;

private:

	void DrawBar(float XP, int32 Level, FLinearColor Color, float& LastXP, FGeometry& BarGeo, int32& LayerId, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, uint32 DrawEffects) const;

	inline int32 ConvertToLevelXP(int32 XP, int32 Level) const
	{
		int32 LevelXPStart = GetXPForLevel(Level - 1);
		return FMath::Max(XP - LevelXPStart, 0);
	}

	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
};


#endif