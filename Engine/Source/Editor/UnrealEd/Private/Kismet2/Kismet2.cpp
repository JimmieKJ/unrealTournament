// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "Engine/LevelScriptBlueprint.h"
#include "BlueprintUtilities.h"
#include "AnimGraphDefinitions.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Toolkits/AssetEditorManager.h"
#include "Editor/Kismet/Public/BlueprintEditorModule.h"
#include "Editor/Kismet/Public/FindInBlueprintManager.h"
#include "Toolkits/ToolkitManager.h"
#include "Editor/KismetCompiler/Public/KismetCompilerModule.h"
#include "Kismet2/CompilerResultsLog.h"
#include "AssetSelection.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "Layers/Layers.h"
#include "ScopedTransaction.h"
#include "AssetToolsModule.h"
#include "EngineAnalytics.h"
#include "IAnalyticsProvider.h"
#include "MessageLog.h"
#include "StructureEditorUtils.h"
#include "ActorEditorUtils.h"
#include "ObjectEditorUtils.h"
#include "DlgPickAssetPath.h"
#include "ComponentAssetBroker.h"
#include "BlueprintEditorSettings.h"
#include "Editor/UnrealEd/Classes/Editor/Transactor.h"
#include "Editor/UnrealEd/Public/PackageTools.h"
#include "NotificationManager.h"
#include "SNotificationList.h" // for FNotificationInfo

#define LOCTEXT_NAMESPACE "UnrealEd.Editor"

//////////////////////////////////////////////////////////////////////////
// FArchiveInvalidateTransientRefs

/**
 * Archive built to go through and find any references to objects in the transient package, and then NULL those references
 */
class FArchiveInvalidateTransientRefs : public FArchiveUObject
{
public:
	FArchiveInvalidateTransientRefs()
	{
		ArIsObjectReferenceCollector = true;
		ArIsPersistent = false;
		ArIgnoreArchetypeRef = false;
	}
protected:
	/** 
	 * UObject serialize operator implementation
	 *
	 * @param Object	reference to Object reference
	 * @return reference to instance of this class
	 */
	FArchive& operator<<( UObject*& Object )
	{
		// Check if this is a reference to an object existing in the transient package, and if so, NULL it.
		if ((Object != NULL) && (Object->GetOutermost() == GetTransientPackage()) )
		{
			check( Object->IsValidLowLevel() );
			Object = NULL;
		}

		return *this;
	}
};


//////////////////////////////////////////////////////////////////////////
// FBlueprintObjectsBeingDebuggedIterator

FBlueprintObjectsBeingDebuggedIterator::FBlueprintObjectsBeingDebuggedIterator(UBlueprint* InBlueprint)
	: Blueprint(InBlueprint)
{
}

UObject* FBlueprintObjectsBeingDebuggedIterator::operator* () const
{
	return Blueprint->GetObjectBeingDebugged();
}

UObject* FBlueprintObjectsBeingDebuggedIterator::operator-> () const
{
	return Blueprint->GetObjectBeingDebugged();
}

FBlueprintObjectsBeingDebuggedIterator& FBlueprintObjectsBeingDebuggedIterator::operator++()
{
	Blueprint = NULL;
	return *this;
}

bool FBlueprintObjectsBeingDebuggedIterator::IsValid() const
{
	return Blueprint != NULL;
}



//////////////////////////////////////////////////////////////////////////
// FObjectsBeingDebuggedIterator

FObjectsBeingDebuggedIterator::FObjectsBeingDebuggedIterator()
	: SelectedActorsIter(*GEditor->GetSelectedActors())
	, LevelScriptActorIndex(INDEX_NONE)
{
	FindNextLevelScriptActor();
}

UWorld* FObjectsBeingDebuggedIterator::GetWorld() const
{
	return (GEditor->PlayWorld != NULL) ? GEditor->PlayWorld : GWorld;
}

UObject* FObjectsBeingDebuggedIterator::operator* () const
{
	return SelectedActorsIter ? *SelectedActorsIter : (UObject*)(GetWorld()->GetLevel(LevelScriptActorIndex)->GetLevelScriptActor());
}

UObject* FObjectsBeingDebuggedIterator::operator-> () const
{
	return SelectedActorsIter ? *SelectedActorsIter : (UObject*)(GetWorld()->GetLevel(LevelScriptActorIndex)->GetLevelScriptActor());
}

FObjectsBeingDebuggedIterator& FObjectsBeingDebuggedIterator::operator++()
{
	if (SelectedActorsIter)
	{
		++SelectedActorsIter;
	}
	else
	{
		FindNextLevelScriptActor();
	}

	return *this;
}

bool FObjectsBeingDebuggedIterator::IsValid() const
{
	return SelectedActorsIter || (LevelScriptActorIndex < GetWorld()->GetNumLevels());
}

void FObjectsBeingDebuggedIterator::FindNextLevelScriptActor()
{
	while (++LevelScriptActorIndex < GetWorld()->GetNumLevels())
	{
		ULevel* Level = GetWorld()->GetLevel(LevelScriptActorIndex);
		if ((Level != NULL) && (Level->GetLevelScriptActor() != NULL))
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FBlueprintUnloader

/** Utility struct, used to aid in unloading and replacing a specific blueprint. */
struct FBlueprintUnloader
{
public:
	FBlueprintUnloader(UBlueprint* OldBlueprint);

	/** 
	 * Unloads the specified Blueprint (marking it pending-kill, and removing it 
	 * from its outer package). Optionally, will unload the package as well.
	 *
	 * @param  bResetPackage	Whether or not this should unload the entire package.
	 */
	void UnloadBlueprint(const bool bResetPackage);
	
	/** 
	 * Replaces all old references to the original blueprints (its class/CDO/etc.)
	 * @param  NewBlueprint	The blueprint to replace old references with
	 */
	void ReplaceStaleRefs(UBlueprint* NewBlueprint);

private:
	TWeakObjectPtr<UBlueprint> OldBlueprint;
	UClass*  OldGeneratedClass;
	UObject* OldCDO;
	UClass*  OldSkeletonClass;
	UObject* OldSkelCDO;
};

FBlueprintUnloader::FBlueprintUnloader(UBlueprint* OldBlueprintIn)
	: OldBlueprint(OldBlueprintIn)
	, OldGeneratedClass(OldBlueprint->GeneratedClass)
	, OldCDO(nullptr)
	, OldSkeletonClass(OldBlueprint->SkeletonGeneratedClass)
	, OldSkelCDO(nullptr)
{
	if (OldGeneratedClass != nullptr)
	{
		OldCDO = OldGeneratedClass->GetDefaultObject(/*bCreateIfNeeded =*/false);
	}
	if (OldSkeletonClass != nullptr)
	{
		OldSkelCDO = OldSkeletonClass->GetDefaultObject(/*bCreateIfNeeded =*/false);
	}
	OldBlueprint = OldBlueprintIn;
}

void FBlueprintUnloader::UnloadBlueprint(const bool bResetPackage)
{
	if (OldBlueprint.IsValid())
	{
		UBlueprint* UnloadingBp = OldBlueprint.Get();

		UPackage* const OldPackage = Cast<UPackage>(UnloadingBp->GetOutermost());
		bool const bIsDirty = OldPackage->IsDirty();

		UPackage* const TransientPackage = GetTransientPackage();
		check(OldPackage != TransientPackage); // is the blueprint already unloaded?
		
		FName const BlueprintName = UnloadingBp->GetFName();
		// move the blueprint to the transient package (to be picked up by garbage collection later)
		FName UnloadedName = MakeUniqueObjectName(TransientPackage, UBlueprint::StaticClass(), BlueprintName);
		UnloadingBp->Rename(*UnloadedName.ToString(), TransientPackage, REN_DontCreateRedirectors | REN_DoNotDirty);
		// @TODO: currently, REN_DoNotDirty does not guarantee that the package 
		//        will not be marked dirty
		OldPackage->SetDirtyFlag(bIsDirty);

		// make sure the blueprint is properly trashed (remove it from the package)
		UnloadingBp->SetFlags(RF_Transient);
		UnloadingBp->ClearFlags(RF_Standalone | RF_RootSet | RF_Transactional);
		UnloadingBp->RemoveFromRoot();
		UnloadingBp->MarkPendingKill();
		// if it's in the undo buffer, then we have to clear that...
		if (FKismetEditorUtilities::IsReferencedByUndoBuffer(UnloadingBp))
		{
			GEditor->Trans->Reset(LOCTEXT("UnloadedBlueprint", "Unloaded Blueprint"));
		}

		if (bResetPackage)
		{
			TArray<UPackage*> PackagesToUnload;
			PackagesToUnload.Add(OldPackage);

			FText PackageUnloadError;
			PackageTools::UnloadPackages(PackagesToUnload, PackageUnloadError);

			if (!PackageUnloadError.IsEmpty())
			{
				const FText ErrorMessage = FText::Format(LOCTEXT("UnloadBpPackageError", "Failed to unload Bluprint '{0}': {1}"),
					FText::FromName(BlueprintName), PackageUnloadError);
				FSlateNotificationManager::Get().AddNotification(FNotificationInfo(ErrorMessage));

				// fallback to manually setting up the package so it can reload 
				// the blueprint 
				ResetLoaders(OldPackage);
				OldPackage->ClearFlags(RF_WasLoaded);
				OldPackage->bHasBeenFullyLoaded = false;
				OldPackage->GetMetaData()->RemoveMetaDataOutsidePackage();
			}
		}

		FKismetEditorUtilities::OnBlueprintUnloaded.Broadcast(UnloadingBp);

		// handled in FBlueprintEditor (from the OnBlueprintUnloaded event)
// 		IAssetEditorInstance* EditorInst = FAssetEditorManager::Get().FindEditorForAsset(UnloadingBp, /*bFocusIfOpen =*/false);
// 		if (EditorInst != nullptr)
// 		{
// 			EditorInst->CloseWindow();
// 		}
	}
}

void FBlueprintUnloader::ReplaceStaleRefs(UBlueprint* NewBlueprint)
{
	//--------------------------------------
	// Construct redirects
	//--------------------------------------

	TMap<UObject*, UObject*> Redirects;
	TArray<UObject*> OldObjsNeedingReplacing;

	if (OldBlueprint.IsValid(/*bEvenIfPendingKill =*/true))
	{
		UBlueprint* ToBeReplaced = OldBlueprint.Get(/*bEvenIfPendingKill =*/true);
		if (OldGeneratedClass != nullptr)
		{
			OldObjsNeedingReplacing.Add(OldGeneratedClass);
			Redirects.Add(OldGeneratedClass, NewBlueprint->GeneratedClass);
		}
		if (OldCDO != nullptr)
		{
			OldObjsNeedingReplacing.Add(OldCDO);
			Redirects.Add(OldCDO, NewBlueprint->GeneratedClass->GetDefaultObject());
		}
		if (OldSkeletonClass != nullptr)
		{
			OldObjsNeedingReplacing.Add(OldSkeletonClass);
			Redirects.Add(OldSkeletonClass, NewBlueprint->SkeletonGeneratedClass);
		}
		if (OldSkelCDO != nullptr)
		{
			OldObjsNeedingReplacing.Add(OldSkelCDO);
			Redirects.Add(OldSkelCDO, NewBlueprint->SkeletonGeneratedClass->GetDefaultObject());
		}

		OldObjsNeedingReplacing.Add(ToBeReplaced);
		Redirects.Add(ToBeReplaced, NewBlueprint);

		// clear the object being debugged; otherwise ReplaceInstancesOfClass()  
		// trys to reset it with a new level instance, and OldBlueprint won't 
		// match the new instance's type (it's now a NewBlueprint)
		ToBeReplaced->SetObjectBeingDebugged(nullptr);
	}

	//--------------------------------------
	// Replace old references
	//--------------------------------------

	TArray<UObject*> Referencers;
	// find all objects, still referencing the old blueprint/class/cdo/etc.
	for (auto Referencer : TFindObjectReferencers<UObject>(OldObjsNeedingReplacing, /*PackageToCheck =*/nullptr, /*bIgnoreTemplates =*/false))
	{
		Referencers.Add(Referencer.Value);
	}

	FBlueprintCompileReinstancer::ReplaceInstancesOfClass(OldGeneratedClass, NewBlueprint->GeneratedClass);

	for (UObject* Referencer : Referencers)
	{
		FArchiveReplaceObjectRef<UObject>(Referencer, Redirects, /*bNullPrivateRefs=*/false, /*bIgnoreOuterRef=*/false, /*bIgnoreArchetypeRef=*/false);
	}
}

//////////////////////////////////////////////////////////////////////////

// Static variable definition
TArray<FString> FKismetEditorUtilities::TrackedBlueprintParentList;
FKismetEditorUtilities::FOnBlueprintUnloaded FKismetEditorUtilities::OnBlueprintUnloaded;

/** Create the correct event graphs for this blueprint */
void FKismetEditorUtilities::CreateDefaultEventGraphs(UBlueprint* Blueprint)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraph* Ubergraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, K2Schema->GN_EventGraph, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	Ubergraph->bAllowDeletion = false; //@TODO: Really, just want to make sure we never drop below 1, not that you cannot delete any particular one!
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Ubergraph);

	Blueprint->LastEditedDocuments.AddUnique(Ubergraph);
}

