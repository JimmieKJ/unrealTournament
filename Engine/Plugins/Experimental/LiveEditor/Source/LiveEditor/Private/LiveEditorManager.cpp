// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "LiveEditorManager.h"
#include "LiveEditorUserData.h"
#include "LiveEditorBlueprintBindingWizard.h"
#include "LiveEditorDeviceSetupWizard.h"
#include "Networking.h"
#include "LiveEditorListenServerPublic.h"

#include "BlueprintEditor.h"
#include "BlueprintEditorUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LiveEditorManagerLog, Log, All);

#define DEFAULT_BUFFER_SIZE 1024

#define LISTENER_ID(channel,id) channel*256 + id


namespace nPieObjectCache
{
	FORCEINLINE bool IsPiePartner( const class UObject& EditorObject, const class UObject& TestObject )
	{
		if ( EditorObject.HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject) )
		{
			return TestObject.IsA( EditorObject.GetClass() );
		}
		else
		{
			return false;
		}
	}
}


/**
 * 
 * ILiveEditListener
 *
 **/

ILiveEditListener::ILiveEditListener(FName _Name, const FLiveEditBinding& _Binding, class UObject* _Target)
:	Name(_Name),
	Binding(_Binding),
	Target(_Target)
{
}

ILiveEditListener::~ILiveEditListener()
{
}



/**
 *
 * FLiveEditorManager::FPieObjectCache
 *
 **/

 //Performs a quick Map lookup if we're already tracking this key, full TObjectIteration otherwise
void FLiveEditorManager::FPieObjectCache::FindPiePartners( const class UObject* EditorObject, TArray< TWeakObjectPtr<UObject> >& OutValues )
{
	if ( EditorObject == NULL || !bCacheActive )
		return;

	if ( ObjectLookupCache.Contains(EditorObject) )
	{
		ObjectLookupCache.MultiFind( EditorObject, OutValues );
	}
	else
	{
		UWorld *OldWorld = NULL;
		if ( GEditor->PlayWorld != NULL )
		{
			OldWorld = GWorld;
			GWorld = GEditor->PlayWorld;
		}

		TArray<UObject*> ObjectsToChange;
		const bool bIncludeDerivedClasses = true;
		GetObjectsOfClass(EditorObject->GetClass(), ObjectsToChange, bIncludeDerivedClasses);
		for ( auto ObjIt = ObjectsToChange.CreateIterator(); ObjIt; ++ObjIt )
		{
			UObject *Object = *ObjIt;

			//UWorld *World = GEngine->GetWorldFromContextObject( Object, false );
			//if ( World == NULL || World->WorldType != EWorldType::PIE )
			//	continue;

			if ( !nPieObjectCache::IsPiePartner(*EditorObject, *Object) )
				continue;

			ObjectLookupCache.AddUnique( EditorObject, Object );
			TrackedObjects.Add( Object );

			OutValues.Add( Object );
		}

		if ( GEditor->PlayWorld != NULL )
		{
			GWorld = OldWorld;
		}
	}
}

static bool ObjectBaseIsA( const UObjectBase *ObjectBase, const UClass* SomeBase )
{
	if ( SomeBase==NULL )
	{
		return true;
	}
	for ( UClass* TempClass=ObjectBase->GetClass(); TempClass; TempClass=TempClass->GetSuperClass() )
	{
		if ( TempClass == SomeBase )
		{
			return true;
		}
	}
	return false;
}

void FLiveEditorManager::FPieObjectCache::OnBeginPIE()
{
	TrackedObjects.Empty();
	ObjectLookupCache.Empty();
	PendingObjectCreations.Empty();
	bCacheActive = true;
}

void FLiveEditorManager::FPieObjectCache::OnEndPIE()
{
	TrackedObjects.Empty();
	ObjectLookupCache.Empty();
	PendingObjectCreations.Empty();
	bCacheActive = false;
}

bool FLiveEditorManager::FPieObjectCache::CacheStarted() const
{
	return (bCacheActive && ObjectLookupCache.Num() > 0);
}

void FLiveEditorManager::FPieObjectCache::OnObjectCreation( const class UObjectBase* ObjectBase )
{
	if ( !CacheStarted() || GEngine == NULL )
		return;

	PendingObjectCreations.Add(ObjectBase);
}

