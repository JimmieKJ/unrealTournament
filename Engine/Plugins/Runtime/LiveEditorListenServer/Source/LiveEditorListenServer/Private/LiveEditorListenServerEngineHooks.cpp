// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorListenServerPrivatePCH.h"

namespace nLiveEditorListenServer
{

/**
 * FCreateListener
 **/
FCreateListener::FCreateListener( FLiveEditorListenServer *_Owner )
: Owner(_Owner)
{
}

FCreateListener::~FCreateListener()
{
}

void FCreateListener::NotifyUObjectCreated(const class UObjectBase *Object, int32 Index)
{
	check( Owner != NULL );
	Owner->OnObjectCreation( Object );
}



/**
 * FDeleteListener
 **/
FDeleteListener::FDeleteListener( FLiveEditorListenServer *_Owner )
: Owner(_Owner)
{
}

FDeleteListener::~FDeleteListener()
{
}

void FDeleteListener::NotifyUObjectDeleted(const class UObjectBase *Object, int32 Index)
{
	check( Owner != NULL );
	Owner->OnObjectDeletion( Object );
}


/**
 * FTickObject
 **/
FTickObject::FTickObject( FLiveEditorListenServer *_Owner )
:	Owner(_Owner)
{
}

void FTickObject::Tick(float DeltaTime)
{
	check( Owner != NULL );
	Owner->Tick(DeltaTime);
}

TStatId FTickObject::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FLiveEditorListenServer, STATGROUP_Tickables);
}

/**
 * FTickObject
 **/
FMapLoadObserver::FMapLoadObserver( FLiveEditorListenServer *_Owner )
: Owner(_Owner)
{
}

FMapLoadObserver::~FMapLoadObserver()
{
}

void FMapLoadObserver::OnPreLoadMap(const FString& MapName)
{
	check( Owner != NULL );
	Owner->OnPreLoadMap();
}

void FMapLoadObserver::OnPostLoadMap()
{
	check( Owner != NULL );
	Owner->OnPostLoadMap();
}

}
