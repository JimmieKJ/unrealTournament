// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentAssetBroker.h"
#include "PhysicsEngine/BodySetup.h"
#include "PaperSpriteComponent.h"

//////////////////////////////////////////////////////////////////////////
// FPaperSpriteAssetBroker

class FPaperSpriteAssetBroker : public IComponentAssetBroker
{
public:
	UClass* GetSupportedAssetClass() override
	{
		return UPaperSprite::StaticClass();
	}

	virtual bool AssignAssetToComponent(UActorComponent* InComponent, UObject* InAsset) override
	{
		if (UPaperSpriteComponent* RenderComponent = Cast<UPaperSpriteComponent>(InComponent))
		{
			UPaperSprite* Sprite = Cast<UPaperSprite>(InAsset);

			if ((Sprite != nullptr) || (InAsset == nullptr))
			{
				RenderComponent->SetSprite(Sprite);

				if (Sprite->BodySetup != nullptr)
				{
					RenderComponent->BodyInstance.CopyBodyInstancePropertiesFrom(&(Sprite->BodySetup->DefaultInstance));
				}

				return true;
			}
		}

		return false;
	}

	virtual UObject* GetAssetFromComponent(UActorComponent* InComponent) override
	{
		if (UPaperSpriteComponent* RenderComponent = Cast<UPaperSpriteComponent>(InComponent))
		{
			return RenderComponent->GetSprite();
		}
		return nullptr;
	}
};