/** Create a new Blueprint and initialize it to a valid state. */
UBlueprint* FKismetEditorUtilities::CreateBlueprint(UClass* ParentClass, UObject* Outer, const FName NewBPName, EBlueprintType BlueprintType, TSubclassOf<UBlueprint> BlueprintClassType, TSubclassOf<UBlueprintGeneratedClass> BlueprintGeneratedClassType, FName CallingContext)
{
	check(FindObject<UBlueprint>(Outer, *NewBPName.ToString()) == NULL); 

	// Not all types are legal for all parent classes, if the parent class is const then the blueprint cannot be an ubergraph-bearing one
	if ((BlueprintType == BPTYPE_Normal) && (ParentClass->HasAnyClassFlags(CLASS_Const)))
	{
		BlueprintType = BPTYPE_Const;
	}
	
	// Create new UBlueprint object
	UBlueprint* NewBP = ConstructObject<UBlueprint>(*BlueprintClassType, Outer, NewBPName, RF_Public|RF_Standalone|RF_Transactional|RF_LoadCompleted);
	NewBP->Status = BS_BeingCreated;
	NewBP->BlueprintType = BlueprintType;
	NewBP->ParentClass = ParentClass;
	NewBP->BlueprintSystemVersion = UBlueprint::GetCurrentBlueprintSystemVersion();
	NewBP->bIsNewlyCreated = true;
	NewBP->bLegacyNeedToPurgeSkelRefs = false;
	NewBP->GenerateNewGuid();

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Create SimpleConstructionScript and UserConstructionScript
	if (FBlueprintEditorUtils::SupportsConstructionScript(NewBP))
	{ 
		// >>> Temporary workaround, before a BlueprintGeneratedClass is the main asset.
		FName NewSkelClassName, NewGenClassName;
		NewBP->GetBlueprintClassNames(NewGenClassName, NewSkelClassName);
		UBlueprintGeneratedClass* NewClass = ConstructObject<UBlueprintGeneratedClass>(
			*BlueprintGeneratedClassType, NewBP->GetOutermost(), NewGenClassName, RF_Public | RF_Transactional);
		NewBP->GeneratedClass = NewClass;
		NewClass->ClassGeneratedBy = NewBP;
		NewClass->SetSuperStruct(ParentClass);
		// <<< Temporary workaround

		NewBP->SimpleConstructionScript = NewObject<USimpleConstructionScript>(NewClass);
		NewBP->SimpleConstructionScript->SetFlags(RF_Transactional);
		NewBP->LastEditedDocuments.Add(NewBP->SimpleConstructionScript);

		UEdGraph* UCSGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, K2Schema->FN_UserConstructionScript, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph(NewBP, UCSGraph, /*bIsUserCreated=*/ false, AActor::StaticClass());

		// If the blueprint is derived from another blueprint, add in a super-call automatically
		if( NewBP->ParentClass && NewBP->ParentClass->ClassGeneratedBy )
		{
			check( UCSGraph->Nodes.Num() > 0 );
			UK2Node_FunctionEntry* UCSEntry = CastChecked<UK2Node_FunctionEntry>(UCSGraph->Nodes[0]);
			UK2Node_CallParentFunction* ParentCallNodeTemplate = NewObject<UK2Node_CallParentFunction>();
			ParentCallNodeTemplate->FunctionReference.SetExternalMember(K2Schema->FN_UserConstructionScript, NewBP->ParentClass);
			UK2Node_CallParentFunction* ParentCallNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_CallParentFunction>(UCSGraph, ParentCallNodeTemplate, FVector2D(200, 0));

			// Wire up the new node
			UEdGraphPin* ExecPin = UCSEntry->FindPin(K2Schema->PN_Then);
			UEdGraphPin* SuperPin = ParentCallNode->FindPin(K2Schema->PN_Execute);
			ExecPin->MakeLinkTo(SuperPin);
		}

		NewBP->LastEditedDocuments.Add(UCSGraph);
		UCSGraph->bAllowDeletion = false;
	}

	// Create default event graph(s)
	if (FBlueprintEditorUtils::DoesSupportEventGraphs(NewBP))
	{
		check(NewBP->UbergraphPages.Num() == 0);
		CreateDefaultEventGraphs(NewBP);
	}

	//@TODO: ANIMREFACTOR 1: This kind of code should be on a per-blueprint basis; not centralized here
	if (UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(NewBP))
	{
		UAnimBlueprint* RootAnimBP = UAnimBlueprint::FindRootAnimBlueprint(AnimBP);
		if (RootAnimBP == NULL)
		{
			// Only allow an anim graph if there isn't one in a parent blueprint
			UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(AnimBP, K2Schema->GN_AnimGraph, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
			FBlueprintEditorUtils::AddDomainSpecificGraph(NewBP, NewGraph);
			NewBP->LastEditedDocuments.Add(NewGraph);
			NewGraph->bAllowDeletion = false;
		}
		else
		{
			// Make sure the anim blueprint targets the same skeleton as the parent
			AnimBP->TargetSkeleton = RootAnimBP->TargetSkeleton;
		}
	}

	// Create initial UClass
	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	
	FCompilerResultsLog Results;
	const bool bReplaceExistingInstances = false;
	NewBP->Status = BS_Dirty;
	FKismetCompilerOptions CompileOptions;
	Compiler.CompileBlueprint(NewBP, CompileOptions, Results);

	//@TODO: ANIMREFACTOR 2: This kind of code should be on a per-blueprint basis; not centralized here
	if(UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(NewBP))
	{
		// add default nodes to event graph
		UEdGraph* Graph = NewBP->UbergraphPages[0];

		if(Graph->Nodes.Num() == 0)
		{
			// add update event graph
			UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
			EventNode->EventSignatureClass = UAnimInstance::StaticClass();
			EventNode->EventSignatureName = FName(TEXT("BlueprintUpdateAnimation"));
			EventNode->bOverrideFunction=true;
			EventNode->CreateNewGuid();
			EventNode->PostPlacedNewNode();
			EventNode->AllocateDefaultPins();
			UEdGraphSchema_K2::SetNodeMetaData(EventNode, FNodeMetadata::DefaultGraphNode);

			Graph->AddNode(EventNode);

			// add try get owner node
			UK2Node_CallFunction* GetOwnerNode = NewObject<UK2Node_CallFunction>(Graph);
			UFunction* MakeNodeFunction = FindObject<UClass>(ANY_PACKAGE, TEXT("AnimInstance"))->FindFunctionByName(TEXT("TryGetPawnOwner"));
			GetOwnerNode->CreateNewGuid();
			GetOwnerNode->PostPlacedNewNode();
			GetOwnerNode->SetFromFunction(MakeNodeFunction);
			GetOwnerNode->AllocateDefaultPins();
			GetOwnerNode->NodePosX = EventNode->NodePosX;
			GetOwnerNode->NodePosY = EventNode->NodePosY + EventNode->NodeHeight + 100;
			UEdGraphSchema_K2::SetNodeMetaData(GetOwnerNode, FNodeMetadata::DefaultGraphNode);

			Graph->AddNode(GetOwnerNode);
		}
	}

	// Report blueprint creation to analytics
	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attribs;

		// translate the CallingContext into a string for analytics
		if (CallingContext != NAME_None)
		{
			Attribs.Add(FAnalyticsEventAttribute(FString("Context"), CallingContext.ToString()));
		}
		
		Attribs.Add(FAnalyticsEventAttribute(FString("ParentType"), ParentClass->ClassGeneratedBy == NULL ? FString("Native") : FString("Blueprint")));

		if(IsTrackedBlueprintParent(ParentClass))
		{
			Attribs.Add(FAnalyticsEventAttribute(FString("ParentClass"), ParentClass->GetName()));
		}

		const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
		Attribs.Add(FAnalyticsEventAttribute(FString("ProjectId"), ProjectSettings.ProjectID.ToString()));
		Attribs.Add(FAnalyticsEventAttribute(FString("BlueprintId"), NewBP->GetBlueprintGuid().ToString()));

		FEngineAnalytics::GetProvider().RecordEvent(FString("Editor.Usage.BlueprintCreated"), Attribs);
	}

	return NewBP;
}

