// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "Toolkits/AssetEditorManager.h"
#include "PackageTools.h"
#include "MessageLog.h"
#include "AssetRegistryModule.h"
#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"
#include "BehaviorTree/Decorators/BTDecorator_BlueprintBase.h"
#include "BehaviorTree/Services/BTService_BlueprintBase.h"
#include "HotReloadInterface.h"

const FString UBehaviorTreeEditorTypes::PinCategory_MultipleNodes("MultipleNodes");
const FString UBehaviorTreeEditorTypes::PinCategory_SingleComposite("SingleComposite");
const FString UBehaviorTreeEditorTypes::PinCategory_SingleTask("SingleTask");
const FString UBehaviorTreeEditorTypes::PinCategory_SingleNode("SingleNode");

#define LOCTEXT_NAMESPACE "SClassViewer"

FClassData::FClassData(UClass* InClass, const FString& InDeprecatedMessage) : 
	bIsHidden(0), 
	bHideParent(0), 
	Class(InClass), 
	DeprecatedMessage(InDeprecatedMessage)
{
	Category = GetCategory();
}

FClassData::FClassData(const FString& InAssetName, const FString& InGeneratedClassPackage, const FString& InClassName, UClass* InClass) :
	bIsHidden(0), 
	bHideParent(0), 
	Class(InClass), 
	AssetName(InAssetName), 
	GeneratedClassPackage(InGeneratedClassPackage), 
	ClassName(InClassName) 
{
	Category = GetCategory();
}

FString FClassData::ToString() const
{
	UClass* MyClass = Class.Get();
	if (MyClass)
	{
		FString ClassDesc = MyClass->GetName();

		if (MyClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
		{
			return ClassDesc.LeftChop(2);
		}

		const int32 ShortNameIdx = ClassDesc.Find(TEXT("_"));
		if (ShortNameIdx != INDEX_NONE)
		{
			ClassDesc = ClassDesc.Mid(ShortNameIdx + 1);
		}

		return ClassDesc;
	}

	return AssetName;
}

FString FClassData::GetClassName() const
{
	return Class.IsValid() ? Class->GetName() : ClassName;
}

FString FClassData::GetCategory() const
{
	return Class.IsValid() ? Class->GetMetaData(TEXT("Category")) : Category;
}

bool FClassData::IsAbstract() const
{
	return Class.IsValid() ? Class.Get()->HasAnyClassFlags(CLASS_Abstract) : false;
}

UClass* FClassData::GetClass(bool bSilent)
{
	UClass* RetClass = Class.Get();
	if (RetClass == NULL && GeneratedClassPackage.Len())
	{
		GWarn->BeginSlowTask(LOCTEXT("LoadPackage", "Loading Package..."), true);

		UPackage* Package = LoadPackage(NULL, *GeneratedClassPackage, LOAD_NoRedirects);
		if (Package)
		{
			Package->FullyLoad();

			UObject* Object = FindObject<UObject>(Package, *AssetName);

			GWarn->EndSlowTask();

			UBlueprint* BlueprintOb = Cast<UBlueprint>(Object);
			RetClass = BlueprintOb ? *BlueprintOb->GeneratedClass :
				Object ? Object->GetClass() : 
				NULL;

			Class = RetClass;
		}
		else
		{
			GWarn->EndSlowTask();

			if (!bSilent)
			{
				FMessageLog EditorErrors("EditorErrors");
				EditorErrors.Error(LOCTEXT("PackageLoadFail", "Package Load Failed"));
				EditorErrors.Info(FText::FromString(GeneratedClassPackage));
				EditorErrors.Notify(LOCTEXT("PackageLoadFail", "Package Load Failed"));
			}
		}
	}

	return RetClass;
}

//////////////////////////////////////////////////////////////////////////
FClassBrowseHelper::FOnPackageListUpdated FClassBrowseHelper::OnPackageListUpdated;
TArray<FName> FClassBrowseHelper::UnknownPackages;
bool FClassBrowseHelper::bMoreThanOneTaskClassAvailable = false;
bool FClassBrowseHelper::bMoreThanOneDecoratorClassAvailable = false;
bool FClassBrowseHelper::bMoreThanOneServiceClassAvailable = false;

void FClassDataNode::AddUniqueSubNode(TSharedPtr<FClassDataNode> SubNode)
{
	for (int32 Idx = 0; Idx < SubNodes.Num(); Idx++)
	{
		if (SubNode->Data.GetClassName() == SubNodes[Idx]->Data.GetClassName())
		{
			return;
		}
	}

	SubNodes.Add(SubNode);
}

FClassBrowseHelper::FClassBrowseHelper()
{
	// Register with the Asset Registry to be informed when it is done loading up files.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnFilesLoaded().AddRaw( this, &FClassBrowseHelper::InvalidateCache );
	AssetRegistryModule.Get().OnAssetAdded().AddRaw( this, &FClassBrowseHelper::OnAssetAdded);
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw( this, &FClassBrowseHelper::OnAssetRemoved );

	// Register to have Populate called when doing a Hot Reload.
	IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
	HotReloadSupport.OnHotReload().AddRaw(this, &FClassBrowseHelper::OnHotReload);

	// Register to have Populate called when a Blueprint is compiled.
	GEditor->OnBlueprintCompiled().AddRaw( this, &FClassBrowseHelper::InvalidateCache );
	GEditor->OnClassPackageLoadedOrUnloaded().AddRaw( this, &FClassBrowseHelper::InvalidateCache );

	UpdateAvailableNodeClasses();
}

FClassBrowseHelper::~FClassBrowseHelper()
{
	// Unregister with the Asset Registry to be informed when it is done loading up files.
	if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);
		AssetRegistryModule.Get().OnAssetAdded().RemoveAll(this);
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);

		// Unregister to have Populate called when doing a Hot Reload.
		if (FModuleManager::Get().IsModuleLoaded(TEXT("HotReload")))
		{
			IHotReloadInterface& HotReloadSupport = FModuleManager::GetModuleChecked<IHotReloadInterface>("HotReload");
			HotReloadSupport.OnHotReload().RemoveAll(this);
		}
	
		// Unregister to have Populate called when a Blueprint is compiled.
		if ( UObjectInitialized() )
		{
			// GEditor can't have been destructed before we call this or we'll crash.
			GEditor->OnBlueprintCompiled().RemoveAll(this);
			GEditor->OnClassPackageLoadedOrUnloaded().RemoveAll(this);
		}
	}
}

