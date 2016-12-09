// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTAspectPanel.h"

#if !UE_SERVER

void SUTAspectPanel::Construct(const SUTAspectPanel::FArguments& InArgs)
{
	CachedLayoutScale = 1.0f;
	bCenterPanel = InArgs._bCenterPanel;
	ChildSlot
		[
			SNew(SBox).WidthOverride(1920.0f).HeightOverride(1080.0f)
			[
				InArgs._Content.Widget
			]
		];
}

void SUTAspectPanel::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if (ArrangedChildren.Accepts(ChildVisibility))
	{
 		FVector2D OrigSize = ChildSlot.GetWidget()->GetDesiredSize(); 
		FVector2D DesiredDrawSize = OrigSize;
		FVector2D ActualGeometrySize = AllottedGeometry.Size * AllottedGeometry.Scale;
		FVector2D FinalOffset(0, 0);
		float Aspect = ActualGeometrySize.Y / ActualGeometrySize.X;
		float Scale = 1.0f;
		if (AllottedGeometry.Size != DesiredDrawSize && !DesiredDrawSize.IsZero())
		{
			// Attempt to scale us with a best fit.

			if (Aspect >= 0.5625) // Less than or equal to 16:9
			{
				// If this is the case, all we have to do is scale the resolution
				// and then center on the screen.

				Scale = AllottedGeometry.Size.X / DesiredDrawSize.X;

				DesiredDrawSize.X = ActualGeometrySize.X;
				DesiredDrawSize.Y = DesiredDrawSize.X * 0.5625;
			}
			else if (Aspect < 0.5625)	// Greater than 16:9
			{
				Scale = AllottedGeometry.Size.Y / DesiredDrawSize.Y;

				DesiredDrawSize.Y = ActualGeometrySize.Y;
				DesiredDrawSize.X = DesiredDrawSize.Y / 0.5625;
				DesiredDrawSize /= AllottedGeometry.Scale;
			}

			FinalOffset /= AllottedGeometry.Scale;
		}

		if (bCenterPanel)
		{
			FinalOffset.Y = (AllottedGeometry.Size.Y - DesiredDrawSize.Y) * 0.5f / Scale;
		}

		DesiredDrawSize /= Scale;
		CachedLayoutScale = Scale;

		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild( ChildSlot.GetWidget(), FinalOffset, DesiredDrawSize, Scale));
	}
}

float SUTAspectPanel::GetRelativeLayoutScale(const FSlotBase& Child) const
{
	return CachedLayoutScale;
}

void SUTAspectPanel::SetContent(TSharedRef<SWidget> InContent)
{
	ChildSlot
		[
			SNew(SBox).WidthOverride(1920.0f).HeightOverride(1080.0f)
			[
				InContent
			]
		];
}

#endif