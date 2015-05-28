// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ObjectEditorUtils.h"
#include "NavigationModifier.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Navigation/NavLinkCustomComponent.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "AI/Navigation/NavLinkCustomComponent.h"
#include "AI/Navigation/NavLinkProxy.h"
#include "AI/Navigation/NavLinkRenderingComponent.h"
#include "NavigationSystemHelpers.h"

ANavLinkProxy::ANavLinkProxy(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PositionComponent"));
	RootComponent = SceneComponent;

	bHidden = true;

#if WITH_EDITORONLY_DATA
	EdRenderComp = CreateDefaultSubobject<UNavLinkRenderingComponent>(TEXT("EdRenderComp"));
	EdRenderComp->PostPhysicsComponentTick.bCanEverTick = false;
	EdRenderComp->AttachParent = RootComponent;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (!IsRunningCommandlet() && (SpriteComponent != NULL))
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture(TEXT("/Engine/EditorResources/AI/S_NavLink"));

		SpriteComponent->Sprite = SpriteTexture.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->bHiddenInGame = true;
		SpriteComponent->bVisible = true;

		SpriteComponent->AttachParent = RootComponent;
		SpriteComponent->SetAbsolute(false, false, true);
		SpriteComponent->bIsScreenSizeScaled = true;
	}
#endif

	SmartLinkComp = CreateDefaultSubobject<UNavLinkCustomComponent>(TEXT("SmartLinkComp"));
	SmartLinkComp->SetNavigationRelevancy(false);
	SmartLinkComp->SetMoveReachedLink(this, &ANavLinkProxy::NotifySmartLinkReached);
	bSmartLinkIsRelevant = false;

	PointLinks.Add(FNavigationLink());
	SetActorEnableCollision(false);

	bCanBeDamaged = false;
}

#if WITH_EDITOR
void ANavLinkProxy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ANavLinkProxy,bSmartLinkIsRelevant))
	{
		SmartLinkComp->SetNavigationRelevancy(bSmartLinkIsRelevant);
	}

	const FName CategoryName = FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property);
	const FName MemberCategoryName = FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.MemberProperty);
	if (CategoryName == TEXT("SimpleLink") || MemberCategoryName == TEXT("SimpleLink"))
	{
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		if (NavSys)
		{
			NavSys->UpdateNavOctree(this);
		}
	}
}
#endif // WITH_EDITOR

void ANavLinkProxy::GetNavigationData(FNavigationRelevantData& Data) const
{
	NavigationHelper::ProcessNavLinkAndAppend(&Data.Modifiers, this, PointLinks);
	NavigationHelper::ProcessNavLinkSegmentAndAppend(&Data.Modifiers, this, SegmentLinks);
}

FBox ANavLinkProxy::GetNavigationBounds() const
{
	return GetComponentsBoundingBox();
}

bool ANavLinkProxy::IsNavigationRelevant() const
{
	return (PointLinks.Num() > 0) || (SegmentLinks.Num() > 0);
}

bool ANavLinkProxy::GetNavigationLinksClasses(TArray<TSubclassOf<UNavLinkDefinition> >& OutClasses) const
{
	return false;
}

bool ANavLinkProxy::GetNavigationLinksArray(TArray<FNavigationLink>& OutLink, TArray<FNavigationSegmentLink>& OutSegments) const
{
	OutLink.Append(PointLinks);
	OutSegments.Append(SegmentLinks);

	return (PointLinks.Num() > 0) || (SegmentLinks.Num() > 0);
}

FBox ANavLinkProxy::GetComponentsBoundingBox(bool bNonColliding) const
{
	FBox LinksBB(0);
	LinksBB += FVector(0,0,-10);
	LinksBB += FVector(0,0,10);

	for (int32 i = 0; i < PointLinks.Num(); ++i)
	{
		const FNavigationLink& Link = PointLinks[i];
		LinksBB += Link.Left;
		LinksBB += Link.Right;
	}

	for (int32 i = 0; i < SegmentLinks.Num(); ++i)
	{
		const FNavigationSegmentLink& SegmentLink = SegmentLinks[i];
		LinksBB += SegmentLink.LeftStart;
		LinksBB += SegmentLink.LeftEnd;
		LinksBB += SegmentLink.RightStart;
		LinksBB += SegmentLink.RightEnd;
	}

	LinksBB = LinksBB.TransformBy(RootComponent->ComponentToWorld);

	if (SmartLinkComp->IsNavigationRelevant())
	{
		LinksBB += SmartLinkComp->GetStartPoint();
		LinksBB += SmartLinkComp->GetEndPoint();
	}

	return LinksBB;
}

void ANavLinkProxy::NotifySmartLinkReached(UNavLinkCustomComponent* LinkComp, UPathFollowingComponent* PathComp, const FVector& DestPoint)
{
	AActor* PathOwner = PathComp->GetOwner();
	AController* ControllerOwner = Cast<AController>(PathOwner);
	if (ControllerOwner)
	{
		PathOwner = ControllerOwner->GetPawn();
	}

	ReceiveSmartLinkReached(PathOwner, DestPoint);
	OnSmartLinkReached.Broadcast(PathOwner, DestPoint);
}

void ANavLinkProxy::ResumePathFollowing(AActor* Agent)
{
	if (Agent)
	{
		UPathFollowingComponent* PathComp = Agent->FindComponentByClass<UPathFollowingComponent>();
		if (PathComp == NULL)
		{
			APawn* PawnOwner = Cast<APawn>(Agent);
			if (PawnOwner && PawnOwner->GetController())
			{
				PathComp = PawnOwner->GetController()->FindComponentByClass<UPathFollowingComponent>();
			}
		}

		if (PathComp)
		{
			PathComp->FinishUsingCustomLink(SmartLinkComp);
		}
	}
}

bool ANavLinkProxy::IsSmartLinkEnabled() const
{
	return SmartLinkComp->IsEnabled();
}

void ANavLinkProxy::SetSmartLinkEnabled(bool bEnabled)
{
	SmartLinkComp->SetEnabled(bEnabled);
}

bool ANavLinkProxy::HasMovingAgents() const
{
	return SmartLinkComp->HasMovingAgents();
}

/** Returns SmartLinkComp subobject **/
UNavLinkCustomComponent* ANavLinkProxy::GetSmartLinkComp() const { return SmartLinkComp; }
#if WITH_EDITORONLY_DATA
/** Returns EdRenderComp subobject **/
UNavLinkRenderingComponent* ANavLinkProxy::GetEdRenderComp() const { return EdRenderComp; }
/** Returns SpriteComponent subobject **/
UBillboardComponent* ANavLinkProxy::GetSpriteComponent() const { return SpriteComponent; }
#endif
