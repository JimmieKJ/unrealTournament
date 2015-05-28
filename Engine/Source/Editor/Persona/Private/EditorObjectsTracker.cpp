// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PersonaPrivatePCH.h"
#include "EditorObjectsTracker.h"

void FEditorObjectTracker::AddReferencedObjects( FReferenceCollector& Collector )
{
	for (TMap<UClass*, UObject*>::TIterator It(EditorObjMap); It; ++It)
	{
		UObject *Obj = It.Value();
		if(ensure(Obj))
		{
			Collector.AddReferencedObject(Obj);
		}
	}	
}

UObject* FEditorObjectTracker::GetEditorObjectForClass( UClass* EdClass )
{
	UObject *Obj = (EditorObjMap.Contains(EdClass) ? *EditorObjMap.Find(EdClass) : NULL);
	if(Obj == NULL)
	{
		FString ObjName = MakeUniqueObjectName(GetTransientPackage(), EdClass).ToString();
		ObjName += "_EdObj";
		Obj = NewObject<UObject>(GetTransientPackage(), EdClass, FName(*ObjName), RF_Public | RF_Standalone | RF_Transient);
		EditorObjMap.Add(EdClass,Obj);
	}
	return Obj;
}