UBlueprint* FKismetEditorUtilities::ReloadBlueprint(UBlueprint* StaleBlueprint)
{
	check(StaleBlueprint->IsAsset());
	FStringAssetReference BlueprintAssetRef(StaleBlueprint);

	FBlueprintUnloader Unloader(StaleBlueprint);
	Unloader.UnloadBlueprint(/*bResetPackage =*/true);

	UBlueprint* ReloadedBlueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), /*Outer =*/nullptr, *BlueprintAssetRef.AssetLongPathname));

	Unloader.ReplaceStaleRefs(ReloadedBlueprint);
	return ReloadedBlueprint;
}

UBlueprint* FKismetEditorUtilities::ReplaceBlueprint(UBlueprint* Target, UBlueprint const* ReplacementArchetype)
{
	UPackage* BlueprintPackage = Target->GetOutermost();
	check(BlueprintPackage != GetTransientPackage());
	const FString BlueprintName = Target->GetName();

	FBlueprintUnloader Unloader(Target);
	Unloader.UnloadBlueprint(/*bResetPackage =*/false);

	UBlueprint* Replacement = Cast<UBlueprint>(StaticDuplicateObject(ReplacementArchetype, BlueprintPackage, *BlueprintName));

	Unloader.ReplaceStaleRefs(Replacement);
	return Replacement;
}

bool FKismetEditorUtilities::IsReferencedByUndoBuffer(UBlueprint* Blueprint)
{
	UObject* BlueprintObj = Blueprint;
	FReferencerInformationList ReferencesIncludingUndo;
	IsReferenced(BlueprintObj, GARBAGE_COLLECTION_KEEPFLAGS, /*bCheckSubObjects =*/true, &ReferencesIncludingUndo);

	FReferencerInformationList ReferencesExcludingUndo;
	// Determine the in-memory references, *excluding* the undo buffer
	GEditor->Trans->DisableObjectSerialization();
	IsReferenced(BlueprintObj, GARBAGE_COLLECTION_KEEPFLAGS, /*bCheckSubObjects =*/true, &ReferencesExcludingUndo);
	GEditor->Trans->EnableObjectSerialization();

	// see if this object is the transaction buffer - set a flag so we know we need to clear the undo stack
	const int32 TotalReferenceCount   = ReferencesIncludingUndo.ExternalReferences.Num() + ReferencesIncludingUndo.InternalReferences.Num();
	const int32 NonUndoReferenceCount = ReferencesExcludingUndo.ExternalReferences.Num() + ReferencesExcludingUndo.InternalReferences.Num();

	return (TotalReferenceCount > NonUndoReferenceCount);
}

void FKismetEditorUtilities::CompileBlueprint(UBlueprint* BlueprintObj, bool bIsRegeneratingOnLoad, bool bSkipGarbageCollection, bool bSaveIntermediateProducts, FCompilerResultsLog* pResults)
{
	// Broadcast pre-compile
#if WITH_EDITOR
	if(GEditor && GIsEditor)
	{
		GEditor->BroadcastBlueprintPreCompile(BlueprintObj);
	}
#endif

	// Reset the flag, so if the user tries to use PIE it will warn them if the BP did not compile
	BlueprintObj->bDisplayCompilePIEWarning = true;

	UPackage* const BlueprintPackage = Cast<UPackage>(BlueprintObj->GetOutermost());
	// compiling the blueprint will inherently dirty the package, but if there 
	// weren't any changes to save before, there shouldn't be after
	bool const bStartedWithUnsavedChanges = (BlueprintPackage != NULL) ? BlueprintPackage->IsDirty() : true;
#if WITH_EDITOR
	// Do not want to run this code without the editor present nor when running commandlets.
	if (GEditor && GIsEditor)
	{
		// We do not want to regenerate a search Guid during loads, nothing has changed in the Blueprint and it is cached elsewhere
		if (!bIsRegeneratingOnLoad)
		{
			FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(BlueprintObj);
		}
	}
#endif

	// The old class is either the GeneratedClass if we had an old successful compile, or the SkeletonGeneratedClass stub if there were previously fatal errors
	UClass* OldClass = (BlueprintObj->GeneratedClass != NULL && (BlueprintObj->GeneratedClass != BlueprintObj->SkeletonGeneratedClass)) ? BlueprintObj->GeneratedClass : NULL;

	// Load the compiler
	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

	// Prepare old objects for reinstancing
	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);

	// Compile
	FCompilerResultsLog LocalResults;
	FCompilerResultsLog& Results = (pResults != NULL) ? *pResults : LocalResults;

	BP_SCOPED_COMPILER_EVENT_NAME(TEXT("Compile Blueprint"));

	FBlueprintCompileReinstancer ReinstanceHelper(OldClass);

	// Suppress errors/warnings in the log if we're recompiling on load on a build machine
	Results.bLogInfoOnly = BlueprintObj->bIsRegeneratingOnLoad && GIsBuildMachine;

	FKismetCompilerOptions CompileOptions;
	CompileOptions.bSaveIntermediateProducts = bSaveIntermediateProducts;
	Compiler.CompileBlueprint(BlueprintObj, CompileOptions, Results, &ReinstanceHelper);

	FBlueprintEditorUtils::UpdateDelegatesInBlueprint(BlueprintObj);

	if (FBlueprintEditorUtils::IsLevelScriptBlueprint(BlueprintObj))
	{
		// When the Blueprint is recompiled, then update the bound events for level scripting
		ULevelScriptBlueprint* LevelScriptBP = CastChecked<ULevelScriptBlueprint>(BlueprintObj);

		if (ULevel* BPLevel = LevelScriptBP->GetLevel())
		{
			BPLevel->OnLevelScriptBlueprintChanged(LevelScriptBP);
		}
	}

	ReinstanceHelper.UpdateBytecodeReferences();

	if (!bIsRegeneratingOnLoad && (OldClass != NULL))
	{
		// Strip off any external components from the CDO, if needed because of reparenting, etc
		FKismetEditorUtilities::StripExternalComponents(BlueprintObj);

		// Ensure that external SCS node references match up with the generated class
		if(BlueprintObj->SimpleConstructionScript)
		{
			BlueprintObj->SimpleConstructionScript->FixupRootNodeParentReferences();
		}

		// Replace instances of this class
		static const FBoolConfigValueHelper ReinstanceOnlyWhenNecessary(TEXT("Kismet"), TEXT("bReinstanceOnlyWhenNecessary"), GEngineIni);
		ReinstanceHelper.ReinstanceObjects(!ReinstanceOnlyWhenNecessary);

		// Notify everyone a blueprint has been compiled and reinstanced, but before GC so they can perform any final cleanup.
		if ( GEditor )
		{
			GEditor->BroadcastBlueprintReinstanced();
		}

		if (!bSkipGarbageCollection)
		{
			BP_SCOPED_COMPILER_EVENT_NAME(TEXT("Garbage Collection"));

			// Garbage collect to make sure the old class and actors are disposed of
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

		// If you need to verify that all old instances are taken care of, uncomment this!
		// ReinstanceHelper.VerifyReplacement();
	}

	{ 
		BP_SCOPED_COMPILER_EVENT_NAME(TEXT("Notify Blueprint Changed"));

		// Blueprint has changed, broadcast notify
		BlueprintObj->BroadcastChanged();

		if(GEditor)
		{
			GEditor->BroadcastBlueprintCompiled();	
		}
	}

	// Default Values are now set in CDO. And these copies could be soon obsolete, so better to reset them.
	for(int VarIndex = 0; VarIndex < BlueprintObj->NewVariables.Num(); ++VarIndex)
	{
		BlueprintObj->NewVariables[VarIndex].DefaultValue.Empty();
	}

	{ 
		BP_SCOPED_COMPILER_EVENT_NAME(TEXT("Refresh Dependent Blueprints"));

		TArray<UBlueprint*> DependentBPs;
		FBlueprintEditorUtils::GetDependentBlueprints(BlueprintObj, DependentBPs);

		// refresh each dependent blueprint
		for (UBlueprint* Dependent : DependentBPs)
		{
			// for interface changes, auto-refresh nodes on any dependent blueprints
			// note: RefreshAllNodes() will internally send a change notification event to the dependent blueprint
			if (FBlueprintEditorUtils::IsInterfaceBlueprint(BlueprintObj))
			{
				bool bPreviousRegenValue = Dependent->bIsRegeneratingOnLoad;
				Dependent->bIsRegeneratingOnLoad = Dependent->bIsRegeneratingOnLoad || BlueprintObj->bIsRegeneratingOnLoad;
				FBlueprintEditorUtils::RefreshAllNodes(Dependent);
				Dependent->bIsRegeneratingOnLoad = bPreviousRegenValue;
			}
			else if(!BlueprintObj->bIsRegeneratingOnLoad)
			{
				// for non-interface changes, nodes with an external dependency have already been refreshed, and it is now safe to send a change notification event
				Dependent->BroadcastChanged();
			}
		}
	}

	if(!bIsRegeneratingOnLoad && BlueprintObj->GeneratedClass)
	{
		BP_SCOPED_COMPILER_EVENT_NAME(TEXT("Validate Generated Class"));
		UBlueprint::ValidateGeneratedClass(BlueprintObj->GeneratedClass);
	}

	if (BlueprintPackage != NULL)
	{
		BlueprintPackage->SetDirtyFlag(bStartedWithUnsavedChanges);

		UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
		const bool bShouldSaveOnCompile = !bIsRegeneratingOnLoad && (( Settings->SaveOnCompile == SoC_Always ) || ( ( Settings->SaveOnCompile == SoC_SuccessOnly ) && ( BlueprintObj->Status == BS_UpToDate ) ));

		// Only try to save on compile if we aren't running a commandlet (i.e. cooking a blueprint shouldn't try to save it)
		if ( !IsRunningCommandlet() && bShouldSaveOnCompile && !GIsAutomationTesting )
		{
			bool const bIsLevelPackage = (UWorld::FindWorldInPackage(BlueprintPackage) != nullptr);
			// we don't want to save the entire level (especially if this 
			// compile was already kicked off as a result of a level save, as it
			// could cause a recursive save)... let the "SaveOnCompile" setting 
			// only save blueprint assets
			if (!bIsLevelPackage)
			{
				TArray<UPackage*> PackagesToSave;
				PackagesToSave.Add(BlueprintPackage);

				FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty =*/true, /*bPromptToSave =*/false);
			}			
		}
	}	
}

