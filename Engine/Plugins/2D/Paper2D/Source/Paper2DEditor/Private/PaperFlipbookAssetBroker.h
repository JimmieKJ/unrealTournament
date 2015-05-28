// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentAssetBroker.h"
#include "PaperFlipbookComponent.h"

//////////////////////////////////////////////////////////////////////////
// FPaperFlipbookAssetBroker

class FPaperFlipbookAssetBroker : public IComponentAssetBroker
{
public:
	UClass* GetSupportedAssetClass() override
	{
		return UPaperFlipbook::StaticClass();
	}

	virtual bool AssignAssetToComponent(UActorComponent* InComponent, UObject* InAsset) override
	{
		if (UPaperFlipbookComponent* RenderComp = Cast<UPaperFlipbookComponent>(InComponent))
		{
			UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(InAsset);

			if ((Flipbook != nullptr) || (InAsset == nullptr))
			{
				RenderComp->SetFlipbook(Flipbook);
				return true;
			}
		}

		return false;
	}

	virtual UObject* GetAssetFromComponent(UActorComponent* InComponent) override
	{
		if (UPaperFlipbookComponent* RenderComp = Cast<UPaperFlipbookComponent>(InComponent))
		{
			return RenderComp->GetFlipbook();
		}
		return nullptr;
	}
};

