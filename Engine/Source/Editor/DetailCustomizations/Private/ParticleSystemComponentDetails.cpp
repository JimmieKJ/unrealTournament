// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "ParticleSystemComponentDetails.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystemComponent.h"

#define LOCTEXT_NAMESPACE "ParticleSystemComponentDetails"

TSharedRef<IDetailCustomization> FParticleSystemComponentDetails::MakeInstance()
{
	return MakeShareable(new FParticleSystemComponentDetails);
}

void FParticleSystemComponentDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	this->DetailLayout = &InDetailLayout;

	InDetailLayout.EditCategory("Particles", FText::GetEmpty(), ECategoryPriority::Important);

	IDetailCategoryBuilder& CustomCategory = InDetailLayout.EditCategory("EmitterActions", NSLOCTEXT("ParticleSystemComponentDetails", "EmitterActionCategoryName", "Emitter Actions"), ECategoryPriority::Important);

	CustomCategory.AddCustomRow(FText::GetEmpty())
		.WholeRowContent()
		.HAlign(HAlign_Left)
		[
			SNew( SBox )
			.MaxDesiredWidth(300.f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2.0f)
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.OnClicked(this, &FParticleSystemComponentDetails::OnAutoPopulateClicked)
					.ToolTipText(NSLOCTEXT("ParticleSystemComponentDetails", "AutoPopulateButtonTooltip", "Copies properties from the source particle system into the instanced parameters of this system"))
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("ParticleSystemComponentDetails", "AutoPopulateButton", "Expose Parameter"))
					]
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.OnClicked(this, &FParticleSystemComponentDetails::OnResetEmitter)
					.ToolTipText(NSLOCTEXT("ParticleSystemComponentDetails", "ResetEmitterButtonTooltip", "Resets the selected particle system."))
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("ParticleSystemComponentDetails", "ResetEmitterButton", "Reset Emitter"))
					]
				]
			]
		];
}

FReply FParticleSystemComponentDetails::OnAutoPopulateClicked()
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout->GetDetailsView().GetSelectedObjects();

	for (int32 Idx = 0; Idx < SelectedObjects.Num(); ++Idx)
	{
		if (SelectedObjects[Idx].IsValid())
		{
			if (UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(SelectedObjects[Idx].Get()))
			{
				PSC->AutoPopulateInstanceProperties();
			}
			else if (AEmitter* Emitter = Cast<AEmitter>(SelectedObjects[Idx].Get()))
			{
				Emitter->AutoPopulateInstanceProperties();
			}
		}
	}

	return FReply::Handled();
}

FReply FParticleSystemComponentDetails::OnResetEmitter()
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout->GetDetailsView().GetSelectedObjects();
	// Iterate over selected Actors.
	for (int32 Idx = 0; Idx < SelectedObjects.Num(); ++Idx)
	{
		if (SelectedObjects[Idx].IsValid())
		{
			UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(SelectedObjects[Idx].Get());
			if (!PSC)
			{
				if (AEmitter* Emitter = Cast<AEmitter>(SelectedObjects[Idx].Get()))
				{
					PSC = Emitter->GetParticleSystemComponent();
				}
			}

			if (PSC)
			{
				PSC->ResetParticles();
				PSC->ActivateSystem();
			}
		}
	}
	return FReply::Handled();
}



#undef LOCTEXT_NAMESPACE