/** Generates a blueprint skeleton only.  Minimal compile, no notifications will be sent, no GC, etc.  Only successful if there isn't already a skeleton generated */
void FKismetEditorUtilities::GenerateBlueprintSkeleton(UBlueprint* BlueprintObj, bool bForceRegeneration)
{
	check(BlueprintObj);

	if( BlueprintObj->SkeletonGeneratedClass == NULL || bForceRegeneration )
	{
		UPackage* Package = Cast<UPackage>(BlueprintObj->GetOutermost());
		bool bIsPackageDirty = Package ? Package->IsDirty() : false;
					
		IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

		TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
		FCompilerResultsLog Results;

		FKismetCompilerOptions CompileOptions;
		CompileOptions.CompileType = EKismetCompileType::SkeletonOnly;
		Compiler.CompileBlueprint(BlueprintObj, CompileOptions, Results);

		// Restore the package dirty flag here
		if( Package != NULL )
		{
			Package->SetDirtyFlag(bIsPackageDirty);
		}
	}
}

/** Recompiles the bytecode of a blueprint only.  Should only be run for recompiling dependencies during compile on load */
void FKismetEditorUtilities::RecompileBlueprintBytecode(UBlueprint* BlueprintObj, TArray<UObject*>* ObjLoaded)
{
	check(BlueprintObj);
	checkf(BlueprintObj->GeneratedClass, TEXT("Invalid generated class for %s"), *BlueprintObj->GetName());

	UPackage* const BlueprintPackage = Cast<UPackage>(BlueprintObj->GetOutermost());
	bool const bStartedWithUnsavedChanges = (BlueprintPackage != NULL) ? BlueprintPackage->IsDirty() : true;

	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
	FCompilerResultsLog Results;

	FBlueprintCompileReinstancer ReinstanceHelper(BlueprintObj->GeneratedClass, true);

	FKismetCompilerOptions CompileOptions;
	CompileOptions.CompileType = EKismetCompileType::BytecodeOnly;
	Compiler.CompileBlueprint(BlueprintObj, CompileOptions, Results, NULL, ObjLoaded);

	ReinstanceHelper.UpdateBytecodeReferences();

	if (BlueprintPackage != NULL)
	{
		BlueprintPackage->SetDirtyFlag(bStartedWithUnsavedChanges);
	}
}

/** Recompiles the bytecode of a blueprint only.  Should only be run for recompiling dependencies during compile on load */
void FKismetEditorUtilities::GenerateCppCode(UBlueprint* InBlueprintObj, TSharedPtr<FString> OutHeaderSource, TSharedPtr<FString> OutCppSource)
{
	check(InBlueprintObj);
	check(InBlueprintObj->GetOutermost() != GetTransientPackage());
	checkf(InBlueprintObj->GeneratedClass, TEXT("Invalid generated class for %s"), *InBlueprintObj->GetName());
	check(OutHeaderSource.IsValid());
	check(OutCppSource.IsValid());

	//TGuardValue<bool> DuplicatingReadOnly(InBlueprintObj->bDuplicatingReadOnly, true);
	{
		auto BlueprintObj = DuplicateObject<UBlueprint>(InBlueprintObj, GetTransientPackage(), *InBlueprintObj->GetName());
		{
			IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

			TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
			FCompilerResultsLog Results;

			FKismetCompilerOptions CompileOptions;
			CompileOptions.CompileType = EKismetCompileType::Cpp;
			CompileOptions.OutCppSourceCode = OutCppSource;
			CompileOptions.OutHeaderSourceCode = OutHeaderSource;
			Compiler.CompileBlueprint(BlueprintObj, CompileOptions, Results);
		}
		BlueprintObj->RemoveGeneratedClasses();
		BlueprintObj->ClearFlags(RF_Standalone);
		BlueprintObj->MarkPendingKill();
	}
}

/** Tries to make sure that a blueprint is conformed to its native parent, in case any native class flags have changed */
void FKismetEditorUtilities::ConformBlueprintFlagsAndComponents(UBlueprint* BlueprintObj)
{
	// Propagate native class flags to the children class.  This fixes up cases where native instanced components get added after BP creation, etc
	const UClass* ParentClass = BlueprintObj->ParentClass;

	if( UClass* SkelClass = BlueprintObj->SkeletonGeneratedClass )
	{
		SkelClass->ClassFlags |= (ParentClass->ClassFlags & CLASS_ScriptInherit);
		UObject* SkelCDO = SkelClass->GetDefaultObject();
		SkelCDO->InstanceSubobjectTemplates();
	}

	if( UClass* GenClass = BlueprintObj->GeneratedClass )
	{
		GenClass->ClassFlags |= (ParentClass->ClassFlags & CLASS_ScriptInherit);
		UObject* GenCDO = GenClass->GetDefaultObject();
		GenCDO->InstanceSubobjectTemplates();
	}
}

/** @return		true is it's possible to create a blueprint from the specified class */
bool FKismetEditorUtilities::CanCreateBlueprintOfClass(const UClass* Class)
{
	bool bAllowDerivedBlueprints = false;
	GConfig->GetBool(TEXT("Kismet"), TEXT("AllowDerivedBlueprints"), /*out*/ bAllowDerivedBlueprints, GEngineIni);

	const bool bCanCreateBlueprint =
		!Class->HasAnyClassFlags(CLASS_Deprecated)
		&& !Class->HasAnyClassFlags(CLASS_NewerVersionExists)
		&& (!Class->ClassGeneratedBy || (bAllowDerivedBlueprints && !IsClassABlueprintSkeleton(Class)));

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	const bool bIsValidClass = Class->GetBoolMetaDataHierarchical(FBlueprintMetadata::MD_IsBlueprintBase)
		|| (Class == UObject::StaticClass());

	return bCanCreateBlueprint && bIsValidClass;
}