void FClassBrowseHelper::GatherClasses(const UClass* BaseClass, TArray<FClassData>& AvailableClasses)
{
	FBehaviorTreeEditorModule& BTEditorModule = FModuleManager::GetModuleChecked<FBehaviorTreeEditorModule>(TEXT("BehaviorTreeEditor"));
	FClassBrowseHelper* HelperInstance = BTEditorModule.GetClassCache().Get();
	if (HelperInstance)
	{
		const FString BaseClassName = BaseClass->GetName();
		if (!HelperInstance->RootNode.IsValid())
		{
			HelperInstance->BuildClassGraph();
		}

		TSharedPtr<FClassDataNode> BaseNode = HelperInstance->FindBaseClassNode(HelperInstance->RootNode, BaseClassName);
		HelperInstance->FindAllSubClasses(BaseNode, AvailableClasses);
	}
}

FString FClassBrowseHelper::GetDeprecationMessage(const UClass* Class)
{
	static FName MetaDeprecated = TEXT("DeprecatedNode");
	static FName MetaDeprecatedMessage = TEXT("DeprecationMessage");
	FString DefDeprecatedMessage("Please remove it!");
	FString DeprecatedPrefix("DEPRECATED");
	FString DeprecatedMessage;

	if (Class && Class->HasAnyClassFlags(CLASS_Native) && Class->HasMetaData(MetaDeprecated))
	{
		DeprecatedMessage = DeprecatedPrefix + TEXT(": ");
		DeprecatedMessage += Class->HasMetaData(MetaDeprecatedMessage) ? Class->GetMetaData(MetaDeprecatedMessage) : DefDeprecatedMessage;
	}

	return DeprecatedMessage;
}

DEFINE_LOG_CATEGORY_STATIC(LogBTDebug3, Log, All);

bool FClassBrowseHelper::IsClassKnown(const FClassData& ClassData)
{
	return !ClassData.IsBlueprint() || !UnknownPackages.Contains(*ClassData.GetPackageName());
}

void FClassBrowseHelper::AddUnknownClass(const FClassData& ClassData)
{
	if (ClassData.IsBlueprint())
	{
		UnknownPackages.AddUnique(*ClassData.GetPackageName());
	}
}

bool FClassBrowseHelper::IsHidingParentClass(UClass* Class)
{
	static FName MetaHideParent = TEXT("HideParentNode");
	return Class && Class->HasAnyClassFlags(CLASS_Native) && Class->HasMetaData(MetaHideParent);
}

bool FClassBrowseHelper::IsHidingClass(UClass* Class)
{
	static FName MetaHideInEditor = TEXT("HiddenNode");
	return Class && Class->HasAnyClassFlags(CLASS_Native) && Class->HasMetaData(MetaHideInEditor);
}

bool FClassBrowseHelper::IsPackageSaved(FName PackageName)
{
	const bool bFound = FPackageName::SearchForPackageOnDisk(PackageName.ToString());
	return bFound;
}

