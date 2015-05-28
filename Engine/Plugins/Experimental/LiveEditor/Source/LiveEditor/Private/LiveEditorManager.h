// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "portmidi.h"
#include "LiveEditorDevice.h"
#include "LiveEditorDeviceSetupWizard.h"

class ILiveEditListener {
public:
	ILiveEditListener(FName _Name, const FLiveEditBinding &_Binding, class UObject *_Target);
	~ILiveEditListener();

	FName Name;
	FLiveEditBinding Binding;
	TWeakObjectPtr<class UObject> Target;
};

class FLiveEditorManager : public FTickableEditorObject
{
public:
	FLiveEditorManager();
	virtual ~FLiveEditorManager();

	void Initialize();

	static FLiveEditorManager &Get();
	static void Shutdown();

	virtual void Tick(float DeltaTime) override;

	virtual bool IsTickable() const override
	{
		return bInitialized;
	}
	virtual bool IsTickableWhenPaused() const
	{
		return bInitialized;
	}
	virtual bool IsTickableInEditor() const
	{
		return bInitialized;
	}
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FLiveEditorManager, STATGROUP_Tickables);
	}

	inline bool IsInitialized() { return bInitialized; }

	void RegisterEventListener( UObject *Target, FName EventName );
	void UnregisterListener( UObject *Target, FName VarName );
	void UnregisterAllListeners( UObject *Target );

	void SelectLiveEditorBlueprint( class UBlueprint *Blueprint );

	inline TArray<FString> &GetActiveBlueprintTemplates()
	{
		return ActiveBlueprintTemplates;
	}
	bool CheckActive( const FString &Name );
	bool Activate( const FString &Name );
	bool DeActivate( const FString &Name );
	bool Remove( const FString &Name );

	void InjectNewBlueprintEditor( const TWeakPtr<class FBlueprintEditor> &InEditor );

	void RefreshDeviceConnections();

	void OnObjectCreation( const class UObjectBase *NewObject );
	void OnObjectDeletion( const class UObjectBase *NewObject );
	void OnBeginPIE();
	void OnEndPIE();
	void FindPiePartners( const class UObject *Archetype, TArray< TWeakObjectPtr<UObject> > &OutValues );
	void BroadcastValueUpdate( const FString &ClassName, const FString &PropertyName, const FString &PropertyValue );

	bool GetDeviceIDs( TArray< TSharedPtr<PmDeviceID> > &DeviceIDs ) const;
	bool GetDeviceData( PmDeviceID DeviceID, struct FLiveEditorDeviceData &DeviceData ) const;
	void ConfigureDevice( PmDeviceID DeviceID );

	bool BlueprintHasBinding( const class UBlueprint &Blueprint );
	void StartBlueprintBinding( class UBlueprint &Blueprint );
	void FinishBlueprintBinding( class UBlueprint &Blueprint );

	void SetActiveWizard( class FLiveEditorWizardBase *Wizard );
	class FLiveEditorWizardBase *GetActiveWizard()
	{
		return ActiveWizard;
	}

	static bool SaveDeviceData( const struct FLiveEditorDeviceData &DeviceData );

	class FLiveEditorUserData *GetLiveEditorUserData()
	{
		return LiveEditorUserData;
	}

	bool ConnectToRemoteHost( FString IPAddress );
	bool DisconnectFromRemoteHost( FString IPAddress );

	inline UWorld *GetLiveEditorWorld() const
	{
		return LiveEditorWorld;
	}

private:
	struct FActiveBlueprintRecord
	{
		FString Name;
		TWeakObjectPtr<class ULiveEditorBlueprint> Blueprint;
	};
	class FPieObjectCache
	{
	public:
		FPieObjectCache():bCacheActive(false) {}
		~FPieObjectCache() {}

		/**
		 * Performs a quick Map lookup if we're already tracking this key, full TObjectIteration otherwise
		 **/
		void FindPiePartners( const class UObject* EditorObject, TArray< TWeakObjectPtr<UObject> >& OutValues );
		void OnBeginPIE();
		void OnEndPIE();
		void OnObjectCreation( const class UObjectBase* ObjectBase );
		void OnObjectDeletion( const class UObjectBase* ObjectBase );
		void EvaluatePendingCreations();

	private:
		bool CacheStarted() const;

		bool bCacheActive;
		TArray< const class UObject* > TrackedObjects;
		TMultiMap< const class UObject* /*Archetype*/, TWeakObjectPtr<class UObject> /*Descendant*/> ObjectLookupCache;
		TArray< const class UObjectBase* > PendingObjectCreations;
	};

private:

	void FindDevices();
	void CloseDeviceConnections();
	//arguments need to remain int (instead of int32) since numbers are derived from TPL that uses int
	void Dispatch( int Status, int Data1, int Data2, const struct FLiveEditorDeviceData &Data );
	class ULiveEditorBlueprint *FindActiveBlueprint( const FString &Name );
	void CreateLiveEditorWorld();

	static bool LoadDeviceData( const FString &DeviceName, struct FLiveEditorDeviceData &DeviceData );

private:
	bool bInitialized;
	PmEvent *MIDIBuffer;
	TMap< PmDeviceID, struct FLiveEditorDeviceInstance > InputConnections;
	TMultiMap<int,ILiveEditListener*> EventListeners; //needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	TArray<FString> ActiveBlueprintTemplates;
	TArray< FActiveBlueprintRecord > ActiveBlueprints;
	TArray< TWeakPtr<class FBlueprintEditor> > ActiveBlueprintEditors;
	FPieObjectCache PieObjectCache;
	class FLiveEditorWizardBase *ActiveWizard;
	class FLiveEditorDeviceSetupWizard *LiveEditorDeviceSetupWizard;
	class FLiveEditorBlueprintBindingWizard *LiveEditorBlueprintBindingWizard;
	class FLiveEditorUserData *LiveEditorUserData;
	TMap<FString, class FSocket *> RemoteConnections;

	UWorld *LiveEditorWorld;
	UWorld *RealWorld;

	float LastUpdateTime;
};
