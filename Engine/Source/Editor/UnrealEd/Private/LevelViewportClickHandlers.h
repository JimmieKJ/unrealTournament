// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FLevelEditorViewportClient;
struct FViewportClick;
struct HGeomEdgeProxy;
struct HGeomPolyProxy;
struct HGeomVertexProxy;
struct HActor;

/**
 * A hit proxy class for sockets in the main editor viewports.
 */
struct HLevelSocketProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	AActor* Actor;
	USceneComponent* SceneComponent;
	FName SocketName;

	HLevelSocketProxy(AActor* InActor, USceneComponent* InSceneComponent, FName InSocketName)
		:	HHitProxy(HPP_UI)
		,   Actor( InActor )
		,   SceneComponent( InSceneComponent )
		,	SocketName( InSocketName )
	{}
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject( Actor );
		Collector.AddReferencedObject( SceneComponent );
	}
};

namespace ClickHandlers
{
	bool ClickViewport(FLevelEditorViewportClient* ViewportClient, const FViewportClick& Click);

	bool ClickActor(FLevelEditorViewportClient* ViewportClient,AActor* Actor,const FViewportClick& Click,bool bAllowSelectionChange);

	bool ClickComponent(FLevelEditorViewportClient* ViewportClient, HActor* ActorHitProxy, const FViewportClick& Click);

	void ClickBrushVertex(FLevelEditorViewportClient* ViewportClient,ABrush* InBrush,FVector* InVertex,const FViewportClick& Click);

	void ClickStaticMeshVertex(FLevelEditorViewportClient* ViewportClient,AActor* InActor,FVector& InVertex,const FViewportClick& Click);
	
	bool ClickGeomPoly(FLevelEditorViewportClient* ViewportClient, HGeomPolyProxy* InHitProxy, const FViewportClick& Click);

	bool ClickGeomEdge(FLevelEditorViewportClient* ViewportClient, HGeomEdgeProxy* InHitProxy, const FViewportClick& Click);

	bool ClickGeomVertex(FLevelEditorViewportClient* ViewportClient,HGeomVertexProxy* InHitProxy,const FViewportClick& Click);
	
	void ClickSurface(FLevelEditorViewportClient* ViewportClient,UModel* Model,int32 iSurf,const FViewportClick& Click);
	
	void ClickBackdrop(FLevelEditorViewportClient* ViewportClient,const FViewportClick& Click);

	void ClickLevelSocket(FLevelEditorViewportClient* ViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
};