UBlueprint* FKismetEditorUtilities::CreateBlueprintFromActor(const FString& Path, UObject* Object, bool bReplaceActor )
{
	UBlueprint* NewBlueprint = NULL;

	if (AActor* Actor = Cast<AActor>(Object))
	{
		// Create a blueprint
		FString PackageName = Path;
		FString AssetName = FPackageName::GetLongPackageAssetName(Path);


		// If no AssetName was found, generate a unique asset name.
		if(AssetName.Len() == 0)
		{
			PackageName = FPackageName::GetLongPackagePath(Path);
			FString BasePath = PackageName + TEXT("/") + LOCTEXT("BlueprintName_Default", "NewBlueprint").ToString();
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			AssetToolsModule.Get().CreateUniqueAssetName(BasePath, TEXT(""), PackageName, AssetName);
		}

		UPackage* Package = CreatePackage(NULL, *PackageName);

		if(Package)
		{
			UActorFactory* FactoryToUse = GEditor->FindActorFactoryForActorClass( Actor->GetClass() );
			const FName BlueprintName = FName(*AssetName);

			if( FactoryToUse != NULL )
			{
				// Create the blueprint
				UObject* Asset = FactoryToUse->GetAssetFromActorInstance(Actor);
				// For Actors that don't have an asset associated with them, Asset will be null
				NewBlueprint = FactoryToUse->CreateBlueprint( Asset, Package, BlueprintName, FName("CreateFromActor") );
			}
			else 
			{
				// We don't have a factory, but we can still try to create a blueprint for this actor class
				NewBlueprint = FKismetEditorUtilities::CreateBlueprint( Object->GetClass(), Package, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("CreateFromActor") );
			}
		}

		if(NewBlueprint)
		{
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(NewBlueprint);

			// Mark the package dirty
			Package->MarkPackageDirty();

			if(NewBlueprint->GeneratedClass)
			{
				UObject* CDO = NewBlueprint->GeneratedClass->GetDefaultObject();
				UEditorEngine::CopyPropertiesForUnrelatedObjects(Object, CDO);
				if(AActor* CDOAsActor = Cast<AActor>(CDO))
				{
					if(USceneComponent* Scene = CDOAsActor->GetRootComponent())
					{
						Scene->SetRelativeLocation(FVector::ZeroVector);

						// Clear out the attachment info after having copied the properties from the source actor
						Scene->AttachParent = NULL;
						Scene->AttachChildren.Empty();

						// Ensure the light mass information is cleaned up
						Scene->InvalidateLightingCache();
					}
				}
			}

			if(bReplaceActor)
			{
				TArray<AActor*> Actors;
				Actors.Add(Actor);

				FVector Location = Actor->GetActorLocation();
				FRotator Rotator = Actor->GetActorRotation();

				CreateBlueprintInstanceFromSelection(NewBlueprint, Actors, Location, Rotator);
			}
		}
	}

	if (NewBlueprint)
	{
		// Open the editor for the new blueprint
		FAssetEditorManager::Get().OpenEditorForAsset(NewBlueprint);
	}
	return NewBlueprint;
}

// This class cracks open the selected actors, harvests their components, and creates a new blueprint containing copies of them
class FCreateConstructionScriptFromSelectedActors
{
public:
	FCreateConstructionScriptFromSelectedActors()
		: Blueprint(NULL)
		, SCS(NULL)
	{
	}

	UBlueprint* Execute(FString Path, TArray<AActor*> SelectedActors, bool bReplaceInWorld)
	{
		if (SelectedActors.Num() > 0)
		{
			// See if we have a single actor with a single component selected
			// If so, we don't bother creating the root SceneComponent.
			USceneComponent* SingleSceneComp = NULL;
			if(SelectedActors.Num() == 1)
			{
				TArray<UActorComponent*> Components;
				SelectedActors[0]->GetComponents(Components);
				for(UActorComponent* Component : Components)
				{
					USceneComponent* SceneComp = Cast<USceneComponent>(Component);
					if( SceneComp != NULL && 
						!SceneComp->bHiddenInGame && 
						SceneComp->GetClass()->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent) )
					{
						// If we already found one, then there is more than one, so set to NULL and break and continue
						if(SingleSceneComp != NULL)
						{
							SingleSceneComp = NULL;
							break;
						}
						// This is the first valid scene component we have found, save it
						else
						{
							SingleSceneComp = SceneComp;
						}
					}
				}
			}

			// Determine the origin for all the actors, so it can be backed out when saving them in the blueprint
			FTransform NewActorTransform = FTransform::Identity;

			// Create a blueprint
			FString PackageName = Path;
			FString AssetName = FPackageName::GetLongPackageAssetName(Path);
			FString BasePath = PackageName + TEXT("/") + AssetName;

			// If no AssetName was found, generate a unique asset name.
			if(AssetName.Len() == 0)
			{
				BasePath = PackageName + TEXT("/") + LOCTEXT("BlueprintName_Default", "NewBlueprint").ToString();
				FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
				AssetToolsModule.Get().CreateUniqueAssetName(BasePath, TEXT(""), PackageName, AssetName);
			}

			UPackage* Package = CreatePackage(NULL, *PackageName);
			Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, *AssetName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("HarvestFromActors"));

			check(Blueprint->SimpleConstructionScript != NULL);
			SCS = Blueprint->SimpleConstructionScript;

			// If we have a single component selected, make a BP with a single component
			if(SingleSceneComp != NULL)
			{
				USCS_Node* Node = CreateUSCSNode(SingleSceneComp);
				Node->VariableName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, SingleSceneComp->GetOwner()->GetActorLabel());
				SCS->AddNode(Node);

				NewActorTransform = SingleSceneComp->ComponentToWorld;
			}
			// Multiple actors/component selected, so we create a root scenecomponent and attach things to it
			else
			{
				// Find average location of all selected actors
				FVector AverageLocation = FVector::ZeroVector;
				for (auto It(SelectedActors.CreateConstIterator()); It; ++It)
				{
					if (const AActor* Actor = Cast<AActor>(*It))
					{
						if (USceneComponent* RootComponent = Actor->GetRootComponent())
						{
							AverageLocation += Actor->GetActorLocation();
						}
					}
				}
				AverageLocation /= (float)SelectedActors.Num();

				// Spawn the new BP at that location
				NewActorTransform.SetTranslation(AverageLocation);

				// Add a new scene component to serve as the root node
				USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass());
				SCS->AddNode(RootNode);

				TMap<const AActor*, USCS_Node*> ActorToUSCSNodeMap;

				// Add each actor's root component to the mapping of Actors to USCS_Node map to be organized later
				for (auto ActorIt = SelectedActors.CreateConstIterator(); ActorIt; ++ActorIt)
				{
					const AActor* Actor = *ActorIt;
					USceneComponent* ActorRootComponent = Actor->GetRootComponent();
					// Check we have a component and it is valid to add to a BP
					if(ActorRootComponent != NULL && ActorRootComponent->GetClass()->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent))
					{
						USCS_Node* Node = CreateUSCSNode(ActorRootComponent);
						Node->VariableName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, Actor->GetActorLabel());

						// Add the node as a child to the root node, this will be rearranged later for correct parent-child relations but needs to be there now for name validation purposes
						RootNode->AddChildNode(Node);

						ActorToUSCSNodeMap.Add(Actor, Node);
					}
				}

				// Clear the root node of all children we added, they will be re-added with correct parent-child relationships
				RootNode->ChildNodes.Empty();

				// Attach all immediate children to their parent, or the root if their parent is not being added
				for (auto ActorIt = SelectedActors.CreateConstIterator(); ActorIt; ++ActorIt)
				{
					const AActor* Actor = *ActorIt;
					USceneComponent* ActorRootComponent = Actor->GetRootComponent();
					// Check we have a component and it is valid to add to a BP
					if(ActorRootComponent != NULL && ActorRootComponent->GetClass()->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent))
					{			
						USCS_Node** CurrentNode = ActorToUSCSNodeMap.Find(Actor);
						USCS_Node** ParentNode = ActorToUSCSNodeMap.Find(Actor->GetAttachParentActor());

						check(CurrentNode);

						FTransform ComponentToWorldSpace;
						if(ParentNode)
						{
							ComponentToWorldSpace = TransformChildComponent(Actor->GetRootComponent(), *CurrentNode, Actor->GetAttachParentActor()->GetRootComponent()->GetComponentToWorld());
							(*ParentNode)->AddChildNode(*CurrentNode);
						}
						else
						{
							ComponentToWorldSpace = TransformChildComponent(Actor->GetRootComponent(), *CurrentNode, NewActorTransform);
							RootNode->AddChildNode(*CurrentNode);
						}

						// Attach any children components as well, as long as their owner is this Actor
						AddActorComponents(Actor, Actor->GetRootComponent(), *CurrentNode, ComponentToWorldSpace);
					}
				}
			}

			// Regenerate skeleton class as components have been added since initial generation
			FKismetEditorUtilities::GenerateBlueprintSkeleton(Blueprint,true); 
			
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Blueprint);

			// Mark the package dirty
			Package->MarkPackageDirty();

			// Delete the old actors and create a new instance in the map
			if (bReplaceInWorld)
			{
				FVector Location = NewActorTransform.GetLocation();
				FRotator Rotator = NewActorTransform.Rotator();

				FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(Blueprint, SelectedActors, Location, Rotator);
			}

			// Open the editor for the new blueprint
			FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);

			return Blueprint;
		}
		return NULL;
	}

