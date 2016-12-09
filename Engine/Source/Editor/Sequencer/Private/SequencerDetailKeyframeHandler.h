// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISequencer.h"
#include "IDetailKeyframeHandler.h"

class IPropertyHandle;

class FSequencerDetailKeyframeHandler : public IDetailKeyframeHandler
{
public:
	FSequencerDetailKeyframeHandler(TSharedRef<ISequencer> InSequencer);

	virtual bool IsPropertyKeyable(UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const override;

	virtual bool IsPropertyKeyingEnabled() const override;

	virtual void OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle) override;

private:
	TWeakPtr<class ISequencer> Sequencer;
};