void FClassBrowseHelper::OnAssetAdded(const class FAssetData& AssetData)
{
	TSharedPtr<FClassDataNode> Node = CreateClassDataNode(AssetData);
	
	TSharedPtr<FClassDataNode> ParentNode;
	if (Node.IsValid())
	{
		ParentNode = FindBaseClassNode(RootNode, Node->ParentClassName);

		if (!IsPackageSaved(AssetData.PackageName))
		{
			UnknownPackages.AddUnique(AssetData.PackageName);
		}
		else
		{
			const int32 PrevListCount = UnknownPackages.Num();
			UnknownPackages.RemoveSingleSwap(AssetData.PackageName);

			if (UnknownPackages.Num() != PrevListCount)
			{
				FClassBrowseHelper::OnPackageListUpdated.Broadcast();
			}
		}
	}

	if (ParentNode.IsValid())
	{
		ParentNode->AddUniqueSubNode(Node);
		Node->ParentNode = ParentNode;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if(!AssetRegistryModule.Get().IsLoadingAssets())
	{
		UpdateAvailableNodeClasses();
	}
}

void FClassBrowseHelper::OnAssetRemoved(const class FAssetData& AssetData)
{
	const FString* GeneratedClassname = AssetData.TagsAndValues.Find("GeneratedClass");
	if (GeneratedClassname)
	{
		FString AssetClassName = *GeneratedClassname;
		UObject* Outer1(NULL);
		ResolveName(Outer1, AssetClassName, false, false);
		
		TSharedPtr<FClassDataNode> Node = FindBaseClassNode(RootNode, AssetClassName);
		if (Node.IsValid() && Node->ParentNode.IsValid())
		{
			Node->ParentNode->SubNodes.RemoveSingleSwap(Node);
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if(!AssetRegistryModule.Get().IsLoadingAssets())
	{
		UpdateAvailableNodeClasses();
	}
}

void FClassBrowseHelper::InvalidateCache()
{
	RootNode.Reset();

	UpdateAvailableNodeClasses();
}

void FClassBrowseHelper::OnHotReload( bool bWasTriggeredAutomatically )
{
	InvalidateCache();
}

TSharedPtr<FClassDataNode> FClassBrowseHelper::CreateClassDataNode(const class FAssetData& AssetData)
{
	TSharedPtr<FClassDataNode> Node;
	const FString* GeneratedClassname = AssetData.TagsAndValues.Find("GeneratedClass");
	const FString* ParentClassname = AssetData.TagsAndValues.Find("ParentClass");

	if (GeneratedClassname && ParentClassname)
	{
		FString AssetClassName = *GeneratedClassname;
		UObject* Outer1(NULL);
		ResolveName(Outer1, AssetClassName, false, false);

		FString AssetParentClassName = *ParentClassname;
		UObject* Outer2(NULL);
		ResolveName(Outer2, AssetParentClassName, false, false);

		Node = MakeShareable(new FClassDataNode);
		Node->ParentClassName = AssetParentClassName;

		UObject* AssetOb = AssetData.IsAssetLoaded() ? AssetData.GetAsset() : NULL;
		UBlueprint* AssetBP = Cast<UBlueprint>(AssetOb);
		UClass* AssetClass = AssetBP ? *AssetBP->GeneratedClass : AssetOb ? AssetOb->GetClass() : NULL;

		FClassData NewData(AssetData.AssetName.ToString(), AssetData.PackageName.ToString(), AssetClassName, AssetClass);
		Node->Data = NewData;
	}

	return Node;
}

TSharedPtr<FClassDataNode> FClassBrowseHelper::FindBaseClassNode(TSharedPtr<FClassDataNode> Node, const FString& ClassName)
{
	TSharedPtr<FClassDataNode> RetNode;
	if (Node.IsValid())
	{
		if (Node->Data.GetClassName() == ClassName)
		{
			return Node;
		}

		for (int32 i = 0; i < Node->SubNodes.Num(); i++)
		{
			RetNode = FindBaseClassNode(Node->SubNodes[i], ClassName);
			if (RetNode.IsValid())
			{
				break;
			}
		}
	}

	return RetNode;
}

void FClassBrowseHelper::FindAllSubClasses(TSharedPtr<FClassDataNode> Node, TArray<FClassData>& AvailableClasses)
{
	if (Node.IsValid())
	{
		if (!Node->Data.IsAbstract() && !Node->Data.IsDeprecated() && !Node->Data.bIsHidden)
		{
			AvailableClasses.Add(Node->Data);
		}

		for (int32 i = 0; i < Node->SubNodes.Num(); i++)
		{
			FindAllSubClasses(Node->SubNodes[i], AvailableClasses);
		}
	}
}

UClass* FClassBrowseHelper::FindAssetClass(const FString& GeneratedClassPackage, const FString& AssetName)
{
	UPackage* Package = FindPackage(NULL, *GeneratedClassPackage );
	if (Package)
	{
		UObject* Object = FindObject<UObject>(Package, *AssetName);
		if (Object)
		{
			UBlueprint* BlueprintOb = Cast<UBlueprint>(Object);
			return BlueprintOb ? *BlueprintOb->GeneratedClass : Object->GetClass();
		}
	}

	return NULL;
}

void FClassBrowseHelper::BuildClassGraph()
{
	UClass* RootNodeClass = UBTNode::StaticClass();

	TArray<TSharedPtr<FClassDataNode> > NodeList;
	TArray<UClass*> HideParentList;
	RootNode.Reset();

	// gather all native classes
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* TestClass = *It;
		if (TestClass->HasAnyClassFlags(CLASS_Native) && TestClass->IsChildOf(RootNodeClass))
		{
			TSharedPtr<FClassDataNode> NewNode = MakeShareable(new FClassDataNode);
			NewNode->ParentClassName = TestClass->GetSuperClass()->GetName();
			
			FString DeprecatedMessage = GetDeprecationMessage(TestClass);
			FClassData NewData(TestClass, DeprecatedMessage);
			
			NewData.bHideParent = IsHidingParentClass(TestClass);
			if (NewData.bHideParent)
			{
				HideParentList.Add(TestClass->GetSuperClass());
			}

			NewData.bIsHidden = IsHidingClass(TestClass);

			NewNode->Data = NewData;

			if (TestClass == RootNodeClass)
			{
				RootNode = NewNode;
			}

			NodeList.Add(NewNode);
		}
	}

	// find all hidden parent classes
	for (int32 i = 0; i < NodeList.Num(); i++)
	{
		TSharedPtr<FClassDataNode> TestNode = NodeList[i];
		if (HideParentList.Contains(TestNode->Data.GetClass()))
		{
			TestNode->Data.bIsHidden = true;
		}
	}

	// gather all blueprints
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> BlueprintList;

	FARFilter Filter;
	Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
	AssetRegistryModule.Get().GetAssets(Filter, BlueprintList);

	for (int32 i = 0; i < BlueprintList.Num(); i++)
	{
		TSharedPtr<FClassDataNode> NewNode = CreateClassDataNode(BlueprintList[i]);
		NodeList.Add(NewNode);
	}

	// build class tree
	AddClassGraphChildren(RootNode, NodeList);
}

void FClassBrowseHelper::AddClassGraphChildren(TSharedPtr<FClassDataNode> Node, TArray<TSharedPtr<FClassDataNode> >& NodeList)
{
	if (!Node.IsValid())
	{
		return;
	}

	const FString NodeClassName = Node->Data.GetClassName();
	for (int32 i = NodeList.Num() - 1; i >= 0; i--)
	{
		if (NodeList[i]->ParentClassName == NodeClassName)
		{
			TSharedPtr<FClassDataNode> MatchingNode = NodeList[i];
			NodeList.RemoveAt(i);

			MatchingNode->ParentNode = Node;
			Node->SubNodes.Add(MatchingNode);

			AddClassGraphChildren(MatchingNode, NodeList);
		}
	}
}

bool FClassBrowseHelper::IsMoreThanOneTaskClassAvailable()
{
	return bMoreThanOneTaskClassAvailable;
}

bool FClassBrowseHelper::IsMoreThanOneDecoratorClassAvailable()
{
	return bMoreThanOneDecoratorClassAvailable;
}

bool FClassBrowseHelper::IsMoreThanOneServiceClassAvailable()
{
	return bMoreThanOneServiceClassAvailable;
}

void FClassBrowseHelper::UpdateAvailableNodeClasses()
{
	if(FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		const bool bSearchSubClasses = true;
		{
			TArray<FName> ClassNames;
			ClassNames.Add(UBTTask_BlueprintBase::StaticClass()->GetFName());
			TSet<FName> DerivedClassNames;
			AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);
			bMoreThanOneTaskClassAvailable = DerivedClassNames.Num() > 1;
		}

		{
			TArray<FName> ClassNames;
			ClassNames.Add(UBTDecorator_BlueprintBase::StaticClass()->GetFName());
			TSet<FName> DerivedClassNames;
			AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);
			bMoreThanOneDecoratorClassAvailable = DerivedClassNames.Num() > 1;
		}

		{
			TArray<FName> ClassNames;
			ClassNames.Add(UBTService_BlueprintBase::StaticClass()->GetFName());
			TSet<FName> DerivedClassNames;
			AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);
			bMoreThanOneServiceClassAvailable = DerivedClassNames.Num() > 1;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

UBehaviorTreeEditorTypes::UBehaviorTreeEditorTypes(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

#undef LOCTEXT_NAMESPACE
