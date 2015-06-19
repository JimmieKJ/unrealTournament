// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "ReimportFeedbackContext.h"
#include "NotificationManager.h"
#include "SNotificationList.h"
#include "INotificationWidget.h"
#include "MessageLogModule.h"
#include "SHyperlink.h"

class SWidgetStack : public SVerticalBox
{
	SLATE_BEGIN_ARGS(SWidgetStack){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, int32 InMaxNumVisible)
	{
		MaxNumVisible = InMaxNumVisible;
		SlideCurve = FCurveSequence(0.f, .5f, ECurveEaseFunction::QuadOut);
		SizeCurve = FCurveSequence(0.f, .5f, ECurveEaseFunction::QuadOut);

		StartSlideOffset = 0;
		StartSizeOffset = FVector2D(ForceInitToZero);
	}

	FVector2D ComputeTotalSize() const
	{
		FVector2D Size(ForceInitToZero);
		for (int32 Index = 0; Index < FMath::Min(NumSlots(), MaxNumVisible); ++Index)
		{
			const FVector2D& ChildSize = Children[Index].GetWidget()->GetDesiredSize();
			if (ChildSize.X > Size.X)
			{
				Size.X = ChildSize.X;
			}
			Size.Y += ChildSize.Y + Children[Index].SlotPadding.Get().GetTotalSpaceAlong<Orient_Vertical>();
		}
		return Size;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		const float Lerp = SizeCurve.GetLerp();
		FVector2D DesiredSize = ComputeTotalSize() * Lerp + StartSizeOffset * (1.f-Lerp);
		DesiredSize.X = FMath::Min(500.f, DesiredSize.X);

		return DesiredSize;
	}

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override
	{
		if (Children.Num() == 0)
		{
			return;
		}

		const float Alpha = 1.f - SlideCurve.GetLerp();
		float PositionSoFar = AllottedGeometry.GetLocalSize().Y + StartSlideOffset*Alpha;

		for (int32 Index = 0; Index < NumSlots(); ++Index)
		{
			const SBoxPanel::FSlot& CurChild = Children[Index];
			const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();

			if (ChildVisibility != EVisibility::Collapsed)
			{
				const FVector2D ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();

				const FMargin SlotPadding(CurChild.SlotPadding.Get());
				const FVector2D SlotSize(AllottedGeometry.Size.X, ChildDesiredSize.Y + SlotPadding.GetTotalSpaceAlong<Orient_Vertical>());

				const AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>( SlotSize.X, CurChild, SlotPadding );
				const AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>( SlotSize.Y, CurChild, SlotPadding );

				ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild(
					CurChild.GetWidget(),
					FVector2D( XAlignmentResult.Offset, PositionSoFar - SlotSize.Y + YAlignmentResult.Offset ),
					FVector2D( XAlignmentResult.Size, YAlignmentResult.Size )
					));

				PositionSoFar -= SlotSize.Y;
			}
		}
	}

	void Add(const TSharedRef<SWidget>& InWidget)
	{
		TSharedPtr<SWidgetStackItem> NewItem;

		InsertSlot(0)
		.AutoHeight()
		[
			SAssignNew(NewItem, SWidgetStackItem)
			[
				InWidget
			]
		];
		
		{
			auto Widget = Children[0].GetWidget();
			Widget->SlatePrepass();

			const float WidgetHeight = Widget->GetDesiredSize().Y;
			StartSlideOffset += WidgetHeight;
			// Fade in time is 1 second x the proportion of the slide amount that this widget takes up
			NewItem->FadeIn(WidgetHeight / StartSlideOffset);

			if (!SlideCurve.IsPlaying())
			{
				SlideCurve.Play(AsShared());
			}
		}

		const FVector2D NewSize = ComputeTotalSize();
		if (NewSize != StartSizeOffset)
		{
			StartSizeOffset = NewSize;

			if (!SizeCurve.IsPlaying())
			{
				SizeCurve.Play(AsShared());
			}
		}
	}
	
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		if (!SlideCurve.IsPlaying())
		{
			StartSlideOffset = 0;
		}

		// Delete any widgets that are now offscreen
		if (Children.Num() != 0)
		{
			const float Alpha = 1.f - SlideCurve.GetLerp();
			float PositionSoFar = AllottedGeometry.GetLocalSize().Y + Alpha * StartSlideOffset;

			for (int32 Index = 0; PositionSoFar > 0 && Index < NumSlots(); ++Index)
			{
				const SBoxPanel::FSlot& CurChild = Children[Index];
				const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();

				if (ChildVisibility != EVisibility::Collapsed)
				{
					const FVector2D ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();
					PositionSoFar -= ChildDesiredSize.Y + CurChild.SlotPadding.Get().GetTotalSpaceAlong<Orient_Vertical>();
				}
			}

			for (int32 Index = MaxNumVisible; Index < Children.Num(); )
			{
				if (StaticCastSharedRef<SWidgetStackItem>(Children[Index].GetWidget())->bIsFinished)
				{
					Children.RemoveAt(Index);
				}
				else
				{
					++Index;
				}
			}
		}
	}

	class SWidgetStackItem : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SWidgetStackItem){}
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			bIsFinished = false;

			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
				.ColorAndOpacity(this, &SWidgetStackItem::GetColorAndOpacity)
				.Padding(0)
				[
					InArgs._Content.Widget
				]
			];
		}
		
		void FadeIn(float Time)
		{
			OpacityCurve = FCurveSequence(0.f, Time, ECurveEaseFunction::QuadOut);
			OpacityCurve.Play(AsShared());
		}

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
		{
			if (!bIsFinished && OpacityCurve.IsAtStart() && OpacityCurve.IsInReverse())
			{
				bIsFinished = true;
			}
		}

		FLinearColor GetColorAndOpacity() const
		{
			return FLinearColor(1.f, 1.f, 1.f, OpacityCurve.GetLerp());
		}

		bool bIsFinished;
		FCurveSequence OpacityCurve;
	};

	FCurveSequence SlideCurve, SizeCurve;

	float StartSlideOffset;
	FVector2D StartSizeOffset;

	int32 MaxNumVisible;
};

