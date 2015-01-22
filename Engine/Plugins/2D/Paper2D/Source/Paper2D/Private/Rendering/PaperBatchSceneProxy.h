// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FPaperBatchSceneProxy

class FPaperBatchSceneProxy : public FPrimitiveSceneProxy
{
public:
	FPaperBatchSceneProxy(const UPrimitiveComponent* InComponent);

 	// FPrimitiveSceneProxy interface.
 	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override;
 	virtual uint32 GetMemoryFootprint() const override;
 	virtual bool CanBeOccluded() const override;
 	// End of FPrimitiveSceneProxy interface.

	void RegisterManagedProxy(class FPaperRenderSceneProxy* Proxy);
	void UnregisterManagedProxy(class FPaperRenderSceneProxy* Proxy);
protected:
	TArray<class FPaperRenderSceneProxy*> ManagedProxies;
};
