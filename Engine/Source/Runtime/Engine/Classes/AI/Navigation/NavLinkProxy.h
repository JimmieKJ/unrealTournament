// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AI/Navigation/NavLinkDefinition.h"
#include "AI/Navigation/NavLinkHostInterface.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "GameFramework/Actor.h"
#include "NavLinkProxy.generated.h"

class UNavLinkDefinition;
class UPathFollowingComponent;
struct FNavigationRelevantData;
class UNavLinkCustomComponent;
class UNavLinkRenderingComponent;
class UBillboardComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FSmartLinkReachedSignature, AActor*, MovingActor, const FVector&, DestinationPoint );

UCLASS(Blueprintable, autoCollapseCategories=(SmartLink, Actor), hideCategories=(Input))
class ENGINE_API ANavLinkProxy : public AActor, public INavLinkHostInterface, public INavRelevantInterface
{
	GENERATED_UCLASS_BODY()

	/** Navigation links (point to point) added to navigation data */
	UPROPERTY(EditAnywhere, Category=SimpleLink)
	TArray<FNavigationLink> PointLinks;
	
	/** Navigation links (segment to segment) added to navigation data
	*	@todo hidden from use until we fix segment links. Not really working now*/
	UPROPERTY()
	TArray<FNavigationSegmentLink> SegmentLinks;

private_subobject:
	/** Smart link: can affect path following */
	DEPRECATED_FORGAME(4.6, "SmartLinkComp should not be accessed directly, please use GetSmartLinkComp() function instead. SmartLinkComp will soon be private and your code will not compile.")
	UPROPERTY(VisibleAnywhere, Category=SmartLink)
	UNavLinkCustomComponent* SmartLinkComp;
public:

	/** Smart link: toggle relevancy */
	UPROPERTY(EditAnywhere, Category=SmartLink)
	bool bSmartLinkIsRelevant;

#if WITH_EDITORONLY_DATA
private_subobject:
	/** Editor Preview */
	DEPRECATED_FORGAME(4.6, "EdRenderComp should not be accessed directly, please use GetEdRenderComp() function instead. EdRenderComp will soon be private and your code will not compile.")
	UPROPERTY()
	UNavLinkRenderingComponent* EdRenderComp;

	DEPRECATED_FORGAME(4.6, "SpriteComponent should not be accessed directly, please use GetSpriteComponent() function instead. SpriteComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
public:
#endif // WITH_EDITORONLY_DATA

	// BEGIN INavRelevantInterface
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual FBox GetNavigationBounds() const override;
	virtual bool IsNavigationRelevant() const override;
	// END INavRelevantInterface

	// BEGIN INavLinkHostInterface
	virtual bool GetNavigationLinksClasses(TArray<TSubclassOf<UNavLinkDefinition> >& OutClasses) const override;
	virtual bool GetNavigationLinksArray(TArray<FNavigationLink>& OutLink, TArray<FNavigationSegmentLink>& OutSegments) const override;
	// END INavLinkHostInterface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false) const override;

	//////////////////////////////////////////////////////////////////////////
	// Blueprint interface for smart links

	/** called when agent reaches smart link during path following, use ResumePathFollowing() to give control back */
	UFUNCTION(BlueprintImplementableEvent)
	void ReceiveSmartLinkReached(AActor* Agent, const FVector& Destination);

	/** resume normal path following */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	void ResumePathFollowing(AActor* Agent);

	/** check if smart link is enabled */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	bool IsSmartLinkEnabled() const;

	/** change state of smart link */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	void SetSmartLinkEnabled(bool bEnabled);

	/** check if any agent is moving through smart link right now */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	bool HasMovingAgents() const;

protected:

	UPROPERTY(BlueprintAssignable)
	FSmartLinkReachedSignature OnSmartLinkReached;

	void NotifySmartLinkReached(UNavLinkCustomComponent* LinkComp, UPathFollowingComponent* PathComp, const FVector& DestPoint);

public:
	/** Returns SmartLinkComp subobject **/
	UNavLinkCustomComponent* GetSmartLinkComp() const;
#if WITH_EDITORONLY_DATA
	/** Returns EdRenderComp subobject **/
	UNavLinkRenderingComponent* GetEdRenderComp() const;
	/** Returns SpriteComponent subobject **/
	UBillboardComponent* GetSpriteComponent() const;
#endif
};