void FLiveEditorManager::FPieObjectCache::EvaluatePendingCreations()
{
	if ( !CacheStarted() || ObjectLookupCache.Num() <= 0 || GEngine == NULL )
		return;

	for ( TArray< const UObjectBase* >::TIterator PendingObjIt(PendingObjectCreations); PendingObjIt; ++PendingObjIt )
	{
		const UObjectBase *ObjectBase = (*PendingObjIt);
		if ( ObjectBase != NULL )
		{
			if ( !ObjectBaseIsA(ObjectBase, UObject::StaticClass()) || (ObjectBase->GetFlags() & RF_ClassDefaultObject) != 0 )
				continue;
			UObject *AsObject = const_cast<UObject*>( static_cast<const UObject*>(ObjectBase) );
	
			//UWorld *World = GEngine->GetWorldFromContextObject( AsObject, false );
			//if ( World == NULL || World->WorldType != EWorldType::PIE )
			//	continue;

			for ( TMultiMap< const UObject*, TWeakObjectPtr<UObject> >::TConstIterator ObjIt(ObjectLookupCache); ObjIt; ++ObjIt )
			{
				const UObject *Key = (*ObjIt).Key;
				if ( !nPieObjectCache::IsPiePartner(*Key, *AsObject) )
					continue;

				ObjectLookupCache.AddUnique( Key, AsObject );
				TrackedObjects.Add( AsObject );
			}
		}
	}
	PendingObjectCreations.Empty();
}

void FLiveEditorManager::FPieObjectCache::OnObjectDeletion( const class UObjectBase* ObjectBase )
{
	if ( !bCacheActive )
		return;

	UObject *AsObject = const_cast<UObject*>( static_cast<const UObject*>(ObjectBase) );

	//if deleted object was a value, remove specific associations
	TArray<const UObject*>::TConstIterator It(TrackedObjects);
	for ( ; It; It++ )
	{
		ObjectLookupCache.Remove( *It, AsObject );
	}

	//if deleted object was a Key, remove all associations
	ObjectLookupCache.Remove( AsObject );
	TrackedObjects.Remove( AsObject );

	for ( int32 i = PendingObjectCreations.Num()-1; i >= 0; --i )
	{
		if ( PendingObjectCreations[i] == ObjectBase )
		{
			PendingObjectCreations.RemoveAt(i);
		}
	}
}



/**
 * 
 * FLiveEditorManager
 *
 **/

struct FLiveEditorManager_Dispatch_Parms
{
	int Delta;
	int32 MidiValue;
	TEnumAsByte<ELiveEditControllerType::Type> ControlType;
};

FLiveEditorManager::FLiveEditorManager()
:	bInitialized(false),
	MIDIBuffer(NULL),
	ActiveWizard(NULL),
	LiveEditorDeviceSetupWizard(NULL),
	LiveEditorBlueprintBindingWizard(NULL),
	LiveEditorUserData(NULL),
	LiveEditorWorld(NULL),
	RealWorld(NULL),
	LastUpdateTime(0.0f)
{
}

void FLiveEditorManager::Initialize()
{
	PmError Error = Pm_Initialize();
	if ( Error != pmNoError )
		return;

	FindDevices();
	MIDIBuffer = new PmEvent[DEFAULT_BUFFER_SIZE];

	GConfig->GetArray(TEXT("LiveEditor"), TEXT("ActiveBlueprintTemplates"), ActiveBlueprintTemplates, GEditorPerProjectIni);
	for ( int32 i = ActiveBlueprintTemplates.Num()-1; i >= 0; --i )
	{
		FString &Name = ActiveBlueprintTemplates[i];
		if ( !FPackageName::DoesPackageExist(*Name) )
		{
			ActiveBlueprintTemplates.RemoveAt(i);
			GConfig->SetArray(TEXT("LiveEditor"), TEXT("ActiveBlueprintTemplates"), ActiveBlueprintTemplates, GEditorPerProjectIni);
			GConfig->Flush(false, GEditorPerProjectIni);
		}
	}

	LiveEditorUserData = new FLiveEditorUserData();

	bInitialized = true;
}