void SReimportFeedback::Disable()
{
	WidgetStack->SetVisibility(EVisibility::HitTestInvisible);
}

void SReimportFeedback::Add(const TSharedRef<SWidget>& Widget)
{
	WidgetStack->Add(Widget);
}

void SReimportFeedback::SetMainText(FText InText)
{
	MainText = InText;
}

FText SReimportFeedback::GetMainText() const
{
	return MainText;
}

EVisibility SReimportFeedback::GetHyperlinkVisibility() const
{
	return WidgetStack->NumSlots() != 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

void SReimportFeedback::Construct(const FArguments& InArgs, FText InMainText)
{
	MainText = InMainText;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(10))
		.BorderImage(FCoreStyle::Get().GetBrush("NotificationList.ItemBackground"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding(FMargin(0))
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(this, &SReimportFeedback::GetMainText)
				.Font(FCoreStyle::Get().GetFontStyle(TEXT("NotificationList.FontBold")))
			]

			+ SVerticalBox::Slot()
			.Padding(FMargin(0, 5, 0, 0))
			.AutoHeight()
			[
				SAssignNew(WidgetStack, SWidgetStack, 3)
			]

			+ SVerticalBox::Slot()
			.Padding(FMargin(0, 5, 0, 0))
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SHyperlink)
				.Visibility(this, &SReimportFeedback::GetHyperlinkVisibility)
				.Text(NSLOCTEXT("ReimportContext", "OpenMessageLog", "Open message log"))
				.TextStyle(FCoreStyle::Get(), "SmallText")
				.OnNavigate_Lambda([]{
					FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
					MessageLogModule.OpenMessageLog("AssetReimport");
				})
			]
		]
	];
}

FReimportFeedbackContext::FReimportFeedbackContext()
	: bSuppressSlowTaskMessages(false)
	, MessageLog("AssetReimport")
{}

void FReimportFeedbackContext::Initialize(TSharedRef<SReimportFeedback> Widget)
{
	NotificationContent = Widget;

	FNotificationInfo Info(SharedThis(this));
	Info.ExpireDuration = 3.f;
	Info.bFireAndForget = false;

	Notification = FSlateNotificationManager::Get().AddNotification(Info);

	MessageLog.NewPage(FText::Format(NSLOCTEXT("ReimportContext", "MessageLogPageLabel", "Outstanding source content changes {0}"), FText::AsTime(FDateTime::Now())));
}

void FReimportFeedbackContext::Destroy()
{
	MessageLog.Notify(FText(), EMessageSeverity::Error);

	if (Notification.IsValid())
	{
		NotificationContent->Disable();
		Notification->SetCompletionState(SNotificationItem::CS_Success);
		Notification->ExpireAndFadeout();
	}
}

void FReimportFeedbackContext::AddMessage(EMessageSeverity::Type Severity, const FText& Message)
{
	MessageLog.Message(Severity, Message);
	AddWidget(SNew(STextBlock).Text(Message));
}

void FReimportFeedbackContext::AddWidget(const TSharedRef<SWidget>& Widget)
{
	NotificationContent->Add(Widget);
}

void FReimportFeedbackContext::StartSlowTask(const FText& Task, bool bShowCancelButton)
{
	FFeedbackContext::StartSlowTask(Task, bShowCancelButton);

	if (!bSuppressSlowTaskMessages && !Task.IsEmpty())
	{
		NotificationContent->Add(SNew(STextBlock).Text(Task));
	}
}