protected:

	void CopyComponentToTemplate(UActorComponent* Source, UActorComponent* Destination)
	{
		check(Source->GetClass() == Destination->GetClass());
		UClass* CommonBaseClass = Source->GetClass();
		UActorComponent* DefaultComponent = CastChecked<UActorComponent>(Destination->GetArchetype());

		//@TODO: Copy of code from inside CopyActorComponentProperties

		// Iterate through the properties, only copying those which are non-native, non-transient, non-component, and not identical
		// to the values in the default component
		for (UProperty* Property = CommonBaseClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
		{
			const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
			const bool bIsIdentical = Property->Identical_InContainer(Source, DefaultComponent);
			const bool bIsComponent = !!( Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference) );

			if (!bIsTransient && !bIsIdentical && !bIsComponent)
			{
				Property->CopyCompleteValue_InContainer(Destination, Source);
			}
		}

		if (UStaticMeshComponent* TemplateMeshComponent = Cast<UStaticMeshComponent>(Destination))
		{
			UStaticMeshComponent* SourceMeshComponent = CastChecked<UStaticMeshComponent>(Source);
			// vert coloring data lives in the mesh component's LODData member,
			// which is transient, so it wouldn't be copied in the above loop
			TemplateMeshComponent->CopyInstanceVertexColorsIfCompatible(SourceMeshComponent);
		}
	}

	/** Creates a USCS Node for the passed in component */
	USCS_Node* CreateUSCSNode(USceneComponent* Component)
	{
		// Create a new SCS node for this component
		USCS_Node* NewNode = SCS->CreateNode(Component->GetClass());

		// Copy properties across to the new template
		CopyComponentToTemplate(Component, NewNode->ComponentTemplate);

		return NewNode;
	}

	/**
	 * Transforms a USCS_Node component based on the original Component and the passed in transform.
	 *
	 * @param InOriginalComponent		The original component the USCS_Node is based on
	 * @param InUSCSNode				The USCS_Node based on the original component
	 * @param InParentToWorldSpace		FTransform to place the USCS_Node in the correct spot.
	 *
	 * @return							The component to world transform of the USCS_Node component
	 */
	FTransform TransformChildComponent(USceneComponent* InOriginalComponent, USCS_Node* InUSCSNode, const FTransform& InParentToWorldSpace)
	{
		// Position this node relative to it's parent
		USceneComponent* NewTemplate = CastChecked<USceneComponent>(InUSCSNode->ComponentTemplate);

		FTransform ChildToWorld = InOriginalComponent->GetComponentToWorld();
		FTransform RelativeTransform = ChildToWorld.GetRelativeTransform(InParentToWorldSpace);

		NewTemplate->SetRelativeTransform(RelativeTransform);

		return ChildToWorld;
	}

	/**
	 * Adds all child components (but not child actors) to the RootNode as a USCS_Node
	 *
	 * @param InParentActor		The actor all components must be a child of
	 * @param InComponent		The component to pull child components from
	 * @param InRootNode		The root node to attach all new USCS_Nodes to
	 * @param InParentToWorldSpace		FTransform to place the USCS_Node in the correct spot.
	 */
	void AddActorComponents(const AActor* InParentActor, USceneComponent* InComponent, USCS_Node* InRootNode, const FTransform& InParentToWorldSpace)
	{
		if (InComponent)
		{
			for (auto ChildIt = InComponent->AttachChildren.CreateConstIterator(); ChildIt; ++ChildIt)
			{
				USceneComponent* ChildComponent = *ChildIt;

				if(ChildComponent->GetOwner() == InParentActor)
				{				
					// if component is 'editor only' we usually don't want to copy it
					// Also check its a valid class to add
					if(!ChildComponent->bHiddenInGame && ChildComponent->GetClass()->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent))
					{
						USCS_Node* NewNode = CreateUSCSNode(ChildComponent);
						FTransform ComponentToWorldSpace = TransformChildComponent(ChildComponent, NewNode, InParentToWorldSpace);
						InRootNode->AddChildNode(NewNode);

						// Add any child components that may be owned by this Actor
						AddActorComponents(InParentActor, ChildComponent, NewNode, ComponentToWorldSpace);
					}
				}

			}
		}
	}


protected:
	UBlueprint* Blueprint;
	USimpleConstructionScript* SCS;
};


UBlueprint* FKismetEditorUtilities::HarvestBlueprintFromActors(const FString& Path, const TArray<AActor*>& Actors, bool bReplaceInWorld)
{
	FCreateConstructionScriptFromSelectedActors Creator;
	return Creator.Execute(Path, Actors, bReplaceInWorld);
}

AActor* FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(UBlueprint* Blueprint, TArray<AActor*>& SelectedActors, const FVector& Location, const FRotator& Rotator)
{
	check (SelectedActors.Num() > 0 );

	// Create transaction to cover conversion
	const FScopedTransaction Transaction( NSLOCTEXT("EditorEngine", "ConvertActorToBlueprint", "Replace Actor(s) with blueprint") );

	// Assume all selected actors are in the same world
	UWorld* World = SelectedActors[0]->GetWorld();

	for(auto It(SelectedActors.CreateIterator());It;++It)
	{
		if (AActor* Actor = Cast<AActor>(*It))
		{
			// Remove from active selection in editor
			GEditor->SelectActor(Actor, /*bSelected=*/ false, /*bNotify=*/ false);

			GEditor->Layers->DisassociateActorFromLayers(Actor);
			World->EditorDestroyActor(Actor, false);
		}
	}

	AActor* NewActor = World->SpawnActor(Blueprint->GeneratedClass, &Location, &Rotator);
	GEditor->Layers->InitializeNewActorLayers(NewActor);

	// Update selection to new actor
	GEditor->SelectActor( NewActor, /*bSelected=*/ true, /*bNotify=*/ true );

	return NewActor;
}

UBlueprint* FKismetEditorUtilities::CreateBlueprintFromClass(FText InWindowTitle, UClass* InParentClass, FString NewNameSuggestion)
{
	check(FKismetEditorUtilities::CanCreateBlueprintOfClass(InParentClass));

	// Pre-generate a unique asset name to fill out the path picker dialog with.
	if (NewNameSuggestion.Len() == 0)
	{
		NewNameSuggestion = TEXT("NewBlueprint");
	}

	FString PackageName = FString(TEXT("/Game/Blueprints/")) + NewNameSuggestion;
	FString Name;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, Name);

	TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
		SNew(SDlgPickAssetPath)
		.Title(InWindowTitle)
		.DefaultAssetPath(FText::FromString(PackageName));

	if (EAppReturnType::Ok == PickAssetPathWidget->ShowModal())
	{
		// Get the full name of where we want to create the physics asset.
		FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
		FName BPName(*FPackageName::GetLongPackageAssetName(UserPackageName));

		// Check if the user inputed a valid asset name, if they did not, give it the generated default name
		if (BPName == NAME_None)
		{
			// Use the defaults that were already generated.
			UserPackageName = PackageName;
			BPName = *Name;
		}

		// Then find/create it.
		UPackage* Package = CreatePackage(NULL, *UserPackageName);
		check(Package);

		// Create and init a new Blueprint
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(InParentClass, Package, BPName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("LevelEditorActions"));
		if (Blueprint)
		{
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Blueprint);

			// Mark the package dirty...
			Package->MarkPackageDirty();

			return Blueprint;
		}
	}
	return NULL;
}

UBlueprint* FKismetEditorUtilities::CreateBlueprintUsingAsset(UObject* Asset, bool bOpenInEditor)
{
	// Check we have an asset.
	if(Asset == NULL)
	{
		return NULL;
	}

	// Check we can create a component from this asset
	TSubclassOf<UActorComponent> ComponentClass = FComponentAssetBrokerage::GetPrimaryComponentForAsset(Asset->GetClass());
	if(ComponentClass != NULL)
	{
		// Create a new empty Actor BP
		UBlueprint* NewBP = CreateBlueprintFromClass(LOCTEXT("CreateBlueprint", "Create Blueprint"), AActor::StaticClass(), Asset->GetName());
		if(NewBP != NULL)
		{
			// Create a new SCS node
			check(NewBP->SimpleConstructionScript != NULL);
			USCS_Node* NewNode = NewBP->SimpleConstructionScript->CreateNode(ComponentClass);

			// Assign the asset to the template
			FComponentAssetBrokerage::AssignAssetToComponent(NewNode->ComponentTemplate, Asset);

			// Add node to the SCS
			NewBP->SimpleConstructionScript->AddNode(NewNode);

			// Recompile skeleton because of the new component we added (and 
			// broadcast the change to those that care, like the BP node database)
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(NewBP);

			// Open in BP editor if desired
			if(bOpenInEditor)
			{
				FAssetEditorManager::Get().OpenEditorForAsset(NewBP);
			}
		}

		return NewBP;
	}

	return NULL;
}


TSharedPtr<class IBlueprintEditor> FKismetEditorUtilities::GetIBlueprintEditorForObject( const UObject* ObjectToFocusOn, bool bOpenEditor )
{
	check(ObjectToFocusOn);

	// Find the associated blueprint
	UBlueprint* TargetBP = Cast<UBlueprint>(const_cast<UObject*>(ObjectToFocusOn));
	if (TargetBP == NULL)
	{
		for (UObject* TestOuter = ObjectToFocusOn->GetOuter(); TestOuter; TestOuter = TestOuter->GetOuter())
		{
			TargetBP = Cast<UBlueprint>(TestOuter);
			if (TargetBP != NULL)
			{
				break;
			}
		}
	}

	TSharedPtr<IBlueprintEditor> BlueprintEditor;
	if (TargetBP != NULL)
	{
		if (bOpenEditor)
		{
			// @todo toolkit major: Needs world-centric support
			FAssetEditorManager::Get().OpenEditorForAsset(TargetBP);
		}

		TSharedPtr< IToolkit > FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(TargetBP);
		// If we found a BlueprintEditor
		if (FoundAssetEditor.IsValid() && FoundAssetEditor->IsBlueprintEditor())
		{
			BlueprintEditor = StaticCastSharedPtr<IBlueprintEditor>(FoundAssetEditor);
		}
	}
	return BlueprintEditor;
}