static FLiveEditorManager *LiveEditorManager = NULL;
FLiveEditorManager &FLiveEditorManager::Get()
{
	if ( LiveEditorManager == NULL )
	{
		LiveEditorManager = new FLiveEditorManager();
	}
	return *LiveEditorManager;
}
void FLiveEditorManager::Shutdown()
{
	if ( LiveEditorManager != NULL )
	{
		delete LiveEditorManager;
		LiveEditorManager = NULL;
	}
}

bool FLiveEditorManager::ConnectToRemoteHost( FString IPAddress )
{
	IPAddress.Replace( TEXT(" "), TEXT("") );

	TArray<FString> Parts;
	IPAddress.ParseIntoArray( Parts, TEXT("."), true );
	if ( Parts.Num() != 4 )
		return false;

	uint8 NumericParts[4];
	for ( int32 i = 0; i < 4; ++i )
	{
		NumericParts[i] = FCString::Atoi( *Parts[i] );
	}

	FSocket* Socket = FTcpSocketBuilder(TEXT("FLiveEditorManager.RemoteConnection"));
	FIPv4Endpoint Endpoint(FIPv4Address(NumericParts[0], NumericParts[1], NumericParts[2], NumericParts[3]), LIVEEDITORLISTENSERVER_DEFAULT_PORT);
	if ( Socket->Connect(*Endpoint.ToInternetAddr()) )
	{
		RemoteConnections.Add(IPAddress, Socket);
		return true;
	}
	else
	{
		return false;
	}
}

bool FLiveEditorManager::DisconnectFromRemoteHost( FString IPAddress )
{
	IPAddress.Replace( TEXT(" "), TEXT("") );

	FSocket **hSocket = RemoteConnections.Find( IPAddress );
	if ( hSocket == NULL )
		return false;

	bool RetVal = (*hSocket)->Close();
	RemoteConnections.Remove( IPAddress );
	return RetVal;
}

FLiveEditorManager::~FLiveEditorManager()
{
	if ( !bInitialized )
		return;

	if ( LiveEditorDeviceSetupWizard != NULL )
	{
		delete LiveEditorDeviceSetupWizard;
		LiveEditorDeviceSetupWizard = NULL;
	}

	bInitialized = false;

	CloseDeviceConnections();
	Pm_Terminate();

	delete[] MIDIBuffer;
	MIDIBuffer = NULL;

	delete LiveEditorUserData;
	LiveEditorUserData = NULL;
}

void FLiveEditorManager::FindDevices()
{
	//find all our devices
	int NumDevices = Pm_CountDevices(); //needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	for ( int i = 0; i < NumDevices; i++ )
	{
		const PmDeviceInfo *Info = Pm_GetDeviceInfo( i );
		if ( Info->input && !InputConnections.Find(i) )
		{
			PortMidiStream *MIDIStream;
			PmError Error = Pm_OpenInput( &MIDIStream, i, NULL, DEFAULT_BUFFER_SIZE, NULL, NULL );
			if ( Error == pmNoError )
			{
				FLiveEditorDeviceInstance DeviceInstance;
				DeviceInstance.Data.DeviceName = FString(Info->name);

				DeviceInstance.Connection.MIDIStream = MIDIStream;

				LoadDeviceData( DeviceInstance.Data.DeviceName, DeviceInstance.Data );

				InputConnections.Add( i, DeviceInstance );
			}
		}
	}
}

void FLiveEditorManager::CloseDeviceConnections()
{
	for( TMap< PmDeviceID, FLiveEditorDeviceInstance >::TIterator It(InputConnections); It; ++It )
	{
		FLiveEditorDeviceInstance Instance = (*It).Value;
		if ( Instance.Connection.MIDIStream )
		{
			Pm_Close( Instance.Connection.MIDIStream );
		}
	}
	InputConnections.Empty();
}

void FLiveEditorManager::RefreshDeviceConnections()
{
	CloseDeviceConnections();
	Pm_Terminate();

	PmError Error = Pm_Initialize();
	if ( Error != pmNoError )
	{
		bInitialized = false;
		return;
	}

	FindDevices();
}

