// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUTAspectPanel.h"

#if !UE_SERVER

void SUTAspectPanel::Construct(const SUTAspectPanel::FArguments& InArgs)
{
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
		FVector2D DesiredSize = OrigSize;
		FVector2D ActualGeometrySize = AllottedGeometry.Size * AllottedGeometry.Scale;
		FVector2D FinalOffset(0, 0);
		float Aspect = ActualGeometrySize.Y / ActualGeometrySize.X;
		float Scale = 1.0f;
		if (AllottedGeometry.Size != DesiredSize)
		{
			// Attempt to scale us with a best fit.

			if (Aspect >= 0.5625) // Less than or equal to 16:9
			{
				// If this is the case, all we have to do is scale the resolution
				// and then center on the screen.

				Scale = AllottedGeometry.Size.X / DesiredSize.X;

				DesiredSize.X = ActualGeometrySize.X;
				DesiredSize.Y = DesiredSize.X * 0.5625;
				DesiredSize /= Scale;
			}
			else if (Aspect < 0.5625)	// Greater than 16:9
			{
				Scale = AllottedGeometry.Size.Y / DesiredSize.Y;

				DesiredSize.Y = ActualGeometrySize.Y;
				DesiredSize.X = DesiredSize.Y / 0.5625;
				DesiredSize /= AllottedGeometry.Scale;

				DesiredSize /= Scale;
			}

			FinalOffset /= AllottedGeometry.Scale;
		}

		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild( ChildSlot.GetWidget(), FinalOffset, DesiredSize, Scale));
	}
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