void FKismetEditorUtilities::PasteNodesHere( class UEdGraph* Graph, const FVector2D& Location )
{
	TSharedPtr<class IBlueprintEditor> Kismet = GetIBlueprintEditorForObject(Graph,false);
	if(Kismet.IsValid())
	{
		Kismet->PasteNodesHere(Graph,Location);
	}
}

bool FKismetEditorUtilities::CanPasteNodes( const class UEdGraph* Graph )
{
	bool bCanPaste = false;
	TSharedPtr<class IBlueprintEditor> Kismet = GetIBlueprintEditorForObject(Graph,false);
	if (Kismet.IsValid())
	{
		bCanPaste = Kismet->CanPasteNodes();
	}
	return bCanPaste;
}

bool FKismetEditorUtilities::GetBoundsForSelectedNodes(const class UBlueprint* Blueprint,  class FSlateRect& Rect, float Padding)
{
	bool bCanPaste = false;
	TSharedPtr<class IBlueprintEditor> Kismet = GetIBlueprintEditorForObject(Blueprint, false);
	if (Kismet.IsValid())
	{
		bCanPaste = Kismet->GetBoundsForSelectedNodes(Rect, Padding);
	}
	return bCanPaste;
}

int32 FKismetEditorUtilities::GetNumberOfSelectedNodes(const class UBlueprint* Blueprint)
{
	int32 NumberNodesSelected = 0;
	TSharedPtr<class IBlueprintEditor> Kismet = GetIBlueprintEditorForObject(Blueprint, false);
	if (Kismet.IsValid())
	{
		NumberNodesSelected = Kismet->GetNumberOfSelectedNodes();
	}
	return NumberNodesSelected;
}

/** Open a Kismet window, focusing on the specified object (either a pin, a node, or a graph).  Prefers existing windows, but will open a new application if required. */
void FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(const UObject* ObjectToFocusOn, bool bRequestRename)
{
	TSharedPtr<IBlueprintEditor> BlueprintEditor = GetIBlueprintEditorForObject(ObjectToFocusOn, true);
	if (BlueprintEditor.IsValid())
	{
		BlueprintEditor->FocusWindow();
		BlueprintEditor->JumpToHyperlink(ObjectToFocusOn, bRequestRename);
	}
}

void FKismetEditorUtilities::ShowActorReferencesInLevelScript(const AActor* Actor)
{
	if (Actor != NULL)
	{
		ULevelScriptBlueprint* LSB = Actor->GetLevel()->GetLevelScriptBlueprint();
		if (LSB != NULL)
		{
			// @todo toolkit major: Needs world-centric support.  Other spots, too?
			FAssetEditorManager::Get().OpenEditorForAsset(LSB);
			TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(LSB);
			if (FoundAssetEditor.IsValid())
			{
				TSharedRef<IBlueprintEditor> BlueprintEditor = StaticCastSharedRef<IBlueprintEditor>(FoundAssetEditor.ToSharedRef());
				BlueprintEditor->FocusWindow();

				const bool bSetFindWithinBlueprint = true;
				const bool bSelectFirstResult = true;
				BlueprintEditor->SummonSearchUI(bSetFindWithinBlueprint, Actor->GetActorLabel(), bSelectFirstResult);
			}
		}

	}
}

// Upgrade any cosmetically stale information in a blueprint (done when edited instead of PostLoad to make certain operations easier)
void FKismetEditorUtilities::UpgradeCosmeticallyStaleBlueprint(UBlueprint* Blueprint)
{
	// Rename the ubergraph page 'StateGraph' to be named 'EventGraph' if possible
	if (FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint))
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		UEdGraph* OldStateGraph = FindObject<UEdGraph>(Blueprint, TEXT("StateGraph"));
		UObject* CollidingObject = FindObject<UObject>(Blueprint, *(K2Schema->GN_EventGraph.ToString()));

		if ((OldStateGraph != NULL) && (CollidingObject == NULL))
		{
			check(!OldStateGraph->HasAnyFlags(RF_Public));
			OldStateGraph->Rename(*(K2Schema->GN_EventGraph.ToString()), OldStateGraph->GetOuter(), REN_DoNotDirty | REN_ForceNoResetLoaders);
			Blueprint->Status = BS_Dirty;
		}
	}
}

void FKismetEditorUtilities::CreateNewBoundEventForActor(AActor* Actor, FName EventName)
{
	if(Actor != NULL && EventName != NAME_None)
	{
		// First, find the property we want to bind to
		UMulticastDelegateProperty* DelegateProperty = FindField<UMulticastDelegateProperty>(Actor->GetClass(), EventName);
		if(DelegateProperty != NULL)
		{
			// Get the correct level script blueprint
			ULevelScriptBlueprint* LSB = Actor->GetLevel()->GetLevelScriptBlueprint();
			UEdGraph* TargetGraph = NULL;
			if(LSB != NULL && LSB->UbergraphPages.Num() > 0)
			{
				TargetGraph = LSB->UbergraphPages[0]; // Just use the forst graph
			}

			if(TargetGraph != NULL)
			{
				// Figure out a decent place to stick the node
				const FVector2D NewNodePos = TargetGraph->GetGoodPlaceForNewNode();

				// Create a new event node
				UK2Node_ActorBoundEvent* EventNodeTemplate = NewObject<UK2Node_ActorBoundEvent>();
				EventNodeTemplate->InitializeActorBoundEventParams(Actor, DelegateProperty);

				UK2Node_ActorBoundEvent* EventNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_ActorBoundEvent>(TargetGraph, EventNodeTemplate, NewNodePos);

				// Finally, bring up kismet and jump to the new node
				if(EventNode != NULL)
				{
					BringKismetToFocusAttentionOnObject(EventNode);
				}
			}
		}
	}
}

void FKismetEditorUtilities::CreateNewBoundEventForComponent(UActorComponent* Component, FName EventName, UBlueprint* Blueprint, UObjectProperty* ComponentProperty)
{
	if ((Component != NULL) && (EventName != NAME_None) && (Blueprint != NULL) && (ComponentProperty != NULL))
	{
		// First, find the property we want to bind to
		UMulticastDelegateProperty* DelegateProperty = FindField<UMulticastDelegateProperty>(Component->GetClass(), EventName);
		if(DelegateProperty != NULL)
		{
			UEdGraph* TargetGraph = NULL;
			if(Blueprint->UbergraphPages.Num() > 0)
			{
				TargetGraph = Blueprint->UbergraphPages[0]; // Just use the first graph
			}

			if(TargetGraph != NULL)
			{
				// Figure out a decent place to stick the node
				const FVector2D NewNodePos = TargetGraph->GetGoodPlaceForNewNode();

				// Create a new event node
				UK2Node_ComponentBoundEvent* EventNodeTemplate = NewObject<UK2Node_ComponentBoundEvent>();
				EventNodeTemplate->InitializeComponentBoundEventParams(ComponentProperty, DelegateProperty);

				UK2Node_ComponentBoundEvent* EventNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_ComponentBoundEvent>(TargetGraph, EventNodeTemplate, NewNodePos);

				// Finally, bring up kismet and jump to the new node
				if (EventNode != NULL)
				{
					BringKismetToFocusAttentionOnObject(EventNode);
				}
			}
		}
	}
}

const UK2Node_ActorBoundEvent* FKismetEditorUtilities::FindBoundEventForActor(AActor const* Actor, FName EventName)
{
	const UK2Node_ActorBoundEvent* Node = NULL;
	if(Actor != NULL && EventName != NAME_None)
	{
		ULevelScriptBlueprint* LSB = Actor->GetLevel()->GetLevelScriptBlueprint(true);
		if(LSB != NULL)
		{
			TArray<UK2Node_ActorBoundEvent*> EventNodes;
			FBlueprintEditorUtils::GetAllNodesOfClass(LSB, EventNodes);
			for(int32 i=0; i<EventNodes.Num(); i++)
			{
				UK2Node_ActorBoundEvent* BoundEvent = EventNodes[i];
				if(BoundEvent->EventOwner == Actor && BoundEvent->DelegatePropertyName == EventName)
				{
					Node = BoundEvent;
					break;
				}
			}
		}
	}
	return Node;
}

const UK2Node_ComponentBoundEvent* FKismetEditorUtilities::FindBoundEventForComponent(const UBlueprint* Blueprint, FName EventName, FName PropertyName)
{
	const UK2Node_ComponentBoundEvent* Node = NULL;
	if (Blueprint && EventName != NAME_None && PropertyName != NAME_None)
	{
		TArray<UK2Node_ComponentBoundEvent*> EventNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, EventNodes);
		for(auto NodeIter = EventNodes.CreateIterator(); NodeIter; ++NodeIter)
		{
			UK2Node_ComponentBoundEvent* BoundEvent = *NodeIter;
			if ((BoundEvent->ComponentPropertyName == PropertyName) && (BoundEvent->DelegatePropertyName == EventName))
			{
				Node = *NodeIter;
				break;
			}
		}
	}
	return Node;
}

bool FKismetEditorUtilities::IsClassABlueprintInterface(const UClass* Class)
{
	if (Class->HasAnyClassFlags(CLASS_Interface) && !Class->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		return true;
	}
	return false;
}