//
// LISTENER REGISTRATION
//

void FLiveEditorManager::RegisterEventListener( UObject *Target, FName EventName )
{
	if ( !Target )
		return;

	UFunction *Function = Target->FindFunction( EventName );
	if ( Function == NULL )
		return;

	ULiveEditorBlueprint *Blueprint = Cast<ULiveEditorBlueprint>(Target);
	check( Blueprint != NULL );
	
	FString BlueprintName = Blueprint->GetName();
	int32 chopIndex = BlueprintName.Find( TEXT("_C"), ESearchCase::CaseSensitive, ESearchDir::FromEnd );
	if ( chopIndex != INDEX_NONE )
	{
		BlueprintName = BlueprintName.LeftChop( BlueprintName.Len() - chopIndex );
	}

	const FLiveEditBinding* Binding = LiveEditorUserData->GetBinding( BlueprintName, EventName.ToString() );
	if ( Binding != NULL )
	{
		ILiveEditListener *Listener = new ILiveEditListener(EventName, *Binding, Target);
		int ID = LISTENER_ID(Listener->Binding.Channel,Listener->Binding.ControlID); //needs to remain int (instead of int32) since numbers are derived from TPL that uses int
		EventListeners.Add(ID,Listener);
	}
	else
	{
		FString Message = FString::Printf( TEXT("A LiveEditor Event (%s) is not bound and will not work. Please Re-bind this LiveEditorBlueprint in the LiveEditor Control Panel"), *EventName.ToString() );
		FPlatformMisc::MessageBoxExt( EAppMsgType::Ok, *Message, *NSLOCTEXT("MessageDialog", "DefaultDebugMessageTitle", "ShowDebugMessagef").ToString() );
	}
}

void FLiveEditorManager::UnregisterListener( UObject *Target, FName VarName )
{
	for ( TMultiMap<int,ILiveEditListener*>::TIterator It(EventListeners); It; ++It )
	{
		ILiveEditListener *Value = It.Value();
		if ( Value->Target == Target && Value->Name == VarName )
		{
			delete Value;
			It.RemoveCurrent();
		}
	}
}

void FLiveEditorManager::UnregisterAllListeners( UObject *Target )
{
	for ( TMultiMap<int,ILiveEditListener*>::TIterator It(EventListeners); It; ++It )
	{
		ILiveEditListener *Value = It.Value();
		if ( Value->Target == Target )
		{
			delete Value;
			It.RemoveCurrent();
		}
	}
}


//
// UI INTERACTION WITH CONFIG WINDOW
//

