// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "StaticMeshSocket.generated.h"

UCLASS(hidecategories=Object, hidecategories=Actor, MinimalAPI)
class UStaticMeshSocket : public UObject
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	Defines a named attachment location on the UStaticMesh. 
	 *	These are set up in editor and used as a shortcut instead of specifying 
	 *	everything explicitly to AttachComponent in the StaticMeshComponent.
	 *	The Outer of a StaticMeshSocket should always be the UStaticMesh.
	 */
	UPROPERTY(Category=StaticMeshSocket, BlueprintReadOnly)
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=StaticMeshSocket)
	FVector RelativeLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=StaticMeshSocket)
	FRotator RelativeRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=StaticMeshSocket)
	FVector RelativeScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=StaticMeshSocket)
	FString Tag;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=StaticMeshSocket)
	class UStaticMesh* PreviewStaticMesh;
#endif // WITH_EDITORONLY_DATA

	/** Utility that returns the current matrix for this socket. Returns false if socket was not valid */
	ENGINE_API bool GetSocketMatrix(FMatrix& OutMatrix, class UStaticMeshComponent const* MeshComp) const;

	/** Utility that returns the current transform for this socket. Returns false if socket was not valid */
	ENGINE_API bool GetSocketTransform(FTransform& OutTransform, class UStaticMeshComponent const* MeshComp) const;

	/** 
	 *	Utility to associate an actor with a socket
	 *	
	 *	@param	Actor			The actor to attach to the socket
	 *	@param	MeshComp		The static mesh component associated with this socket.
	 *
	 *	@return	bool			true if successful, false if not
	 */
	ENGINE_API bool AttachActor(AActor* Actor, class UStaticMeshComponent* MeshComp) const;

public:
#if WITH_EDITOR
	/** Broadcasts a notification whenever the socket property has changed. */
	DECLARE_EVENT_TwoParams( UStaticMeshSocket, FChangedEvent, const class UStaticMeshSocket*, const class UProperty* );
	FChangedEvent& OnPropertyChanged() { return ChangedEvent; }

	// Begin UObject interface
	virtual void PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) override;
	// End UObject interface

private:
	/** Broadcasts a notification whenever the socket property has changed. */
	FChangedEvent ChangedEvent;
#endif // WITH_EDITOR
};