bool FKismetEditorUtilities::CanBlueprintImplementInterface(UBlueprint const* Blueprint, UClass const* Class)
{
	bool bCanImplementInterface = false;

	// if the class is an actual implementable interface
	if (IsClassABlueprintInterface(Class) && !Class->HasMetaData(FBlueprintMetadata::MD_CannotImplementInterfaceInBlueprint))
	{
		bCanImplementInterface = true;

		UClass const* const ParentClass = Blueprint->ParentClass;
		// see if the parent class has any prohibited interfaces
		if ((ParentClass != NULL) && ParentClass->HasMetaData(FBlueprintMetadata::MD_ProhibitedInterfaces))
		{
			FString const& ProhibitedList = Blueprint->ParentClass->GetMetaData(FBlueprintMetadata::MD_ProhibitedInterfaces);
			
			TArray<FString> ProhibitedInterfaceNames;
			ProhibitedList.ParseIntoArray(&ProhibitedInterfaceNames, TEXT(","), true);

			FString const& InterfaceName = Class->GetName();
			// loop over all the prohibited interfaces
			for (int32 ExclusionIndex = 0; ExclusionIndex < ProhibitedInterfaceNames.Num(); ++ExclusionIndex)
			{
				FString const& Exclusion = ProhibitedInterfaceNames[ExclusionIndex].Trim();
				// if this interface matches one of the prohibited ones
				if (InterfaceName == Exclusion) 
				{
					bCanImplementInterface = false;
					break;
				}
			}
		}
	}

	return bCanImplementInterface;
}

bool FKismetEditorUtilities::IsClassABlueprintSkeleton(const UClass* Class)
{
	// Find generating blueprint for a class
	UBlueprint* GeneratingBP = Cast<UBlueprint>(Class->ClassGeneratedBy);
	if( GeneratingBP && GeneratingBP->SkeletonGeneratedClass )
	{
		return (Class == GeneratingBP->SkeletonGeneratedClass) && (GeneratingBP->SkeletonGeneratedClass != GeneratingBP->GeneratedClass);
	}
	return Class->HasAnyFlags(RF_Transient) && Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
}

bool FKismetEditorUtilities::IsClassABlueprintMacroLibrary(const UClass* Class)
{
	// Find generating blueprint for a class
	UBlueprint* GeneratingBP = Cast<UBlueprint>(Class->ClassGeneratedBy);
	return (GeneratingBP && GeneratingBP->BlueprintType == BPTYPE_MacroLibrary);
}

/** Run over the components references, and then NULL any that fall outside this blueprint's scope (e.g. components brought over after reparenting from another class, which are now in the transient package) */
void FKismetEditorUtilities::StripExternalComponents(class UBlueprint* Blueprint)
{
	FArchiveInvalidateTransientRefs InvalidateRefsAr;
	
	UClass* SkeletonGeneratedClass = Blueprint->SkeletonGeneratedClass;
	UObject* SkeletonCDO = SkeletonGeneratedClass->GetDefaultObject();

	SkeletonCDO->Serialize(InvalidateRefsAr);

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	UObject* GeneratedCDO = GeneratedClass->GetDefaultObject();

	GeneratedCDO->Serialize(InvalidateRefsAr);
}

bool FKismetEditorUtilities::IsTrackedBlueprintParent(const UClass* ParentClass)
{
	if (ParentClass->ClassGeneratedBy == NULL)
	{
		// Always track native parent classes
		return true;
	}

	UBlueprint* ParentBlueprint = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);

	// Cache the list of allowed blueprint names the first time it is requested
	if (TrackedBlueprintParentList.Num() == 0)
	{
		GConfig->GetArray(TEXT("Kismet"), TEXT("TrackedBlueprintParents"), /*out*/ TrackedBlueprintParentList, GEngineIni);
	}

	for (auto TrackedBlueprintIter = TrackedBlueprintParentList.CreateConstIterator(); TrackedBlueprintIter; ++TrackedBlueprintIter)
	{
		if (ParentBlueprint->GetName().EndsWith(*TrackedBlueprintIter))
		{
			return true;
		}
	}
	return false;
}

bool FKismetEditorUtilities::IsActorValidForLevelScript(const AActor* Actor)
{
	return Actor && !FActorEditorUtils::IsABuilderBrush(Actor);
}

bool FKismetEditorUtilities::AnyBoundLevelScriptEventForActor(AActor* Actor, bool bCouldAddAny)
{
	if (IsActorValidForLevelScript(Actor))
	{
		for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			// Check for multicast delegates that we can safely assign
			if (!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintAssignable))
			{
				const FName EventName = Property->GetFName();
				const UK2Node_ActorBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForActor(Actor, EventName);
				if ((NULL != ExistingNode) != bCouldAddAny)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void FKismetEditorUtilities::AddLevelScriptEventOptionsForActor(class FMenuBuilder& MenuBuilder, TWeakObjectPtr<AActor> ActorPtr, bool bExistingEvents, bool bNewEvents, bool bOnlyEventName)
{
	struct FCreateEventForActorHelper
	{
		static void CreateEventForActor(TWeakObjectPtr<AActor> InActorPtr, FName EventName)
		{
			if (!GEditor->bIsSimulatingInEditor && GEditor->PlayWorld == NULL)
			{
				AActor* Actor = InActorPtr.Get();
				if (Actor != NULL && EventName != NAME_None)
				{
					const UK2Node_ActorBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForActor(Actor, EventName);
					if (ExistingNode != NULL)
					{
						FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ExistingNode);
					}
					else
					{
						FKismetEditorUtilities::CreateNewBoundEventForActor(Actor, EventName);
					}
				}
			}
		}
	};

	AActor* Actor = ActorPtr.Get();
	if (IsActorValidForLevelScript(Actor))
	{
		// Struct to store event properties by category
		struct FEventCategory
		{
			FString CategoryName;
			TArray<UProperty*> EventProperties;
		};
		// ARray of event properties by category
		TArray<FEventCategory> CategorizedEvents;

		// Find all events we can assign
		for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			// Check for multicast delegates that we can safely assign
			if (!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintAssignable))
			{
				// Get category for this property
				FString PropertyCategory = FObjectEditorUtils::GetCategory(Property);
				// See if we already have a list for this
				bool bFound = false;
				for (FEventCategory& Category : CategorizedEvents)
				{
					if(Category.CategoryName == PropertyCategory)
					{
						Category.EventProperties.Add(Property);
						bFound = true;
					}
				}
				// If not, create one
				if(!bFound)
				{
					FEventCategory NewCategory;
					NewCategory.CategoryName = PropertyCategory;
					NewCategory.EventProperties.Add(Property);
					CategorizedEvents.Add(NewCategory);
				}
			}
		}

		// Now build the menu
		for(FEventCategory& Category : CategorizedEvents)
		{
			MenuBuilder.BeginSection(NAME_None, FText::FromString(Category.CategoryName));

			for(UProperty* Property : Category.EventProperties)
			{
				const FName EventName = Property->GetFName();
				const UK2Node_ActorBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForActor(Actor, EventName);

				if ((!ExistingNode && !bNewEvents) || (ExistingNode && !bExistingEvents))
				{
					continue;
				}

				FText EntryText;
				if (bOnlyEventName)
				{
					EntryText = FText::FromName(EventName);
				}
				else
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("EventName"), FText::FromName(EventName));

					if (NULL == ExistingNode)
					{
						EntryText = FText::Format(LOCTEXT("AddEvent_ToolTip", "Add {EventName}"), Args);
					}
					else
					{
						EntryText = FText::Format(LOCTEXT("ViewEvent_ToolTip", "View {EventName}"), Args);
					}
				}

				// create menu entry
				MenuBuilder.AddMenuEntry(
					EntryText,
					Property->GetToolTipText(),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FCreateEventForActorHelper::CreateEventForActor, ActorPtr, EventName))
					);
			}

			MenuBuilder.EndSection();
		}
	}
}

void FKismetEditorUtilities::GetInformationOnMacro(UEdGraph* MacroGraph, /*out*/ UK2Node_Tunnel*& EntryNode, /*out*/ UK2Node_Tunnel*& ExitNode, bool& bIsMacroPure)
{
	check(MacroGraph);

	// Look at the graph for the entry & exit nodes
	TArray<UK2Node_Tunnel*> TunnelNodes;
	MacroGraph->GetNodesOfClass(TunnelNodes);

	for (int32 i = 0; i < TunnelNodes.Num(); i++)
	{
		UK2Node_Tunnel* Node = TunnelNodes[i];

		// Composite nodes should never be considered for function entry / exit, since we're searching for a graph's terminals
		if (Node->IsEditable() && !Node->IsA(UK2Node_Composite::StaticClass()))
		{
			if (Node->bCanHaveOutputs)
			{
				check(!EntryNode);
				EntryNode = Node;
			}
			else if (Node->bCanHaveInputs)
			{
				check(!ExitNode);
				ExitNode = Node;
			}
		}
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Determine the macro's purity
	//@TODO: May want to check what is *inside* a macro too, to determine it's relative purity
	bIsMacroPure = true;

	if (EntryNode != NULL)
	{
		for (int32 PinIndex = 0; PinIndex < EntryNode->Pins.Num(); ++PinIndex)
		{
			if (K2Schema->IsExecPin(*(EntryNode->Pins[PinIndex])))
			{
				bIsMacroPure = false;
				break;
			}
		}
	}

	if (bIsMacroPure && (ExitNode != NULL))
	{
		for (int32 PinIndex = 0; PinIndex < ExitNode->Pins.Num(); ++PinIndex)
		{
			if (K2Schema->IsExecPin(*(ExitNode->Pins[PinIndex])))
			{
				bIsMacroPure = false;
				break;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE 