void FLiveEditorManager::SelectLiveEditorBlueprint( UBlueprint *Blueprint )
{
	if(	Blueprint != NULL
		&& Blueprint->GeneratedClass != NULL
		&& Blueprint->GeneratedClass->IsChildOf( ULiveEditorBlueprint::StaticClass() ) )
	{
		FString BlueprintName = Blueprint->GeneratedClass->GetPathName();
		if ( BlueprintName.RightChop( BlueprintName.Len() - 2 ) == FString( TEXT("_C") ) )
			BlueprintName = BlueprintName.LeftChop(2);
		UE_LOG( LiveEditorManagerLog, Log, TEXT("%s"), *BlueprintName );
		ActiveBlueprintTemplates.AddUnique( BlueprintName );
		GConfig->SetArray(TEXT("LiveEditor"), TEXT("ActiveBlueprintTemplates"), ActiveBlueprintTemplates, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}
}

bool FLiveEditorManager::CheckActive( const FString &Name )
{
	return FindActiveBlueprint( Name ) != NULL;
}

class ULiveEditorBlueprint *FLiveEditorManager::FindActiveBlueprint( const FString &Name )
{
	FString CheckName = Name.Left( Name.Find( TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd ) );
	for ( TArray< FActiveBlueprintRecord >::TIterator It(ActiveBlueprints); It; It++ )
	{
		ULiveEditorBlueprint *BP = (*It).Blueprint.Get();
		if ( !BP )
			continue;

		FString CheckPathName = BP->GetArchetype()->GetPathName();
		CheckPathName = CheckPathName.Left( CheckPathName.Find( TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd ) );

		UE_LOG( LiveEditorManagerLog, Log, TEXT("%s | %s"), *CheckPathName, *CheckName );
		if ( CheckPathName == CheckName )
			return BP;
	}

	return NULL;
}

bool FLiveEditorManager::Activate( const FString &Name )
{
	RealWorld = GWorld;
	check( LiveEditorWorld != NULL );
	GWorld = LiveEditorWorld;

	bool bSuccess = false;

	UBlueprint *Blueprint = LoadObject<UBlueprint>( NULL, *Name, NULL, 0, NULL );
	if(	Blueprint != NULL
		&& Blueprint->GeneratedClass != NULL
		&& Blueprint->GeneratedClass->IsChildOf( ULiveEditorBlueprint::StaticClass() ) )
	{
		FActiveBlueprintRecord Record;
		Record.Name = Name;

		auto Instance = NewObject<ULiveEditorBlueprint>(GetTransientPackage(), Blueprint->GeneratedClass, NAME_None, RF_Transient | RF_Public | RF_RootSet | RF_Standalone);
		Instance->DoInit();
		Record.Blueprint = Instance;

		ActiveBlueprints.Add( Record );
		bSuccess = true;
	}

	GWorld = RealWorld;
	RealWorld = NULL;

	return bSuccess;
}

bool FLiveEditorManager::DeActivate( const FString &Name )
{
	RealWorld = GWorld;
	check( LiveEditorWorld != NULL );
	GWorld = LiveEditorWorld;

	bool bSuccess = false;

	ULiveEditorBlueprint *BP = FindActiveBlueprint(Name);
	if ( BP != NULL )
	{
		UnregisterAllListeners(BP);
		BP->OnShutdown();
		//BP->MarkPendingKill(); TODO how do we delete this without crashing?
	
		int32 i = 0;
		while( i < ActiveBlueprints.Num() )
		{
			if ( ActiveBlueprints[i].Blueprint.Get() == BP )
			{
				ActiveBlueprints.RemoveAtSwap(i);
				continue;
			}
			else
			{
				++i;
			}
		}

		bSuccess = true;
	}

	GWorld = RealWorld;
	RealWorld = NULL;

	return bSuccess;
}

bool FLiveEditorManager::Remove( const FString &Name )
{
	DeActivate( Name );
	
	ActiveBlueprintTemplates.Remove( Name );
	GConfig->SetArray(TEXT("LiveEditor"), TEXT("ActiveBlueprintTemplates"), ActiveBlueprintTemplates, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
	return true;
}


//
// UPDATE
//

void FLiveEditorManager::InjectNewBlueprintEditor( const TWeakPtr<FBlueprintEditor> &InEditor )
{
	FBlueprintEditor* Editor = InEditor.Pin().Get();
	if ( !Editor )
		return;
	UBlueprint *Blueprint = Editor->GetBlueprintObj();
	if(	Blueprint != NULL
		&& Blueprint->GeneratedClass != NULL
		&& Blueprint->GeneratedClass->IsChildOf( ULiveEditorBlueprint::StaticClass() ) )
	{
		ActiveBlueprintEditors.Add( InEditor.Pin() );
	}
}

void FLiveEditorManager::CreateLiveEditorWorld()
{
	LiveEditorWorld = NewObject<UWorld>();
	LiveEditorWorld->SetFlags(RF_Transactional);
	LiveEditorWorld->WorldType = EWorldType::Preview;
	
	bool bTransactional = false;
	if (!bTransactional)
	{
		LiveEditorWorld->ClearFlags(RF_Transactional);
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Preview);
	WorldContext.SetCurrentWorld(LiveEditorWorld);

	LiveEditorWorld->InitializeNewWorld(UWorld::InitializationValues()
										.AllowAudioPlayback(true)
										.CreatePhysicsScene(false)
										.RequiresHitProxies(false)
										.CreateNavigation(false)
										.CreateAISystem(false)
										.ShouldSimulatePhysics(false)
										.SetTransactional(bTransactional));
	LiveEditorWorld->InitializeActorsForPlay(FURL());
}


void FLiveEditorManager::Tick(float DeltaTime)
{
	//avoid multiple tick DOOM ( FTickableGameObject's get ticked once per UWorld that is active and Ticking )
	float CurTime = GWorld->GetRealTimeSeconds();
	if ( LastUpdateTime == CurTime )
	{
		return;
	}
	LastUpdateTime = CurTime;

	if ( LiveEditorWorld == NULL )
	{
		CreateLiveEditorWorld();
	}

	RealWorld = GWorld;
	check( LiveEditorWorld != NULL );
	GWorld = LiveEditorWorld;

	if ( ActiveBlueprints.Num() > 0 )
		LiveEditorWorld->Tick( ELevelTick::LEVELTICK_All, DeltaTime );

	//
	//update our ActiveBlueprints
	//
	int32 i = 0;
	while ( i < ActiveBlueprints.Num() )
	{
		FActiveBlueprintRecord record = ActiveBlueprints[i];
		ULiveEditorBlueprint *Instance = record.Blueprint.Get();
		if ( Instance == NULL )
		{
			ActiveBlueprints.RemoveAtSwap(i);	//clean out the dead entry
			Activate( record.Name );			//try to ressurect the Blueprint (user has likely just recompiled it)
			continue;
		}
		Instance->Tick( DeltaTime );
		++i;
	}

	//
	// handle the actual MIDI messages
	//
	for( TMap< PmDeviceID, FLiveEditorDeviceInstance >::TIterator It(InputConnections); It; ++It )
	{
		PmDeviceID DeviceID = (*It).Key;
		FLiveEditorDeviceInstance &DeviceInstance = (*It).Value;
		int NumRead = Pm_Read( DeviceInstance.Connection.MIDIStream, MIDIBuffer, DEFAULT_BUFFER_SIZE ); //needs to remain int (instead of int32) since numbers are derived from TPL that uses int
		if ( NumRead < 0 )
		{
			continue; //error occurred, portmidi should handle this silently behind the scenes
		}
		else if ( NumRead > 0 )
		{
			DeviceInstance.Data.LastInputTime = GWorld->GetTimeSeconds();
		}

		//dispatch
		for ( int32 BufferIndex = 0; BufferIndex < NumRead; BufferIndex++ )
		{
			PmMessage Msg = MIDIBuffer[BufferIndex].message;
			int Status = Pm_MessageStatus(Msg);	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
			int Data1 = Pm_MessageData1(Msg);	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
			int Data2 = Pm_MessageData2(Msg);	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int

			if ( ActiveWizard != NULL )
			{
				ActiveWizard->ProcessMIDI( Status, Data1, Data2, DeviceID, DeviceInstance.Data );
			}
			else
			{
				switch ( DeviceInstance.Data.ConfigState )
				{
					case FLiveEditorDeviceData::UNCONFIGURED:
						break;
					case FLiveEditorDeviceData::CONFIGURED:
						Dispatch( Status, Data1, Data2, DeviceInstance.Data );
						break;
					default:
						break;
				}
			}
		}
	}

	PieObjectCache.EvaluatePendingCreations();

	GWorld = RealWorld;
	RealWorld = NULL;
}

//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
void FLiveEditorManager::Dispatch( int Status, int Data1, int Data2, const struct FLiveEditorDeviceData &Data )
{
	int Type = (Status & 0xF0) >> 4;
	int Channel = (Status % 16) + 1;
	int ControlID = Data1;
	int ID = LISTENER_ID(Channel,ControlID);
	
	int Delta = 0;	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int

	//
	// Dispatch the MIDI data to all Active Listners
	//
	TArray<ILiveEditListener*> Results;
	EventListeners.MultiFind( ID, Results );
	for ( TArray<ILiveEditListener*>::TIterator It(Results); It; It++ )
	{
		ILiveEditListener *Listener = *It;
		if ( !Listener || !Listener->Target.IsValid() )
		{
			continue;
		}

		switch ( Listener->Binding.ControlType )
		{
			case ELiveEditControllerType::NoteOnOff:
				Delta = 0;
				break;
			case ELiveEditControllerType::ControlChangeContinuous:
				if ( Data2 == Data.ContinuousIncrement )
					Delta = 1;
				else if ( Data2 == Data.ContinuousDecrement )
					Delta = -1;
				else
					Delta = 0;
				break;
			case ELiveEditControllerType::ControlChangeFixed:
				Delta = Data2 - Listener->Binding.LastValue;
				Listener->Binding.LastValue = Data2;
			default:
				break;
		}

		UObject *Target = Listener->Target.Get();
		FLiveEditorManager_Dispatch_Parms Parms;
		Parms.Delta = Delta;
		Parms.MidiValue = (int32)Data2;
		Parms.ControlType = Listener->Binding.ControlType;
		Target->ProcessEvent( Target->FindFunctionChecked( Listener->Name ), &Parms );
	}
}

void FLiveEditorManager::OnBeginPIE()
{
	PieObjectCache.OnBeginPIE();
}

void FLiveEditorManager::OnEndPIE()
{
	PieObjectCache.OnEndPIE();
}

void FLiveEditorManager::OnObjectCreation( const UObjectBase *NewObject )
{
	PieObjectCache.OnObjectCreation( NewObject );
}

void FLiveEditorManager::OnObjectDeletion( const class UObjectBase *NewObject )
{
	PieObjectCache.OnObjectDeletion( NewObject );
}

void FLiveEditorManager::FindPiePartners( const class UObject *Archetype, TArray< TWeakObjectPtr<UObject> > &OutValues )
{
	UWorld *OldWorld = GWorld;
	GWorld = RealWorld;
	check( GWorld != NULL );

	PieObjectCache.FindPiePartners( Archetype, OutValues );

	GWorld = OldWorld;
}

void FLiveEditorManager::BroadcastValueUpdate( const FString &ClassName, const FString &PropertyName, const FString &PropertyValue )
{
	// remove closed connections
	for ( TMap<FString, class FSocket *>::TIterator ConnectionIt(RemoteConnections); ConnectionIt; ++ConnectionIt )
	{
		FSocket *Connection = (*ConnectionIt).Value;
		if ( Connection->GetConnectionState() != SCS_Connected )
		{
			RemoteConnections.Remove( (*ConnectionIt).Key );
		}
	}

	//no one to talk to, exit
	if ( RemoteConnections.Num() == 0 )
	{
		return;
	}

	nLiveEditorListenServer::FNetworkMessage Message;
	Message.Type = nLiveEditorListenServer::CLASSDEFAULT_OBJECT_PROPERTY;
	Message.Payload.ClassName = ClassName;
	Message.Payload.PropertyName = PropertyName;
	Message.Payload.PropertyValue = PropertyValue;

	TSharedPtr<FArrayWriter> Datagram = MakeShareable(new FArrayWriter(true));
	*Datagram << Message;

	for ( TMap<FString, class FSocket *>::TIterator ConnectionIt(RemoteConnections); ConnectionIt; ++ConnectionIt )
	{
		FSocket *Connection = (*ConnectionIt).Value;
		int32 BytesSent = 0;
		Connection->Send( Datagram->GetData(), Datagram->Num(), BytesSent );
	}
}

bool FLiveEditorManager::GetDeviceIDs( TArray< TSharedPtr<PmDeviceID> > &DeviceIDs ) const
{
	if ( InputConnections.Num() == 0 )
	{
		return false;
	}

	for( TMap< PmDeviceID, FLiveEditorDeviceInstance >::TConstIterator It(InputConnections); It; ++It )
	{
		DeviceIDs.Add( MakeShareable( new PmDeviceID((*It).Key)) );
	}
	return true;
}

bool FLiveEditorManager::GetDeviceData( PmDeviceID DeviceID, FLiveEditorDeviceData &DeviceData ) const
{
	const FLiveEditorDeviceInstance *DeviceInstance = InputConnections.Find(DeviceID);
	if ( DeviceInstance == NULL )
	{
		return false;
	}

	DeviceData = DeviceInstance->Data;
	return true;
}

void FLiveEditorManager::ConfigureDevice( PmDeviceID DeviceID )
{
	FLiveEditorDeviceInstance *DeviceInstance = InputConnections.Find(DeviceID);
	if ( DeviceInstance == NULL )
	{
		return;
	}

	//create out wizard if we need it and then start it
	if ( LiveEditorDeviceSetupWizard == NULL )
	{
		LiveEditorDeviceSetupWizard = new FLiveEditorDeviceSetupWizard();
	}
	LiveEditorDeviceSetupWizard->Start( DeviceID, DeviceInstance->Data );

	//make our Wizard overlay window active and bind it to the DeviceSetup Wizard
	SetActiveWizard(LiveEditorDeviceSetupWizard);
}

bool FLiveEditorManager::BlueprintHasBinding( const class UBlueprint &Blueprint )
{
	return LiveEditorUserData->HasBlueprintBindings( Blueprint );
}

void FLiveEditorManager::StartBlueprintBinding( UBlueprint &Blueprint )
{
	if ( LiveEditorBlueprintBindingWizard == NULL )
	{
		LiveEditorBlueprintBindingWizard = new FLiveEditorBlueprintBindingWizard();
	}
	LiveEditorBlueprintBindingWizard->Start( Blueprint );

	//make our Wizard overlay window active and bind it to the DeviceSetup Wizard
	SetActiveWizard(LiveEditorBlueprintBindingWizard);
}

void FLiveEditorManager::FinishBlueprintBinding( UBlueprint &Blueprint )
{
	if ( Blueprint.GeneratedClass == NULL )
	{
		if(FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformMisc::DebugBreak();
		}
		return;
	}

	FString BlueprintName = Blueprint.GeneratedClass->GetPathName();
	if ( BlueprintName.RightChop( BlueprintName.Len() - 2 ) == FString( TEXT("_C") ) )
		BlueprintName = BlueprintName.LeftChop(2);
	if ( CheckActive( BlueprintName ) )
	{
		DeActivate( BlueprintName );
		Activate( BlueprintName );
	}
}

void FLiveEditorManager::SetActiveWizard( class FLiveEditorWizardBase *Wizard )
{
	check( ActiveWizard == NULL || Wizard == NULL );
	ActiveWizard = Wizard;
}

bool FLiveEditorManager::SaveDeviceData( const FLiveEditorDeviceData &DeviceData )
{
	if ( DeviceData.ConfigState != FLiveEditorDeviceData::CONFIGURED )
	{
		return false;
	}

	FString SectionName = FString::Printf( TEXT("LiveEditor.%s"), *DeviceData.DeviceName );
	GConfig->SetInt( *SectionName, TEXT("ButtonSignalDown"), DeviceData.ButtonSignalDown, GEditorPerProjectIni );
	GConfig->SetInt( *SectionName, TEXT("ButtonSignalUp"), DeviceData.ButtonSignalUp, GEditorPerProjectIni );
	GConfig->SetInt( *SectionName, TEXT("ContinuousIncrement"), DeviceData.ContinuousIncrement, GEditorPerProjectIni );
	GConfig->SetInt( *SectionName, TEXT("ContinuousDecrement"), DeviceData.ContinuousDecrement, GEditorPerProjectIni );

	return true;
}

bool FLiveEditorManager::LoadDeviceData( const FString &DeviceName, FLiveEditorDeviceData &DeviceData )
{
	FString SectionName = FString::Printf( TEXT("LiveEditor.%s"), *DeviceName );
	TArray< FString > Section; 
	if ( !GConfig->GetSection( *SectionName, Section, GEditorPerProjectIni ) )
	{
		return false;
	}

	DeviceData.DeviceName = DeviceName;
	DeviceData.ConfigState = FLiveEditorDeviceData::CONFIGURED;
	GConfig->GetInt( *SectionName, TEXT("ButtonSignalDown"), DeviceData.ButtonSignalDown, GEditorPerProjectIni );
	GConfig->GetInt( *SectionName, TEXT("ButtonSignalUp"), DeviceData.ButtonSignalUp, GEditorPerProjectIni );
	GConfig->GetInt( *SectionName, TEXT("ContinuousIncrement"), DeviceData.ContinuousIncrement, GEditorPerProjectIni );
	GConfig->GetInt( *SectionName, TEXT("ContinuousDecrement"), DeviceData.ContinuousDecrement, GEditorPerProjectIni );

	return true;
}
