// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FLiveEditorListenServer;

namespace nLiveEditorListenServer
{

class FCreateListener : public FUObjectArray::FUObjectCreateListener
{
public:
	FCreateListener( FLiveEditorListenServer *_Owner );
	virtual ~FCreateListener();
	virtual void NotifyUObjectCreated(const class UObjectBase *Object, int32 Index);

private:
	FLiveEditorListenServer *Owner;
};

class FDeleteListener : public FUObjectArray::FUObjectDeleteListener
{
public:
	FDeleteListener( FLiveEditorListenServer *_Owner );
	virtual ~FDeleteListener();
	virtual void NotifyUObjectDeleted(const class UObjectBase *Object, int32 Index);

private:
	FLiveEditorListenServer *Owner;
};

class FTickObject : FTickableGameObject
{
public:
	FTickObject( FLiveEditorListenServer *_Owner );

	virtual void Tick(float DeltaTime) override;

	virtual bool IsTickable() const override
	{
		return true;
	}
	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}
	virtual bool IsTickableInEditor() const override
	{
		return false;
	}
	virtual TStatId GetStatId() const override;

private:
	FLiveEditorListenServer *Owner;
};

class FMapLoadObserver : public TSharedFromThis<FMapLoadObserver>
{
public:
	FMapLoadObserver( FLiveEditorListenServer *_Owner );
	~FMapLoadObserver();

	void OnPreLoadMap(const FString& MapName);
	void OnPostLoadMap();

private:
	FLiveEditorListenServer *Owner;
};

}

