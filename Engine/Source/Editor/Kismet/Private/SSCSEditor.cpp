// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "BlueprintEditorPrivatePCH.h"
#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "BlueprintUtilities.h"
#include "ComponentAssetBroker.h"

#include "SSCSEditor.h"
#include "SKismetInspector.h"
#include "SSCSEditorViewport.h"
#include "SComponentClassCombo.h"
#include "PropertyPath.h"

#include "AssetSelection.h"
#include "Editor/SceneOutliner/Private/SSocketChooser.h"
#include "Factories.h"
#include "ScopedTransaction.h"

#include "DragAndDrop/AssetDragDropOp.h"
#include "ClassIconFinder.h"

#include "ObjectTools.h"

#include "IDocumentation.h"
#include "Kismet2NameValidators.h"
#include "UnrealExporter.h"
#include "TutorialMetaData.h"
#include "SInlineEditableTextBlock.h"
#include "GenericCommands.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "SSCSEditor"

static const FName SCS_ColumnName_ComponentClass( "ComponentClass" );
static const FName SCS_ColumnName_Asset( "Asset" );
static const FName SCS_ColumnName_Mobility( "Mobility" );

//////////////////////////////////////////////////////////////////////////
// SSCSEditorDragDropTree
void SSCSEditorDragDropTree::Construct( const FArguments& InArgs )
{
	SCSEditor = InArgs._SCSEditor;

	STreeView<FSCSEditorTreeNodePtrType>::FArguments BaseArgs;
	BaseArgs.OnGenerateRow( InArgs._OnGenerateRow )
			.OnItemScrolledIntoView( InArgs._OnItemScrolledIntoView )
			.OnGetChildren( InArgs._OnGetChildren )
			.TreeItemsSource( InArgs._TreeItemsSource )
			.ItemHeight( InArgs._ItemHeight )
			.OnContextMenuOpening( InArgs._OnContextMenuOpening )
			.OnMouseButtonDoubleClick( InArgs._OnMouseButtonDoubleClick )
			.OnSelectionChanged( InArgs._OnSelectionChanged )
			.OnExpansionChanged( InArgs._OnExpansionChanged )
			.SelectionMode( InArgs._SelectionMode )
			.HeaderRow( InArgs._HeaderRow )
			.ClearSelectionOnClick( InArgs._ClearSelectionOnClick )
			.ExternalScrollbar( InArgs._ExternalScrollbar );

	STreeView<FSCSEditorTreeNodePtrType>::Construct( BaseArgs );
}

FReply SSCSEditorDragDropTree::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	FReply Handled = FReply::Unhandled();

	if ( SCSEditor != NULL )
	{
		const bool bIsValidDrag = DragDropEvent.GetOperationAs<FExternalDragOperation>().IsValid();
		if (bIsValidDrag)
		{
			Handled = AssetUtil::CanHandleAssetDrag(DragDropEvent);
		}
	}

	return Handled;
}

FReply SSCSEditorDragDropTree::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) 
{
	FReply Handled = FReply::Unhandled();

	if ( SCSEditor != NULL )
	{
		TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
		if (Operation.IsValid() && (Operation->IsOfType<FExternalDragOperation>() || Operation->IsOfType<FAssetDragDropOp>()))
		{
			TArray< FAssetData > DroppedAssetData = AssetUtil::ExtractAssetDataFromDrag( DragDropEvent );
			const int32 NumAssets = DroppedAssetData.Num();

			if ( NumAssets > 0 )
			{
				GWarn->BeginSlowTask( LOCTEXT("LoadingComponents", "Loading Component(s)"), true );

				for (int32 DroppedAssetIdx = 0; DroppedAssetIdx < NumAssets; ++DroppedAssetIdx)
				{
					const FAssetData& AssetData = DroppedAssetData[DroppedAssetIdx];
				
					TSubclassOf<UActorComponent>  ComponentClasses = FComponentAssetBrokerage::GetPrimaryComponentForAsset( AssetData.GetClass() );
					if ( NULL != ComponentClasses )
					{
						GWarn->StatusUpdate( DroppedAssetIdx, NumAssets, FText::Format( LOCTEXT("LoadingComponent", "Loading Component {0}"), FText::FromName( AssetData.AssetName ) ) );
						SCSEditor->AddNewComponent(ComponentClasses, AssetData.GetAsset() );
					}
				}

				GWarn->EndSlowTask();
			}

			Handled = FReply::Handled();
		}
	}

	return Handled;
}



//////////////////////////////////////////////////////////////////////////
//

class FSCSRowDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FSCSRowDragDropOp, FDragDropOperation)

	/** Available drop actions */
	enum EDropActionType
	{
		DropAction_None,
		DropAction_AttachTo,
		DropAction_DetachFrom,
		DropAction_MakeNewRoot,
		DropAction_AttachToOrMakeNewRoot
	};

	/** Node(s) that we started the drag from */
	TArray<FSCSEditorTreeNodePtrType> SourceNodes;

	/** String to show as hover text */
	FText CurrentHoverText;

	/** The type of drop action that's pending while dragging */
	EDropActionType PendingDropAction;
	
	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			[				
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0,0,3,0)
				[
					SNew(SImage)
					.Image(this, &FSCSRowDragDropOp::GetIcon)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock) 
					.Text(this, &FSCSRowDragDropOp::GetHoverText)
				]
			];
	}

	FText GetHoverText() const
	{
		return !CurrentHoverText.IsEmpty()
			? CurrentHoverText
			: LOCTEXT("DropActionToolTip_InvalidDropTarget", "Cannot drop here.");
	}

	const FSlateBrush* GetIcon() const
	{
		return PendingDropAction != DropAction_None
			? FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"))
			: FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
	}

	static TSharedRef<FSCSRowDragDropOp> New()
	{
		TSharedPtr<FSCSRowDragDropOp> Operation = MakeShareable(new FSCSRowDragDropOp);
		Operation->Construct();
		return Operation.ToSharedRef();
	}
};

//////////////////////////////////////////////////////////////////////////
// FSCSEditorTreeNode

FSCSEditorTreeNode::FSCSEditorTreeNode()
	:bIsInherited(false)
	,bNonTransactionalRename(false)
{
}

FSCSEditorTreeNode::FSCSEditorTreeNode(USCS_Node* InSCSNode, bool bInIsInherited)
	:bIsInherited(bInIsInherited)
	,SCSNodePtr(InSCSNode)
	,ComponentTemplatePtr(InSCSNode != NULL ? InSCSNode->ComponentTemplate : NULL)
	,bNonTransactionalRename(false)
{
}

FSCSEditorTreeNode::FSCSEditorTreeNode(UActorComponent* InComponentTemplate)
	:bIsInherited(false)
	,SCSNodePtr(NULL)
	,ComponentTemplatePtr(InComponentTemplate)
	,bNonTransactionalRename(false)
{
}

FName FSCSEditorTreeNode::GetVariableName() const
{
	FName VariableName = NAME_None;

	USCS_Node* SCS_Node = GetSCSNode();
	UActorComponent* ComponentTemplate = GetComponentTemplate();

	if(SCS_Node != NULL)
	{
		// Use the same variable name as is obtained by the compiler
		VariableName = SCS_Node->GetVariableName();
	}
	else if(ComponentTemplate != NULL)
	{
		// If the owner class is a Blueprint class, see if there's a corresponding object property that contains the component template
		check(ComponentTemplate->GetOwner());
		UClass* OwnerClass = ComponentTemplate->GetOwner()->GetActorClass();
		if(OwnerClass != NULL)
		{
			UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(OwnerClass);
			if(Blueprint != NULL && Blueprint->ParentClass != NULL)
			{
				for ( TFieldIterator<UProperty> It(Blueprint->ParentClass); It; ++It )
				{
					UProperty* Property  = *It;
					if(UObjectProperty* ObjectProp = Cast<UObjectProperty>(Property))
					{
						UObject* CDO = Blueprint->ParentClass->GetDefaultObject();
						UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(CDO));

						if(Object)
						{
							if(Object->GetClass() != ComponentTemplate->GetClass())
							{
								continue;
							}

							if(Object->GetFName() == ComponentTemplate->GetFName())
							{
								VariableName = ObjectProp->GetFName();
								break;
							}
						}
					}
				}
			}
		}
	}

	return VariableName;
}

FString FSCSEditorTreeNode::GetDisplayString() const
{
	FName VariableName = GetVariableName();
	UActorComponent* ComponentTemplate = GetComponentTemplate();

	// Only display SCS node variable names in the tree if they have not been autogenerated
	if (VariableName != NAME_None)
	{
		return VariableName.ToString();
	}
	else if(IsNative() && ComponentTemplate != NULL)
	{
		return ComponentTemplate->GetFName().ToString();
	}
	else
	{
		FString UnnamedString = LOCTEXT("UnnamedToolTip", "Unnamed").ToString();
		FString NativeString = IsNative() ? LOCTEXT("NativeToolTip", "Native ").ToString() : TEXT("");

		if(ComponentTemplate != NULL)
		{
			return FString::Printf(TEXT("[%s %s%s]"), *UnnamedString, *NativeString, *ComponentTemplate->GetClass()->GetName());
		}
		else
		{
			return FString::Printf(TEXT("[%s %s]"), *UnnamedString, *NativeString);
		}
	}
}

UActorComponent* FSCSEditorTreeNode::FindComponentInstanceInActor(const AActor* InActor, bool bIsPreviewActor) const
{
	USCS_Node* SCS_Node = GetSCSNode();
	UActorComponent* ComponentTemplate = GetComponentTemplate();

	UActorComponent* ComponentInstance = NULL;
	if(InActor != NULL)
	{
		if(SCS_Node != NULL)
		{
			FName VariableName = SCS_Node->GetVariableName();
			if(VariableName != NAME_None)
			{
				UObjectPropertyBase* Property = FindField<UObjectPropertyBase>(InActor->GetClass(), VariableName);
				if(Property != NULL)
				{
					// Return the component instance that's stored in the property with the given variable name
					ComponentInstance = Cast<UActorComponent>(Property->GetObjectPropertyValue_InContainer(InActor));
				}
				else if(bIsPreviewActor)
				{
					// If this is the preview actor, return the cached component instance that's being used for the preview actor prior to recompiling the Blueprint
					ComponentInstance = SCS_Node->EditorComponentInstance;
				}
			}
		}
		else if(ComponentTemplate != NULL)
		{
			// Look for a native component instance with a name that matches the template name
			TArray<UActorComponent*> Components;
			InActor->GetComponents(Components);

			for(auto It = Components.CreateConstIterator(); It; ++It)
			{
				UActorComponent* Component = *It;
				if(Component->GetFName() == ComponentTemplate->GetFName())
				{
					ComponentInstance = Component;
					break;
				}
			}
		}
	}

	return ComponentInstance;
}

UBlueprint* FSCSEditorTreeNode::GetBlueprint() const
{
	USCS_Node* SCS_Node = GetSCSNode();
	UActorComponent* ComponentTemplate = GetComponentTemplate();

	if(SCS_Node)
	{
		USimpleConstructionScript* SCS = SCS_Node->GetSCS();
		if(SCS)
		{
			return SCS->GetBlueprint();
		}
	}
	else if(ComponentTemplate)
	{
		AActor* CDO = ComponentTemplate->GetOwner();
		if(CDO)
		{
			check(CDO->GetClass());

			return Cast<UBlueprint>(CDO->GetClass()->ClassGeneratedBy);
		}
	}

	return NULL;
}

bool FSCSEditorTreeNode::IsRoot() const
{
	bool bIsRoot = true;
	USCS_Node* SCS_Node = GetSCSNode();
	UActorComponent* ComponentTemplate = GetComponentTemplate();

	if(SCS_Node != NULL)
	{
		USimpleConstructionScript* SCS = SCS_Node->GetSCS();
		if(SCS != NULL)
		{
			// Evaluate to TRUE if we have an SCS node reference, it is contained in the SCS root set and does not have an external parent
			bIsRoot = SCS->GetRootNodes().Contains(SCS_Node) && SCS_Node->ParentComponentOrVariableName == NAME_None;
		}
	}
	else if(ComponentTemplate != NULL)
	{
		AActor* CDO = ComponentTemplate->GetOwner();
		if(CDO != NULL)
		{
			// Evaluate to TRUE if we have a valid component reference that matches the native root component
			bIsRoot = (ComponentTemplate == CDO->GetRootComponent());
		}
	}

	return bIsRoot;
}

bool FSCSEditorTreeNode::IsAttachedTo(FSCSEditorTreeNodePtrType InNodePtr) const
{ 
	FSCSEditorTreeNodePtrType TestParentPtr = ParentNodePtr;
	while(TestParentPtr.IsValid())
	{
		if(TestParentPtr == InNodePtr)
		{
			return true;
		}

		TestParentPtr = TestParentPtr->ParentNodePtr;
	}

	return false; 
}

bool FSCSEditorTreeNode::IsDefaultSceneRoot() const
{
	USCS_Node* SCS_Node = GetSCSNode();
	if(SCS_Node != NULL)
	{
		USimpleConstructionScript* SCS = SCS_Node->GetSCS();
		if(SCS != NULL)
		{
			return SCS_Node == SCS->GetDefaultSceneRootNode();
		}
	}

	return false;
}

bool FSCSEditorTreeNode::CanEditDefaults() const
{
	bool bCanEdit = false;
	USCS_Node* SCS_Node = GetSCSNode();
	UActorComponent* ComponentTemplate = GetComponentTemplate();

	if(!IsNative())
	{
		// Evaluate to TRUE for non-native nodes if it represents a valid SCS node and it is not inherited from a parent Blueprint
		bCanEdit = SCS_Node != NULL && !IsInherited();
	}
	else if(ComponentTemplate != NULL)
	{
		// Evaluate to TRUE for native nodes if it is bound to a member variable and that variable has either EditDefaultsOnly or EditAnywhere flags set
		check(ComponentTemplate->GetOwner());
		UClass* OwnerClass = ComponentTemplate->GetOwner()->GetActorClass();
		if(OwnerClass != NULL)
		{
			UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(OwnerClass);
			if(Blueprint != NULL && Blueprint->ParentClass != NULL)
			{
				for(TFieldIterator<UProperty> It(Blueprint->ParentClass); It; ++It)
				{
					UProperty* Property = *It;
					if(UObjectProperty* ObjectProp = Cast<UObjectProperty>(Property))
					{
						//must be editable
						if((Property->PropertyFlags & (CPF_Edit)) == 0)
						{
							continue;
						}

						UObject* ParentCDO = Blueprint->ParentClass->GetDefaultObject();

						if(!ComponentTemplate->GetClass()->IsChildOf(ObjectProp->PropertyClass))
						{
							continue;
						}

						UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(ParentCDO));
						bCanEdit = Object != NULL && Object->GetFName() == ComponentTemplate->GetFName();

						if (bCanEdit)
						{
							break;
						}
					}
				}
			}
		}
	}

	return bCanEdit;
}

FSCSEditorTreeNodePtrType FSCSEditorTreeNode::FindClosestParent(TArray<FSCSEditorTreeNodePtrType> InNodes)
{
	uint32 MinDepth = MAX_uint32;
	FSCSEditorTreeNodePtrType ClosestParentNodePtr;

	for(int32 i = 0; i < InNodes.Num() && MinDepth > 1; ++i)
	{
		if(InNodes[i].IsValid())
		{
			uint32 CurDepth = 0;
			if(InNodes[i]->FindChild(GetComponentTemplate(), true, &CurDepth).IsValid())
			{
				if(CurDepth < MinDepth)
				{
					MinDepth = CurDepth;
					ClosestParentNodePtr = InNodes[i];
				}
			}
		}
	}

	return ClosestParentNodePtr;
}

void FSCSEditorTreeNode::AddChild(FSCSEditorTreeNodePtrType InChildNodePtr)
{
	USCS_Node* SCS_Node = GetSCSNode();
	UActorComponent* ComponentTemplate = GetComponentTemplate();

	// Ensure the node is not already parented elsewhere
	if(InChildNodePtr->GetParent().IsValid())
	{
		InChildNodePtr->GetParent()->RemoveChild(InChildNodePtr);
	}

	// Add the given node as a child and link its parent
	Children.AddUnique(InChildNodePtr);
	InChildNodePtr->ParentNodePtr = AsShared();

	// Add a child node to the SCS tree node if not already present
	USCS_Node* SCS_ChildNode = InChildNodePtr->GetSCSNode();
	if(SCS_ChildNode != NULL)
	{
		// Get the SCS instance that owns the child node
		USimpleConstructionScript* SCS = SCS_ChildNode->GetSCS();
		if(SCS != NULL)
		{
			// If the parent is also a valid SCS node
			if(SCS_Node != NULL)
			{
				// If the parent and child are both owned by the same SCS instance
				if(SCS_Node->GetSCS() == SCS)
				{
					// Add the child into the parent's list of children
					if(!SCS_Node->ChildNodes.Contains(SCS_ChildNode))
					{
						SCS_Node->AddChildNode(SCS_ChildNode);
					}
				}
				else
				{
					// Adds the child to the SCS root set if not already present
					SCS->AddNode(SCS_ChildNode);

					// Set parameters to parent this node to the "inherited" SCS node
					SCS_ChildNode->SetParent(SCS_Node);
				}
			}
			else if(ComponentTemplate != NULL)
			{
				// Adds the child to the SCS root set if not already present
				SCS->AddNode(SCS_ChildNode);

				// Set parameters to parent this node to the native component template
				SCS_ChildNode->SetParent(Cast<USceneComponent>(ComponentTemplate));
			}
			else
			{
				// Adds the child to the SCS root set if not already present
				SCS->AddNode(SCS_ChildNode);
			}
		}
	}
}

FSCSEditorTreeNodePtrType FSCSEditorTreeNode::AddChild(USCS_Node* InSCSNode, bool bInIsInherited)
{
	// Ensure that the given SCS node is valid
	check(InSCSNode != NULL);

	// If it doesn't already exist as a child node
	FSCSEditorTreeNodePtrType ChildNodePtr = FindChild(InSCSNode);
	if(!ChildNodePtr.IsValid())
	{
		// Add a child node to the SCS editor tree
		AddChild(ChildNodePtr = MakeShareable(new FSCSEditorTreeNode(InSCSNode, bInIsInherited)));
	}

	return ChildNodePtr;
}

FSCSEditorTreeNodePtrType FSCSEditorTreeNode::AddChild(UActorComponent* InComponentTemplate)
{
	// Ensure that the given component template is valid
	check(InComponentTemplate != NULL);

	// If it doesn't already exist in the SCS editor tree
	FSCSEditorTreeNodePtrType ChildNodePtr = FindChild(InComponentTemplate);
	if(!ChildNodePtr.IsValid())
	{
		// Add a child node to the SCS editor tree
		AddChild(ChildNodePtr = MakeShareable(new FSCSEditorTreeNode(InComponentTemplate)));
	}

	return ChildNodePtr;
}

FSCSEditorTreeNodePtrType FSCSEditorTreeNode::FindChild(const USCS_Node* InSCSNode, bool bRecursiveSearch, uint32* OutDepth) const
{
	FSCSEditorTreeNodePtrType Result;

	// Ensure that the given SCS node is valid
	if(InSCSNode != NULL)
	{
		// Look for a match in our set of child nodes
		for(int32 ChildIndex = 0; ChildIndex < Children.Num() && !Result.IsValid(); ++ChildIndex)
		{
			if(InSCSNode == Children[ChildIndex]->GetSCSNode())
			{
				Result = Children[ChildIndex];
			}
			else if(bRecursiveSearch)
			{
				Result = Children[ChildIndex]->FindChild(InSCSNode, true, OutDepth);
			}
		}
	}

	if(OutDepth && Result.IsValid())
	{
		*OutDepth += 1;
	}

	return Result;
}

FSCSEditorTreeNodePtrType FSCSEditorTreeNode::FindChild(const UActorComponent* InComponentTemplate, bool bRecursiveSearch, uint32* OutDepth) const
{
	FSCSEditorTreeNodePtrType Result;

	// Ensure that the given component template is valid
	if(InComponentTemplate != NULL)
	{
		// Look for a match in our set of child nodes
		for(int32 ChildIndex = 0; ChildIndex < Children.Num() && !Result.IsValid(); ++ChildIndex)
		{
			if(InComponentTemplate == Children[ChildIndex]->GetComponentTemplate())
			{
				Result = Children[ChildIndex];
			}
			else if(bRecursiveSearch)
			{
				Result = Children[ChildIndex]->FindChild(InComponentTemplate, true, OutDepth);
			}
		}
	}

	if(OutDepth && Result.IsValid())
	{
		*OutDepth += 1;
	}

	return Result;
}

void FSCSEditorTreeNode::RemoveChild(FSCSEditorTreeNodePtrType InChildNodePtr)
{
	// Remove the given node as a child and reset its parent link
	Children.Remove(InChildNodePtr);
	InChildNodePtr->ParentNodePtr.Reset();

	// Remove the SCS node from the SCS tree, if present
	USCS_Node* SCS_ChildNode = InChildNodePtr->GetSCSNode();
	if(SCS_ChildNode != NULL)
	{
		USimpleConstructionScript* SCS = SCS_ChildNode->GetSCS();
		if(SCS != NULL)
		{
			SCS->RemoveNode(SCS_ChildNode);
		}
	}
}

void FSCSEditorTreeNode::OnRequestRename(bool bTransactional)
{
	bNonTransactionalRename = !bTransactional;
	RenameRequestedDelegate.ExecuteIfBound();
}

void FSCSEditorTreeNode::OnCompleteRename(const FText& InNewName)
{
	FScopedTransaction* TransactionContext = NULL;
	if(bNonTransactionalRename)
	{
		// Reset for next time through - if the next rename operation is not explicitly initiated by OnRequestRename(), then the rename must always be transactional.
		bNonTransactionalRename = false;
	}
	else
	{
		TransactionContext = new FScopedTransaction(LOCTEXT("RenameComponentVariable", "Rename Component Variable"));
	}

	FBlueprintEditorUtils::RenameComponentMemberVariable(GetBlueprint(), GetSCSNode(), FName( *InNewName.ToString() ));

	if(TransactionContext)
	{
		delete TransactionContext;
	}
}

//////////////////////////////////////////////////////////////////////////
// FSCSEditorComponentObjectTextFactory

struct FSCSEditorComponentObjectTextFactory : public FCustomizableTextObjectFactory
{
	// Child->Parent name map
	TMap<FName, FName> ParentMap;

	// Name->Instance object mapping
	TMap<FName, UActorComponent*> NewObjectMap;

	// Determine whether or not scene components in the new object set can be attached to the given scene root component
	bool CanAttachComponentsTo(USceneComponent* InRootComponent)
	{
		check(InRootComponent);

		// For each component in the set, check against the given root component and break if we fail to validate
		bool bCanAttachToRoot = true;
		for(auto NewComponentIt = NewObjectMap.CreateConstIterator(); NewComponentIt && bCanAttachToRoot; ++NewComponentIt)
		{
			// If this is a scene component, and it does not already have a parent within the set
			USceneComponent* SceneComponent = Cast<USceneComponent>(NewComponentIt->Value);
			if(SceneComponent != NULL && !ParentMap.Contains(SceneComponent->GetFName()))
			{
				// Determine if we are allowed to attach the scene component to the given root component
				bCanAttachToRoot = InRootComponent->CanAttachAsChild(SceneComponent, NAME_None)
					&& SceneComponent->Mobility >= InRootComponent->Mobility
					&& (!InRootComponent->IsEditorOnly() || SceneComponent->IsEditorOnly());
			}
		}

		return bCanAttachToRoot;
	}

	// Constructs a new object factory from the given text buffer
	static TSharedRef<FSCSEditorComponentObjectTextFactory> Get(FString InTextBuffer)
	{
		// Construct a new instance
		TSharedPtr<FSCSEditorComponentObjectTextFactory> FactoryPtr = MakeShareable(new FSCSEditorComponentObjectTextFactory());
		check(FactoryPtr.IsValid());

		// Create new objects if we're allowed to
		if(FactoryPtr->CanCreateObjectsFromText(InTextBuffer))
		{
			// Use the transient package initially for creating the objects, since the variable name is used when copying
			FactoryPtr->ProcessBuffer(GetTransientPackage(), RF_ArchetypeObject|RF_Transactional, InTextBuffer);
		}

		return FactoryPtr.ToSharedRef();
	}

	virtual ~FSCSEditorComponentObjectTextFactory() {}

protected:	
	// Constructor; protected to only allow this class to instance itself
	FSCSEditorComponentObjectTextFactory()
		:FCustomizableTextObjectFactory(GWarn)
	{
	}

	// FCustomizableTextObjectFactory implementation

	virtual bool CanCreateClass(UClass* ObjectClass) const override
	{
		// Only allow actor component types to be created
		return ObjectClass->IsChildOf(UActorComponent::StaticClass());
	}

	virtual void ProcessConstructedObject(UObject* NewObject) override
	{
		check(NewObject);

		// Add it to the new object map
		NewObjectMap.Add(NewObject->GetFName(), Cast<UActorComponent>(NewObject));

		// If this is a scene component and it has a parent
		USceneComponent* SceneComponent = Cast<USceneComponent>(NewObject);
		if(SceneComponent && SceneComponent->AttachParent)
		{
			// Add an entry to the child->parent name map
			ParentMap.Add(NewObject->GetFName(), SceneComponent->AttachParent->GetFName());

			// Clear this so it isn't used when constructing the new SCS node
			SceneComponent->AttachParent = NULL;
		}
	}

	// FCustomizableTextObjectFactory (end)
};

//////////////////////////////////////////////////////////////////////////
// SSCS_RowWidget

void SSCS_RowWidget::Construct( const FArguments& InArgs, TSharedPtr<SSCSEditor> InSCSEditor, FSCSEditorTreeNodePtrType InNodePtr, TSharedPtr<STableViewBase> InOwnerTableView  )
{
	check(InNodePtr.IsValid());

	SCSEditor = InSCSEditor;
	NodePtr = InNodePtr;

	SMultiColumnTableRow<FSCSEditorTreeNodePtrType>::Construct( FSuperRowType::FArguments().Padding(FMargin(0.f, 0.f, 0.f, 4.f)), InOwnerTableView.ToSharedRef() );
}

SSCS_RowWidget::~SSCS_RowWidget()
{
	// Clear delegate when widget goes away
	//Ask SCSEditor if Node is still active, if it isn't it might have been collected so we can't do anything to it
	TSharedPtr<SSCSEditor> Editor = SCSEditor.Pin();
	if(Editor.IsValid())
	{
		USCS_Node* SCS_Node = NodePtr->GetSCSNode();
		if(SCS_Node != NULL && Editor->IsNodeInSimpleConstructionScript(SCS_Node))
		{
			SCS_Node->SetOnNameChanged(FSCSNodeNameChanged());
		}
	}
}


TSharedRef<SWidget> SSCS_RowWidget::GenerateWidgetForColumn( const FName& ColumnName )
{
	if(ColumnName == SCS_ColumnName_ComponentClass)
	{
		// Setup a default icon brush.
		const FSlateBrush* ComponentIcon = FEditorStyle::GetBrush("SCS.NativeComponent");
		if(NodePtr->GetComponentTemplate() != NULL)
		{
			ComponentIcon = FClassIconFinder::FindIconForClass( NodePtr->GetComponentTemplate()->GetClass(), TEXT("SCS.Component") );
		}

		TSharedPtr<SInlineEditableTextBlock> InlineWidget = 
			SNew(SInlineEditableTextBlock)
				.Text(this, &SSCS_RowWidget::GetNameLabel)
				.OnVerifyTextChanged( this, &SSCS_RowWidget::OnNameTextVerifyChanged )
				.OnTextCommitted( this, &SSCS_RowWidget::OnNameTextCommit )
				.IsSelected( this, &SSCS_RowWidget::IsSelectedExclusively )
				.IsReadOnly( !NodePtr->CanRename() || (SCSEditor.IsValid() && !SCSEditor.Pin()->InEditingMode()) );

		NodePtr->SetRenameRequestedDelegate(FSCSEditorTreeNode::FOnRenameRequested::CreateSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode));

		return	SNew(SHorizontalBox)
				.ToolTip(IDocumentation::Get()->CreateToolTip(
					TAttribute<FText>(this, &SSCS_RowWidget::GetTooltipText),
					NULL,
					GetDocumentationLink(),
					GetDocumentationExcerptName()))
				+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SExpanderArrow, SharedThis(this))
					]
				+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(ComponentIcon)
						.ColorAndOpacity(this, &SSCS_RowWidget::GetColorTint)
					]
				+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4,0,4,0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RootLabel", "[ROOT]"))
						.Visibility(this, &SSCS_RowWidget::GetRootLabelVisibility)
					]
				+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						InlineWidget.ToSharedRef()
					];
	}
	else if(ColumnName == SCS_ColumnName_Asset)
	{
		return
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 0, 0)
			[
				SNew(STextBlock)
				.Visibility(this, &SSCS_RowWidget::GetAssetVisibility)
				.Text(this, &SSCS_RowWidget::GetAssetName)
				.ToolTipText(this, &SSCS_RowWidget::GetAssetNameTooltip)
			];
	}
	else if (ColumnName == SCS_ColumnName_Mobility)
	{
		TWeakObjectPtr<USCS_Node> SCSNode(NodePtr->GetSCSNode());

		TSharedPtr<SToolTip> MobilityTooltip;
		SAssignNew(MobilityTooltip, SToolTip)
			.Text(this, &SSCS_RowWidget::GetMobilityToolTipText, SCSNode);

		return	SNew(SHorizontalBox)
					.ToolTip(MobilityTooltip)
					.Visibility(EVisibility::Visible) // so we still get tooltip text for an empty SHorizontalBox
				+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SImage)
							.Image(this, &SSCS_RowWidget::GetMobilityIconImage, SCSNode)
							.ToolTip(MobilityTooltip)
					];
	}
	else
	{
		return	SNew(STextBlock)
				.Text( LOCTEXT("UnknownColumn", "Unknown Column") );
	}
}

FSlateBrush const* SSCS_RowWidget::GetMobilityIconImage(TWeakObjectPtr<USCS_Node> SCSNode) const
{
	if (!SCSNode.IsValid())
	{
		return NULL;
	}

	USceneComponent* SceneComponentTemplate = Cast<USceneComponent>(SCSNode->ComponentTemplate);
	if (SceneComponentTemplate == NULL)
	{
		return NULL;
	}

	if (SceneComponentTemplate->Mobility == EComponentMobility::Movable)
	{
		return FEditorStyle::GetBrush(TEXT("ClassIcon.MovableMobilityIcon"));
	}
	else if (SceneComponentTemplate->Mobility == EComponentMobility::Stationary)
	{
		return FEditorStyle::GetBrush(TEXT("ClassIcon.StationaryMobilityIcon"));
	}

	// static components don't get an icon (because static is the most common
	// mobility type, and we'd like to keep the icon clutter to a minimum)
	return NULL;
}

FText SSCS_RowWidget::GetMobilityToolTipText(TWeakObjectPtr<USCS_Node> SCSNode) const
{
	FText MobilityToolTip = LOCTEXT("NoMobilityTooltip", "This component does not have 'Mobility' associated with it");
	if (SCSNode.IsValid())
	{
		USceneComponent* SceneComponentTemplate = Cast<USceneComponent>(SCSNode->ComponentTemplate);
		if (SceneComponentTemplate == NULL)
		{
			MobilityToolTip = LOCTEXT("NoMobilityTooltip", "This component does not have 'Mobility' associated with it");
		}
		else if (SceneComponentTemplate->Mobility == EComponentMobility::Movable)
		{
			MobilityToolTip = LOCTEXT("MovableMobilityTooltip", "Movable component");
		}
		else if (SceneComponentTemplate->Mobility == EComponentMobility::Stationary)
		{
			MobilityToolTip = LOCTEXT("StationaryMobilityTooltip", "Stationary component");
		}
		else 
		{
			// make sure we're the mobility type we're expecting (we've handled Movable & Stationary)
			ensureMsgf(SceneComponentTemplate->Mobility == EComponentMobility::Static, TEXT("Unhandled mobility type [%d], is this a new type that we don't handle here?"), SceneComponentTemplate->Mobility.GetValue());

			MobilityToolTip = LOCTEXT("StaticMobilityTooltip", "Static component");
		}
	}

	return MobilityToolTip;
}

FString SSCS_RowWidget::GetAssetName() const
{
	FString AssetName(TEXT("None"));
	if(NodePtr.IsValid() && NodePtr->GetComponentTemplate())
	{
		UObject* Asset = FComponentAssetBrokerage::GetAssetFromComponent(NodePtr->GetComponentTemplate());
		if(Asset != NULL)
		{
			AssetName = Asset->GetName();
		}
	}

	return AssetName;
}

FString SSCS_RowWidget::GetAssetNameTooltip() const
{
	FString AssetName(TEXT("None"));
	if(NodePtr.IsValid() && NodePtr->GetComponentTemplate())
	{
		UObject* Asset = FComponentAssetBrokerage::GetAssetFromComponent(NodePtr->GetComponentTemplate());
		if(Asset != NULL)
		{
			AssetName = Asset->GetPathName();
		}
	}

	return AssetName;
}


EVisibility SSCS_RowWidget::GetAssetVisibility() const
{
	if(NodePtr.IsValid() && NodePtr->GetComponentTemplate() && FComponentAssetBrokerage::SupportsAssets(NodePtr->GetComponentTemplate()))
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Hidden;
	}
}

FSlateColor SSCS_RowWidget::GetColorTint() const
{
	if(NodePtr->IsNative())
	{
		return FLinearColor(0.08f,0.15f,0.6f);
	}
	else if(NodePtr->IsInherited())
	{
		return FLinearColor(0.08f,0.35f,0.6f);
	}

	return FSlateColor::UseForeground();
}

EVisibility SSCS_RowWidget::GetRootLabelVisibility() const
{
	if(NodePtr.IsValid() && SCSEditor.IsValid() && NodePtr == SCSEditor.Pin()->SceneRootNodePtr)
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

TSharedPtr<SWidget> SSCS_RowWidget::BuildSceneRootDropActionMenu(FSCSEditorTreeNodePtrType DroppedNodePtr)
{
	FMenuBuilder MenuBuilder(true, SCSEditor.Pin()->CommandList);

	MenuBuilder.BeginSection("SceneRootNodeDropActions", LOCTEXT("SceneRootNodeDropActionContextMenu", "Drop Actions"));
	{
		const FText DroppedVariableNameText = FText::FromName( DroppedNodePtr->GetVariableName() );
		const FText NodeVariableNameText = FText::FromName( NodePtr->GetVariableName() );

		check(SCSEditor.IsValid());
		check(SCSEditor.Pin()->SCS);
		const bool bDroppedInSameBlueprint = DroppedNodePtr->GetBlueprint() == SCSEditor.Pin()->SCS->GetBlueprint();

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DropActionLabel_AttachToRootNode", "Attach"),
			bDroppedInSameBlueprint 
			? FText::Format( LOCTEXT("DropActionToolTip_AttachToRootNode", "Attach {0} to {1}."), DroppedVariableNameText, NodeVariableNameText )
			: FText::Format( LOCTEXT("DropActionToolTip_AttachToRootNodeFromCopy", "Copy {0} to a new variable and attach it to {1}."), DroppedVariableNameText, NodeVariableNameText ),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SSCS_RowWidget::OnAttachToDropAction, DroppedNodePtr),
				FCanExecuteAction()));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DropActionLabel_MakeNewRootNode", "Make New Root"),
			bDroppedInSameBlueprint
			? FText::Format( LOCTEXT("DropActionToolTip_MakeNewRootNode", "Make {0} the new root."), DroppedVariableNameText )
			: FText::Format( LOCTEXT("DropActionToolTip_MakeNewRootNodeFromCopy", "Copy {0} to a new variable and make it the new root."), DroppedVariableNameText ),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SSCS_RowWidget::OnMakeNewRootDropAction, DroppedNodePtr),
				FCanExecuteAction()));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FReply SSCS_RowWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FReply Reply = SMultiColumnTableRow<FSCSEditorTreeNodePtrType>::OnMouseButtonDown( MyGeometry, MouseEvent );
		return Reply.DetectDrag( SharedThis(this) , EKeys::LeftMouseButton );
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SSCS_RowWidget::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	auto SCSEditorPtr = SCSEditor.Pin();
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton )
			&& SCSEditorPtr.IsValid()
			&& SCSEditorPtr->InEditingMode() ) //can only drag when editing
	{
		TSharedRef<FSCSRowDragDropOp> Operation = FSCSRowDragDropOp::New();
		Operation->CurrentHoverText = FText::GetEmpty();
		Operation->PendingDropAction = FSCSRowDragDropOp::DropAction_None;

		TArray<TSharedPtr<FSCSEditorTreeNode>> SelectedNodePtrs = SCSEditorPtr->GetSelectedNodes();
		if(SelectedNodePtrs.Num() == 0)
		{
			SelectedNodePtrs.Add(NodePtr);
		}
		
		for(const auto& SelectedNodePtr : SelectedNodePtrs)
		{
			Operation->SourceNodes.Add(SelectedNodePtr);
			if(!SelectedNodePtr->CanReparent()
				&& Operation->CurrentHoverText.IsEmpty())
			{
				// We set the tooltip text here because it won't change across entry/leave events
				if(SelectedNodePtrs.Num() == 1)
				{
					Operation->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotReparent", "The selected component cannot be moved.");
				}
				else
				{
					Operation->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotReparentMultiple", "One or more of the selected components cannot be moved.");
				}
			}
		}

		return FReply::Handled().BeginDragDrop(Operation);
	}
	else
	{
		return FReply::Unhandled();
	}
}

void SSCS_RowWidget::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return;
	}

	if ( Operation->IsOfType<FSCSRowDragDropOp>() )
	{
		TSharedPtr<FSCSRowDragDropOp> DragRowOp = StaticCastSharedPtr<FSCSRowDragDropOp>( Operation );
		check(DragRowOp.IsValid());

		// If the hover text is already set, skip everything below, because it means we already know we can't drag-and-drop one or more of the selected nodes.
		if(!DragRowOp->CurrentHoverText.IsEmpty())
		{
			return;
		}

		check(SCSEditor.IsValid());
		FSCSEditorTreeNodePtrType SceneRootNodePtr = SCSEditor.Pin()->SceneRootNodePtr;
		check(SceneRootNodePtr.IsValid());

		// Validate each selected node being dragged against the node that belongs to this row. Exit the loop if we have a valid tooltip OR a valid pending drop action once all nodes in the selection have been validated.
		for(auto SourceNodeIter = DragRowOp->SourceNodes.CreateConstIterator(); SourceNodeIter && (DragRowOp->CurrentHoverText.IsEmpty() || DragRowOp->PendingDropAction != FSCSRowDragDropOp::DropAction_None); ++SourceNodeIter)
		{
			FSCSEditorTreeNodePtrType DraggedNodePtr = *SourceNodeIter;
			check(DraggedNodePtr.IsValid());

			// Reset the pending drop action each time through the loop
			DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_None;

			// Get the component template objects associated with each node
			USceneComponent* HoveredTemplate = Cast<USceneComponent>(NodePtr->GetComponentTemplate());
			USceneComponent* DraggedTemplate = Cast<USceneComponent>(DraggedNodePtr->GetComponentTemplate());

			if(DraggedNodePtr == NodePtr)
			{
				// Attempted to drag and drop onto self
				if(DragRowOp->SourceNodes.Num() > 1)
				{
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_Error_CannotAttachToSelfWithMultipleSelection", "Cannot attach the selected components here because it would result in {0} being attached to itself. Remove it from the selection and try again."), FText::FromName(DraggedNodePtr->GetVariableName()));
				}
				else
				{
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_Error_CannotAttachToSelf", "Cannot attach {0} to itself."), FText::FromName(DraggedNodePtr->GetVariableName()));
				}
			}
			else if(NodePtr->IsAttachedTo(DraggedNodePtr))
			{
				// Attempted to drop a parent onto a child
				if(DragRowOp->SourceNodes.Num() > 1)
				{
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_Error_CannotAttachToChildWithMultipleSelection", "Cannot attach the selected components here because it would result in {0} being attached to one of its children. Remove it from the selection and try again."), FText::FromName(DraggedNodePtr->GetVariableName()));
				}
				else
				{
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_Error_CannotAttachToChild", "Cannot attach {0} to one of its children."), FText::FromName(DraggedNodePtr->GetVariableName()));
				}
			}
			else if(HoveredTemplate == NULL || DraggedTemplate == NULL)
			{
				// Can't attach non-USceneComponent types
				DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_NotAttachable", "Cannot attach to this component.");
			}
			else if(NodePtr == SceneRootNodePtr)
			{
				bool bCanMakeNewRoot = false;
				bool bCanAttachToRoot = !NodePtr->IsDefaultSceneRoot()
					&& !DraggedNodePtr->IsDirectlyAttachedTo(NodePtr)
					&& HoveredTemplate->CanAttachAsChild(DraggedTemplate, NAME_None)
					&& DraggedTemplate->Mobility >= HoveredTemplate->Mobility
					&& (!HoveredTemplate->IsEditorOnly() || DraggedTemplate->IsEditorOnly());

				if(!NodePtr->CanReparent() && (!NodePtr->IsDefaultSceneRoot() || NodePtr->IsInherited()))
				{
					// Cannot make the dropped node the new root if we cannot reparent the current root
					DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotReparentRootNode", "The root component in this Blueprint cannot be replaced.");						
				}
				else if(DraggedTemplate->IsEditorOnly() && !HoveredTemplate->IsEditorOnly()) 
				{
					// can't have a new root that's editor-only (when children would be around in-game)
					DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotReparentEditorOnly", "Cannot re-parent game components under editor-only ones.");
				}
				else if(DraggedTemplate->Mobility > HoveredTemplate->Mobility)
				{
					// can't have a new root that's movable if the existing root is static or stationary
					DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotReparentNonMovable", "Cannot replace a non-movable scene root with a movable component.");
				}
				else if(DragRowOp->SourceNodes.Num() > 1)
				{
					DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotAssignMultipleRootNodes", "Cannot replace the scene root with multiple components. Please select only a single component and try again.");
				}
				else
				{
					bCanMakeNewRoot = true;
				}

				if(bCanMakeNewRoot && bCanAttachToRoot)
				{
					// User can choose to either attach to the current root or make the dropped node the new root
					DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_AttachToOrMakeNewRoot", "Drop here to see available actions.");
					DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_AttachToOrMakeNewRoot;
				}
				else if(DraggedNodePtr->GetBlueprint() != SCSEditor.Pin()->SCS->GetBlueprint())
				{
					if(bCanMakeNewRoot)
					{
						// Only available action is to copy the dragged node to the other Blueprint and make it the new root
						DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_DropMakeNewRootNodeFromCopy", "Drop here to copy {0} to a new variable and make it the new root."), FText::FromName(DraggedNodePtr->GetVariableName()));
						DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_MakeNewRoot;
					}
					else if(bCanAttachToRoot)
					{
						// Only available action is to copy the dragged node(s) to the other Blueprint and attach it to the root
						if(DragRowOp->SourceNodes.Num() > 1)
						{
							DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_AttachToThisNodeFromCopyWithMultipleSelection", "Drop here to copy the selected components to new variables and attach them to {0}."), FText::FromName(NodePtr->GetVariableName()));
						}
						else
						{
							DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_AttachToThisNodeFromCopy", "Drop here to copy {0} to a new variable and attach it to {1}."), FText::FromName(DraggedNodePtr->GetVariableName()), FText::FromName(NodePtr->GetVariableName()));
						}
						
						DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_AttachTo;
					}
				}
				else if(bCanMakeNewRoot)
				{
					// Only available action is to make the dragged node the new root
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_DropMakeNewRootNode", "Drop here to make {0} the new root."), FText::FromName(DraggedNodePtr->GetVariableName()));
					DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_MakeNewRoot;
				}
				else if(bCanAttachToRoot)
				{
					// Only available action is to attach the dragged node(s) to the root
					if(DragRowOp->SourceNodes.Num() > 1)
					{
						DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_AttachToThisNodeWithMultipleSelection", "Drop here to attach the selected components to {0}."), FText::FromName(NodePtr->GetVariableName()));
					}
					else
					{
						DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_AttachToThisNode", "Drop here to attach {0} to {1}."), FText::FromName(DraggedNodePtr->GetVariableName()), FText::FromName(NodePtr->GetVariableName()));
					}

					DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_AttachTo;
				}
			}
			else if(DraggedNodePtr->IsDirectlyAttachedTo(NodePtr)) // if dropped onto parent
			{
				// Detach the dropped node(s) from the current node and reattach to the root node
				if(DragRowOp->SourceNodes.Num() > 1)
				{
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_DetachFromThisNodeWithMultipleSelection", "Drop here to detach the selected components from {0}."), FText::FromName(NodePtr->GetVariableName()));
				}
				else
				{
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_DetachFromThisNode", "Drop here to detach {0} from {1}."), FText::FromName(DraggedNodePtr->GetVariableName()), FText::FromName(NodePtr->GetVariableName()));
				}
				
				DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_DetachFrom;
			}
			else if (!DraggedTemplate->IsEditorOnly() && HoveredTemplate->IsEditorOnly()) 
			{
				// can't have a game component child nested under an editor-only one
				DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotAttachToEditorOnly", "Cannot attach game components to editor-only ones.");
			}
			else if ((DraggedTemplate->Mobility == EComponentMobility::Static) && ((HoveredTemplate->Mobility == EComponentMobility::Movable) || (HoveredTemplate->Mobility == EComponentMobility::Stationary)))
			{
				// Can't attach Static components to mobile ones
				DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotAttachStatic", "Cannot attach Static components to movable ones.");
			}
			else if ((DraggedTemplate->Mobility == EComponentMobility::Stationary) && (HoveredTemplate->Mobility == EComponentMobility::Movable))
			{
				// Can't attach Static components to mobile ones
				DragRowOp->CurrentHoverText = LOCTEXT("DropActionToolTip_Error_CannotAttachStationary", "Cannot attach Stationary components to movable ones.");
			}
			else if(HoveredTemplate->CanAttachAsChild(DraggedTemplate, NAME_None))
			{
				// Attach the dragged node(s) to this node
				if(DraggedNodePtr->GetBlueprint() != SCSEditor.Pin()->SCS->GetBlueprint())
				{
					if(DragRowOp->SourceNodes.Num() > 1)
					{
						DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_AttachToThisNodeFromCopyWithMultipleSelection", "Drop here to copy the selected nodes to new variables and attach to {0}."), FText::FromName(NodePtr->GetVariableName()));
					}
					else
					{
						DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_AttachToThisNodeFromCopy", "Drop here to copy {0} to a new variable and attach it to {1}."), FText::FromName(DraggedNodePtr->GetVariableName()), FText::FromName(NodePtr->GetVariableName()));
					}
				}
				else if(DragRowOp->SourceNodes.Num() > 1)
				{
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_AttachToThisNodeWithMultipleSelection", "Drop here to attach the selected nodes to {0}."), FText::FromName(NodePtr->GetVariableName()));
				}
				else
				{
					DragRowOp->CurrentHoverText = FText::Format(LOCTEXT("DropActionToolTip_AttachToThisNode", "Drop here to attach {0} to {1}."), FText::FromName(DraggedNodePtr->GetVariableName()), FText::FromName(NodePtr->GetVariableName()));
				}

				DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_AttachTo;
			}
			else
			{
				// The dropped node cannot be attached to the current node
				DragRowOp->CurrentHoverText = FText::Format( LOCTEXT("DropActionToolTip_Error_TooManyAttachments", "Unable to attach {0} to {1}."), FText::FromName( DraggedNodePtr->GetVariableName() ), FText::FromName( NodePtr->GetVariableName() ) );
			}
		}
	}
	else if ( Operation->IsOfType<FExternalDragOperation>() || Operation->IsOfType<FAssetDragDropOp>() )
	{
		// defer to the tree widget's handler for this type of operation
		TSharedPtr<SSCSEditor> PinnedEditor = SCSEditor.Pin();
		if ( PinnedEditor.IsValid() && PinnedEditor->SCSTreeWidget.IsValid() )
		{
			PinnedEditor->SCSTreeWidget->OnDragEnter( MyGeometry, DragDropEvent );
		}
	}
}

void SSCS_RowWidget::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FSCSRowDragDropOp> DragRowOp = DragDropEvent.GetOperationAs<FSCSRowDragDropOp>();
	if (DragRowOp.IsValid())
	{
		bool bCanReparentAllNodes = true;
		for(auto SourceNodeIter = DragRowOp->SourceNodes.CreateConstIterator(); SourceNodeIter && bCanReparentAllNodes; ++SourceNodeIter)
		{
			FSCSEditorTreeNodePtrType DraggedNodePtr = *SourceNodeIter;
			check(DraggedNodePtr.IsValid());

			bCanReparentAllNodes = DraggedNodePtr->CanReparent();
		}

		// Only clear the tooltip text if all dragged nodes support it
		if(bCanReparentAllNodes)
		{
			DragRowOp->CurrentHoverText = FText::GetEmpty();
			DragRowOp->PendingDropAction = FSCSRowDragDropOp::DropAction_None;
		}
	}
}

FReply SSCS_RowWidget::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return FReply::Handled();
	}

	if ( Operation->IsOfType<FSCSRowDragDropOp>() && NodePtr->GetComponentTemplate()->IsA<USceneComponent>())	
	{
		TSharedPtr<FSCSRowDragDropOp> DragRowOp = StaticCastSharedPtr<FSCSRowDragDropOp>( Operation );	
		check(DragRowOp.IsValid());

		switch(DragRowOp->PendingDropAction)
		{
		case FSCSRowDragDropOp::DropAction_AttachTo:
			OnAttachToDropAction(DragRowOp->SourceNodes);
			break;
			
		case FSCSRowDragDropOp::DropAction_DetachFrom:
			OnDetachFromDropAction(DragRowOp->SourceNodes);
			break;

		case FSCSRowDragDropOp::DropAction_MakeNewRoot:
			check(DragRowOp->SourceNodes.Num() == 1);
			OnMakeNewRootDropAction(DragRowOp->SourceNodes[0]);
			break;

		case FSCSRowDragDropOp::DropAction_AttachToOrMakeNewRoot:
			check(DragRowOp->SourceNodes.Num() == 1);
			FSlateApplication::Get().PushMenu(
				SharedThis(this),
				BuildSceneRootDropActionMenu(DragRowOp->SourceNodes[0]).ToSharedRef(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
			break;

		case FSCSRowDragDropOp::DropAction_None:
		default:
			break;
		}
	}
	else if (Operation->IsOfType<FExternalDragOperation>() || Operation->IsOfType<FAssetDragDropOp>())
	{
		// defer to the tree widget's handler for this type of operation
		TSharedPtr<SSCSEditor> PinnedEditor = SCSEditor.Pin();
		if ( PinnedEditor.IsValid() && PinnedEditor->SCSTreeWidget.IsValid() )
		{
			PinnedEditor->SCSTreeWidget->OnDrop( MyGeometry, DragDropEvent );
		}
	}

	return FReply::Handled();
}

void SSCS_RowWidget::OnAttachToDropAction(const TArray<FSCSEditorTreeNodePtrType>& DroppedNodePtrs)
{
	check(NodePtr.IsValid());
	check(DroppedNodePtrs.Num() > 0);

	TSharedPtr<SSCSEditor> SCSEditorPtr = SCSEditor.Pin();
	check(SCSEditorPtr.IsValid());

	TSharedPtr<FBlueprintEditor> Kismet2Ptr = SCSEditorPtr->Kismet2Ptr.Pin();
	check(Kismet2Ptr.IsValid());

	AActor* PreviewActor = Kismet2Ptr->GetPreviewActor();
	check(PreviewActor);

	// Get the current Blueprint context
	check(SCSEditorPtr->SCS);
	UBlueprint* Blueprint = SCSEditorPtr->SCS->GetBlueprint();
	check(Blueprint);

	bool bRegenerateTreeNodes = false;
	const FScopedTransaction TransactionContext(DroppedNodePtrs.Num() > 1 ? LOCTEXT("AttachComponents", "Attach Components") : LOCTEXT("AttachComponent", "Attach Component"));

	for(const auto& DroppedNodePtr : DroppedNodePtrs)
	{
		// Clone the component if it's being dropped into a different SCS
		if(DroppedNodePtr->GetBlueprint() != Blueprint)
		{
			bRegenerateTreeNodes = true;

			check(DroppedNodePtr.IsValid());
			UActorComponent* ComponentTemplate = DroppedNodePtr->GetComponentTemplate();
			check(ComponentTemplate);

			// Note: This will mark the Blueprint as structurally modified
			UActorComponent* ClonedComponent = SCSEditorPtr->AddNewComponent(ComponentTemplate->GetClass(), NULL);
			check(ClonedComponent);

			//Serialize object properties using write/read operations.
			TArray<uint8> SavedProperties;
			FObjectWriter Writer(ComponentTemplate, SavedProperties);
			FObjectReader(ClonedComponent, SavedProperties);

			// Attach the copied node to the target node (this will also detach it from the root if necessary)
			FSCSEditorTreeNodePtrType NewNodePtr = SCSEditorPtr->GetNodeFromActorComponent(ClonedComponent);
			if(NewNodePtr.IsValid())
			{
				NodePtr->AddChild(NewNodePtr);
			}
		}
		else
		{
			// Get the associated component template if it is a scene component, so we can adjust the transform
			USceneComponent* SceneComponentTemplate = Cast<USceneComponent>(DroppedNodePtr->GetComponentTemplate());

			// Check for a valid parent node
			FSCSEditorTreeNodePtrType ParentNodePtr = DroppedNodePtr->GetParent();
			if(ParentNodePtr.IsValid())
			{
				// Detach the dropped node from its parent
				ParentNodePtr->RemoveChild(DroppedNodePtr);

				// If the associated component template is a scene component, maintain its preview world position
				if(SceneComponentTemplate)
				{
					// Save current state
					SceneComponentTemplate->Modify();

					// Reset the attach socket name
					SceneComponentTemplate->AttachSocketName = NAME_None;
					USCS_Node* SCS_Node = DroppedNodePtr->GetSCSNode();
					if(SCS_Node)
					{
						SCS_Node->Modify();
						SCS_Node->AttachToName = NAME_None;
					}

					// Attempt to locate a matching instance of the component template in the preview scene
					USceneComponent* PreviewSceneComponent = Cast<USceneComponent>(DroppedNodePtr->FindComponentInstanceInActor(PreviewActor, true));
					if(PreviewSceneComponent)
					{
						// If we find a match, save off the world position
						FTransform ComponentToWorld = PreviewSceneComponent->GetComponentToWorld();
						SceneComponentTemplate->RelativeLocation = ComponentToWorld.GetTranslation();
						SceneComponentTemplate->RelativeRotation = ComponentToWorld.Rotator();
						SceneComponentTemplate->RelativeScale3D = ComponentToWorld.GetScale3D();
					}
				}
			}

			// Attach the dropped node to the given node
			NodePtr->AddChild(DroppedNodePtr);

			// Attempt to locate a matching instance of the parent component template in the preview scene
			USceneComponent* PreviewSceneParentComponent = Cast<USceneComponent>(NodePtr->FindComponentInstanceInActor(PreviewActor, true));
			if(SceneComponentTemplate && PreviewSceneParentComponent)
			{
				// If we find a match, calculate its new position relative to the scene root component instance in the preview scene
				FTransform ComponentToWorld(SceneComponentTemplate->RelativeRotation, SceneComponentTemplate->RelativeLocation, SceneComponentTemplate->RelativeScale3D);
				FTransform ParentToWorld = PreviewSceneParentComponent->GetSocketTransform(SceneComponentTemplate->AttachSocketName);
				FTransform RelativeTM = ComponentToWorld.GetRelativeTransform(ParentToWorld);

				// Store new relative location value (if not set to absolute)
				if(!SceneComponentTemplate->bAbsoluteLocation)
				{
					SceneComponentTemplate->RelativeLocation = RelativeTM.GetTranslation();
				}

				// Store new relative rotation value (if not set to absolute)
				if(!SceneComponentTemplate->bAbsoluteRotation)
				{
					SceneComponentTemplate->RelativeRotation = RelativeTM.Rotator();
				}

				// Store new relative scale value (if not set to absolute)
				if(!SceneComponentTemplate->bAbsoluteScale)
				{
					SceneComponentTemplate->RelativeScale3D = RelativeTM.GetScale3D();
				}
			}
		}
	}

	check(SCSEditorPtr->SCSTreeWidget.IsValid());
	SCSEditorPtr->SCSTreeWidget->SetItemExpansion(NodePtr, true);

	PostDragDropAction(bRegenerateTreeNodes);
}

void SSCS_RowWidget::OnDetachFromDropAction(const TArray<FSCSEditorTreeNodePtrType>& DroppedNodePtrs)
{
	check(NodePtr.IsValid());
	check(DroppedNodePtrs.Num() > 0);

	TSharedPtr<SSCSEditor> SCSEditorPtr = SCSEditor.Pin();
	check(SCSEditorPtr.IsValid());

	TSharedPtr<FBlueprintEditor> Kismet2Ptr = SCSEditorPtr->Kismet2Ptr.Pin();
	check(Kismet2Ptr.IsValid());

	AActor* PreviewActor = Kismet2Ptr->GetPreviewActor();
	check(PreviewActor);

	const FScopedTransaction TransactionContext(DroppedNodePtrs.Num() > 1 ? LOCTEXT("DetachComponents", "Detach Components") : LOCTEXT("DetachComponent", "Detach Component"));

	for(const auto& DroppedNodePtr : DroppedNodePtrs)
	{
		check(DroppedNodePtr.IsValid());

		// Detach the node from its parent
		NodePtr->RemoveChild(DroppedNodePtr);

		// If the associated component template is a scene component, maintain its preview world position
		USceneComponent* SceneComponentTemplate = Cast<USceneComponent>(DroppedNodePtr->GetComponentTemplate());
		if(SceneComponentTemplate)
		{
			// Save current state
			SceneComponentTemplate->Modify();

			// Reset the attach socket name
			SceneComponentTemplate->AttachSocketName = NAME_None;
			USCS_Node* SCS_Node = DroppedNodePtr->GetSCSNode();
			if(SCS_Node)
			{
				SCS_Node->Modify();
				SCS_Node->AttachToName = NAME_None;
			}

			// Attempt to locate a matching instance of the component template in the preview scene
			USceneComponent* PreviewSceneComponent = Cast<USceneComponent>(DroppedNodePtr->FindComponentInstanceInActor(PreviewActor, true));
			if(PreviewSceneComponent)
			{
				// If we find a match, save off the world position
				FTransform ComponentToWorld = PreviewSceneComponent->GetComponentToWorld();
				SceneComponentTemplate->RelativeLocation = ComponentToWorld.GetTranslation();
				SceneComponentTemplate->RelativeRotation = ComponentToWorld.Rotator();
				SceneComponentTemplate->RelativeScale3D = ComponentToWorld.GetScale3D();
			}
		}

		// Attach the dropped node to the current scene root node
		check(SCSEditorPtr->SceneRootNodePtr.IsValid());
		SCSEditorPtr->SceneRootNodePtr->AddChild(DroppedNodePtr);

		// Attempt to locate a matching instance of the scene root component template in the preview scene
		USceneComponent* PreviewSceneRootComponent = Cast<USceneComponent>(SCSEditorPtr->SceneRootNodePtr->FindComponentInstanceInActor(PreviewActor, true));
		if(SceneComponentTemplate && PreviewSceneRootComponent)
		{
			// If we find a match, calculate its new position relative to the scene root component instance in the preview scene
			FTransform ComponentToWorld(SceneComponentTemplate->RelativeRotation, SceneComponentTemplate->RelativeLocation, SceneComponentTemplate->RelativeScale3D);
			FTransform ParentToWorld = PreviewSceneRootComponent->GetSocketTransform(SceneComponentTemplate->AttachSocketName);
			FTransform RelativeTM = ComponentToWorld.GetRelativeTransform(ParentToWorld);

			// Store new relative location value (if not set to absolute)
			if(!SceneComponentTemplate->bAbsoluteLocation)
			{
				SceneComponentTemplate->RelativeLocation = RelativeTM.GetTranslation();
			}

			// Store new relative rotation value (if not set to absolute)
			if(!SceneComponentTemplate->bAbsoluteRotation)
			{
				SceneComponentTemplate->RelativeRotation = RelativeTM.Rotator();
			}

			// Store new relative scale value (if not set to absolute)
			if(!SceneComponentTemplate->bAbsoluteScale)
			{
				SceneComponentTemplate->RelativeScale3D = RelativeTM.GetScale3D();
			}
		}
	}
	
	PostDragDropAction(false);
}

void SSCS_RowWidget::OnMakeNewRootDropAction(FSCSEditorTreeNodePtrType DroppedNodePtr)
{
	TSharedPtr<SSCSEditor> SCSEditorPtr = SCSEditor.Pin();
	check(SCSEditorPtr.IsValid());

	// Get the current SCS context
	USimpleConstructionScript* SCS = SCSEditorPtr->SCS;
	check(SCS != NULL);

	// Get the current Blueprint context
	UBlueprint* Blueprint = SCS->GetBlueprint();
	check(Blueprint != NULL);

	// Get the current scene root node
	FSCSEditorTreeNodePtrType& SceneRootNodePtr = SCSEditorPtr->SceneRootNodePtr;

	check(NodePtr.IsValid() && NodePtr == SceneRootNodePtr);
	check(DroppedNodePtr.IsValid());

	// Create a transaction record
	const FScopedTransaction TransactionContext(LOCTEXT("MakeNewSceneRoot", "Make New Scene Root"));

	// Remember whether or not we're replacing the default scene root
	bool bWasDefaultSceneRoot = SceneRootNodePtr.IsValid() && SceneRootNodePtr->IsDefaultSceneRoot();

	// Clone the component if it's being dropped into a different SCS
	if(DroppedNodePtr->GetBlueprint() != Blueprint)
	{
		UActorComponent* ComponentTemplate = DroppedNodePtr->GetComponentTemplate();
		check(ComponentTemplate);

		// Note: This will mark the Blueprint as structurally modified
		UActorComponent* ClonedComponent = SCSEditorPtr->AddNewComponent(ComponentTemplate->GetClass(), NULL);
		check(ClonedComponent);

		//Serialize object properties using write/read operations.
		TArray<uint8> SavedProperties;
		FObjectWriter Writer(ComponentTemplate, SavedProperties);
		FObjectReader(ClonedComponent, SavedProperties);

		DroppedNodePtr = SCSEditorPtr->GetNodeFromActorComponent(ClonedComponent);
		check(DroppedNodePtr.IsValid());
	}

	if(DroppedNodePtr->GetParent().IsValid()
		&& DroppedNodePtr->GetBlueprint() == Blueprint)
	{
		// Remove the dropped node from its existing parent
		DroppedNodePtr->GetParent()->RemoveChild(DroppedNodePtr);

		// If the associated component template is a scene component, reset its transform since it will now become the root
		USceneComponent* SceneComponentTemplate = Cast<USceneComponent>(DroppedNodePtr->GetComponentTemplate());
		if(SceneComponentTemplate)
		{
			// Save current state
			SceneComponentTemplate->Modify();

			// Reset the attach socket name
			SceneComponentTemplate->AttachSocketName = NAME_None;
			USCS_Node* SCS_Node = DroppedNodePtr->GetSCSNode();
			if(SCS_Node)
			{
				SCS_Node->Modify();
				SCS_Node->AttachToName = NAME_None;
			}

			// Reset the relative transform
			SceneComponentTemplate->SetRelativeLocation(FVector::ZeroVector);
			SceneComponentTemplate->SetRelativeRotation(FRotator::ZeroRotator);
			SceneComponentTemplate->SetRelativeScale3D(FVector(1.f));
		}
	}

	if(!bWasDefaultSceneRoot)
	{
		check(SceneRootNodePtr->CanReparent());

		// Remove the current scene root node from the SCS context
		SCS->RemoveNode(SceneRootNodePtr->GetSCSNode());
	}

	// Save old root node
	FSCSEditorTreeNodePtrType OldSceneRootNodePtr;
	if(!bWasDefaultSceneRoot)
	{
		OldSceneRootNodePtr = SceneRootNodePtr;
	}

	// Set node we are dropping as new root
	SceneRootNodePtr = DroppedNodePtr;

	// Add dropped node to the SCS context
	SCS->AddNode(SceneRootNodePtr->GetSCSNode());

	// Set old root as child of new root
	if(OldSceneRootNodePtr.IsValid())
	{
		SceneRootNodePtr->AddChild(OldSceneRootNodePtr);
	}

	PostDragDropAction(true);
}

void SSCS_RowWidget::PostDragDropAction(bool bRegenerateTreeNodes)
{
	TSharedPtr<SSCSEditor> PinnedEditor = SCSEditor.Pin();
	if(PinnedEditor.IsValid())
	{
		PinnedEditor->UpdateTree(bRegenerateTreeNodes);

		PinnedEditor->RefreshSelectionDetails();

		check(PinnedEditor->SCS != NULL);
		FBlueprintEditorUtils::PostEditChangeBlueprintActors(PinnedEditor->SCS->GetBlueprint());
	}
}

FText SSCS_RowWidget::GetNameLabel() const
{
	// NOTE: Whatever this returns also becomes the variable name
	return FText::FromString(NodePtr->GetDisplayString());
}

FText SSCS_RowWidget::GetTooltipText() const
{
	if(NodePtr->IsDefaultSceneRoot())
	{
		if(NodePtr->IsInherited())
		{
			return LOCTEXT("InheritedDefaultSceneRootToolTip", "This is the default scene root component. It cannot be copied, renamed or deleted. It has been inherited from the parent class, so its properties cannot be edited here. New scene components will automatically be attached to it.");
		}
		else
		{
			return LOCTEXT("DefaultSceneRootToolTip", "This is the default scene root component. It cannot be copied, renamed or deleted. Adding a new scene component will automatically replace it as the new root.");
		}
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), FText::FromString( (NodePtr->GetComponentTemplate() != NULL) ? NodePtr->GetComponentTemplate()->GetClass()->GetName() : TEXT("(null)") ) );
		Args.Add( TEXT("NodeName"), FText::FromString( NodePtr->GetDisplayString() ) );

		if ( NodePtr->IsNative() )
		{
			if(NodePtr->GetComponentTemplate() != NULL)
			{
				return FText::Format( LOCTEXT("NativeClassToolTip", "Native {ClassName}"), Args );
			}

			return FText::Format( LOCTEXT("MissingNativeComponentToolTip", "MISSING!! Native {NodeName}"), Args );
		}
		else
		{
			if ( NodePtr->IsInherited() )
			{
				if ( NodePtr->GetComponentTemplate() != NULL )
				{
					return FText::Format( LOCTEXT("InheritedToolTip", "Inherited {ClassName}"), Args );
				}

				return FText::Format( LOCTEXT("MissingInheritedComponentToolTip", "MISSING!! Inherited {NodeName}"), Args );
			}
			else
			{
				if ( NodePtr->GetComponentTemplate() != NULL )
				{
					return FText::Format( LOCTEXT("RegularToolTip", "{ClassName}"), Args );
				}

				return FText::Format( LOCTEXT("MissingRegularComponentToolTip", "MISSING!! {NodeName}"), Args );
			}
		}
	}
}

FString SSCS_RowWidget::GetDocumentationLink() const
{
	check(SCSEditor.IsValid());

	if( NodePtr == SCSEditor.Pin()->SceneRootNodePtr
		|| NodePtr->IsNative() || NodePtr->IsInherited())
	{
		return TEXT("Shared/Editors/BlueprintEditor/ComponentsMode");
	}

	return TEXT("");
}

FString SSCS_RowWidget::GetDocumentationExcerptName() const
{
	check(SCSEditor.IsValid());

	if( NodePtr == SCSEditor.Pin()->SceneRootNodePtr)
	{
		return TEXT("RootComponent");
	}
	else if(NodePtr->IsNative())
	{
		return TEXT("NativeComponents");
	}
	else if(NodePtr->IsInherited())
	{
		return TEXT("InheritedComponents");
	}

	return TEXT("");
}

UBlueprint* SSCS_RowWidget::GetBlueprint() const
{
	return SCSEditor.Pin()->Blueprint;
}

bool SSCS_RowWidget::OnNameTextVerifyChanged(const FText& InNewText, FText& OutErrorMessage)
{
	USCS_Node* SCS_Node = NodePtr->GetSCSNode();
	if(SCS_Node != NULL && !InNewText.IsEmpty() && !SCS_Node->IsValidVariableNameString(InNewText.ToString()))
	{
		OutErrorMessage = LOCTEXT("RenameFailed_NotValid", "This name is reserved for engine use.");
		return false;
	}

	TSharedPtr<INameValidatorInterface> NameValidator = MakeShareable(new FKismetNameValidator(GetBlueprint(), NodePtr->GetVariableName()));

	EValidatorResult ValidatorResult = NameValidator->IsValid(InNewText.ToString());
	if(ValidatorResult == EValidatorResult::AlreadyInUse)
	{
		OutErrorMessage = FText::Format(LOCTEXT("RenameFailed_InUse", "{0} is in use by another variable or function!"), InNewText);
	}
	else if(ValidatorResult == EValidatorResult::EmptyName)
	{
		OutErrorMessage = LOCTEXT("RenameFailed_LeftBlank", "Names cannot be left blank!");
	}
	else if(ValidatorResult == EValidatorResult::TooLong)
	{
		OutErrorMessage = LOCTEXT("RenameFailed_NameTooLong", "Names must have fewer than 100 characters!");
	}

	if(OutErrorMessage.IsEmpty())
	{
		return true;
	}

	return false;
}

void SSCS_RowWidget::OnNameTextCommit(const FText& InNewName, ETextCommit::Type InTextCommit)
{
	NodePtr->OnCompleteRename(InNewName);
}

//////////////////////////////////////////////////////////////////////////
// SSCSEditor


void SSCSEditor::Construct( const FArguments& InArgs, TSharedPtr<FBlueprintEditor> InKismet2, USimpleConstructionScript* InSCS, UBlueprint* InBlueprint, TSharedPtr<SKismetInspector> Inspector )
{
	Kismet2Ptr = InKismet2;
	KismetInspectorPtr = Inspector.IsValid() ? Inspector : InKismet2->GetInspector();
	SCS = InSCS;
	Blueprint = InBlueprint;

	CommandList = MakeShareable( new FUICommandList );
	CommandList->MapAction( FGenericCommands::Get().Cut,
		FUIAction( FExecuteAction::CreateSP( this, &SSCSEditor::CutSelectedNodes ), 
		FCanExecuteAction::CreateSP( this, &SSCSEditor::CanCutNodes ) ) 
		);
	CommandList->MapAction( FGenericCommands::Get().Copy,
		FUIAction( FExecuteAction::CreateSP( this, &SSCSEditor::CopySelectedNodes ), 
		FCanExecuteAction::CreateSP( this, &SSCSEditor::CanCopyNodes ) ) 
		);
	CommandList->MapAction( FGenericCommands::Get().Paste,
		FUIAction( FExecuteAction::CreateSP( this, &SSCSEditor::PasteNodes ), 
		FCanExecuteAction::CreateSP( this, &SSCSEditor::CanPasteNodes ) ) 
		);
	CommandList->MapAction( FGenericCommands::Get().Duplicate,
		FUIAction( FExecuteAction::CreateSP( this, &SSCSEditor::OnDuplicateComponent ), 
		FCanExecuteAction::CreateSP( this, &SSCSEditor::CanDuplicateComponent ) ) 
		);

	CommandList->MapAction( FGenericCommands::Get().Delete,
		FUIAction( FExecuteAction::CreateSP( this, &SSCSEditor::OnDeleteNodes ), 
		FCanExecuteAction::CreateSP( this, &SSCSEditor::CanDeleteNodes ) ) 
		);

	CommandList->MapAction( FGenericCommands::Get().Rename,
			FUIAction( FExecuteAction::CreateSP( this, &SSCSEditor::OnRenameComponent, true ), // true = transactional (i.e. undoable)
			FCanExecuteAction::CreateSP( this, &SSCSEditor::CanRenameComponent ) ) 
		);

	FSlateBrush const* MobilityHeaderBrush = FEditorStyle::GetBrush(TEXT("ClassIcon.ComponentMobilityHeaderIcon"));

	SAssignNew(SCSTreeWidget, SSCSTreeType)
		.ToolTipText(LOCTEXT("DropAssetToAddComponent", "Drop asset here to add component."))
		.SCSEditor(this)
		.TreeItemsSource(&RootNodes)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SSCSEditor::MakeTableRowWidget)
		.OnGetChildren(this, &SSCSEditor::OnGetChildrenForTree)
		.OnSelectionChanged(this, &SSCSEditor::OnTreeSelectionChanged)
		.OnContextMenuOpening(this, &SSCSEditor::CreateContextMenu)
		.OnItemScrolledIntoView(this, &SSCSEditor::OnItemScrolledIntoView)
		.ItemHeight(24)
		.HeaderRow
		(
		SNew(SHeaderRow)

		+ SHeaderRow::Column(SCS_ColumnName_Mobility)
		.DefaultLabel(LOCTEXT("MobilityColumnLabel", "Mobility"))
		.FixedWidth(16.0f) // mobility icons are 16px (16 slate-units = 16px, when application scale == 1)
		.HeaderContent()
		[
			SNew(SHorizontalBox)
			.ToolTip(SNew(SToolTip).Text(LOCTEXT("MobilityColumnTooltip", "Mobility")))
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SImage).Image(MobilityHeaderBrush)
			]
		]

	+ SHeaderRow::Column(SCS_ColumnName_ComponentClass)
		.DefaultLabel(LOCTEXT("Class", "Class"))
		.FillWidth(4)

		+ SHeaderRow::Column(SCS_ColumnName_Asset)
		.DefaultLabel(LOCTEXT("Asset", "Asset"))
		.FillWidth(3)
		);

	TSharedPtr<SWidget> Contents;
	if( InKismet2.IsValid() )
	{
		Contents = SNew(SVerticalBox)

		// Component picker
		+ SVerticalBox::Slot()
		.Padding(1.f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SComponentClassCombo)
				.OnComponentClassSelected(this, &SSCSEditor::PerformComboAddClass)
			]
		]

		// Tree
		+ SVerticalBox::Slot()
		.Padding(0.f, 0.f, 0.f, 2.f)
		[
			SCSTreeWidget.ToSharedRef()
		];
	}
	else
	{
		Contents = SCSTreeWidget;
	}

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(2.0f)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ComponentsPanel")))
		[
			Contents.ToSharedRef()
		]
	];

	// Refresh the tree widget
	UpdateTree();

	// Expand the scene root node so we show all children by default
	if(SceneRootNodePtr.IsValid())
	{
		SCSTreeWidget->SetItemExpansion(SceneRootNodePtr, true);
	}
}

FReply SSCSEditor::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( CommandList->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<ITableRow> SSCSEditor::MakeTableRowWidget( FSCSEditorTreeNodePtrType InNodePtr, const TSharedRef<STableViewBase>& OwnerTable )
{
	// Native components do not have a SCSNode, so ignore them
	if(InNodePtr->GetSCSNode() && DeferredRenameRequest.Get() == InNodePtr->GetSCSNode())
	{
		SCSTreeWidget->SetSelection(InNodePtr);
		OnRenameComponent(false);
	}

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("TableRow"));
	if (InNodePtr.IsValid() && InNodePtr->GetComponentTemplate() != NULL )
	{
		TagMeta.FriendlyName = FString::Printf(TEXT("TableRow,%s,0"), *InNodePtr->GetComponentTemplate()->GetReadableName());
	}
	return SNew(SSCS_RowWidget, SharedThis(this), InNodePtr, OwnerTable)
		.AddMetaData<FTutorialMetaData>(TagMeta);
}

void SSCSEditor::GetSelectedItemsForContextMenu(TArray<FComponentEventConstructionData>& OutSelectedItems) const
{
	TArray<FSCSEditorTreeNodePtrType> SelectedTreeItems = SCSTreeWidget->GetSelectedItems();
	for ( auto NodeIter = SelectedTreeItems.CreateConstIterator(); NodeIter; ++NodeIter )
	{
		FComponentEventConstructionData NewItem;
		auto TreeNode = *NodeIter;
		NewItem.VariableName = TreeNode->GetVariableName();
		NewItem.Component = TreeNode->GetComponentTemplate();
		OutSelectedItems.Add(NewItem);
	}
}

TSharedPtr< SWidget > SSCSEditor::CreateContextMenu()
{
	TArray<FSCSEditorTreeNodePtrType> SelectedNodes = SCSTreeWidget->GetSelectedItems();

	if (SelectedNodes.Num() > 0 || CanPasteNodes())
	{
		const bool CloseAfterSelection = true;
		FMenuBuilder MenuBuilder( CloseAfterSelection, CommandList );

		MenuBuilder.BeginSection("ComponentActions", LOCTEXT("ComponentContextMenu", "Component Actions") );
		{
			if(SelectedNodes.Num() > 0)
			{
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Cut) ;
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Copy );
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Paste );
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Duplicate );
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Delete );
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Rename );

				// Collect the classes of all selected objects
				TArray<UClass*> SelectionClasses;
				for( auto NodeIter = SelectedNodes.CreateConstIterator(); NodeIter; ++NodeIter )
				{
					auto TreeNode = *NodeIter;
					if( TreeNode->GetComponentTemplate() )
					{
						SelectionClasses.Add( TreeNode->GetComponentTemplate()->GetClass() );
					}
				}

				TSharedPtr<FBlueprintEditor> PinnedBlueprintEditor = Kismet2Ptr.Pin();
				if ( PinnedBlueprintEditor.IsValid() && SelectionClasses.Num() )
				{
					// Find the common base class of all selected classes
					UClass* SelectedClass = UClass::FindCommonBase( SelectionClasses );
					// Build an event submenu if we can generate events
					if( FBlueprintEditor::CanClassGenerateEvents( SelectedClass ))
					{
						MenuBuilder.AddSubMenu(	LOCTEXT("AddEventSubMenu", "Add Event"), 
							LOCTEXT("ActtionsSubMenu_ToolTip", "Add Event"), 
							FNewMenuDelegate::CreateStatic( &SSCSEditor::BuildMenuEventsSection, PinnedBlueprintEditor, SelectedClass, 
							FGetSelectedObjectsDelegate::CreateSP(this, &SSCSEditor::GetSelectedItemsForContextMenu)));
					}
				}
			}
			else
			{
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Paste );
			}
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}
	return TSharedPtr<SWidget>();
}

void SSCSEditor::BuildMenuEventsSection(FMenuBuilder& Menu, TSharedPtr<FBlueprintEditor> BlueprintEditor, UClass* SelectedClass, FGetSelectedObjectsDelegate GetSelectedObjectsDelegate)
{
	// Get Selected Nodes
	TArray<FComponentEventConstructionData> SelectedNodes;
	GetSelectedObjectsDelegate.ExecuteIfBound( SelectedNodes );
	UBlueprint* Blueprint = BlueprintEditor->GetBlueprintObj();

	struct FMenuEntry
	{
		FText		Label;
		FText		ToolTip;
		FUIAction	UIAction;
	};

	TArray< FMenuEntry > Actions;
	TArray< FMenuEntry > NodeActions;
	// Build Events entries
	for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(SelectedClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* Property = *PropertyIt;

		// Check for multicast delegates that we can safely assign
		if (!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintAssignable))
		{
			FName EventName = Property->GetFName();
			int32 ComponentEventViewEntries = 0;
			// Add View Event Per Component
			for (auto NodeIter = SelectedNodes.CreateConstIterator(); NodeIter; ++NodeIter )
			{
				if( NodeIter->Component.IsValid() )
				{
					FName VariableName = NodeIter->VariableName;
					UObjectProperty* VariableProperty = FindField<UObjectProperty>( Blueprint->SkeletonGeneratedClass, VariableName );

					if( VariableProperty && FKismetEditorUtilities::FindBoundEventForComponent( Blueprint, EventName, VariableProperty->GetFName() ))
					{
						FMenuEntry NewEntry;
						NewEntry.Label = ( SelectedNodes.Num() > 1 ) ?	FText::Format( LOCTEXT("ViewEvent_ToolTipFor", "{0} for {1}"), FText::FromName( EventName ), FText::FromName( VariableName )) : 
																		FText::Format( LOCTEXT("ViewEvent_ToolTip", "{0}"), FText::FromName( EventName ));
						NewEntry.UIAction =	FUIAction(FExecuteAction::CreateStatic( &SSCSEditor::ViewEvent, Blueprint, EventName, *NodeIter ),
													  FCanExecuteAction::CreateSP( BlueprintEditor.Get(), &FBlueprintEditor::InEditingMode ));
						NodeActions.Add( NewEntry );
						ComponentEventViewEntries++;
					}
				}
			}
			if( ComponentEventViewEntries < SelectedNodes.Num() )
			{
			// Create menu Add entry
				FMenuEntry NewEntry;
				NewEntry.Label = FText::Format( LOCTEXT("AddEvent_ToolTip", "Add {0}" ), FText::FromName( EventName ));
				NewEntry.UIAction =	FUIAction(FExecuteAction::CreateStatic( &SSCSEditor::CreateEventsForSelection, BlueprintEditor, EventName, GetSelectedObjectsDelegate),
											  FCanExecuteAction::CreateSP(BlueprintEditor.Get(), &FBlueprintEditor::InEditingMode));
				Actions.Add( NewEntry );
		}
	}
}
	// Build Menu Sections
	Menu.BeginSection("AddComponentActions", LOCTEXT("AddEventHeader", "Add Event"));
	for (auto ItemIter = Actions.CreateConstIterator(); ItemIter; ++ItemIter )
	{
		Menu.AddMenuEntry( ItemIter->Label, ItemIter->ToolTip, FSlateIcon(), ItemIter->UIAction );
	}
	Menu.EndSection();
	Menu.BeginSection("ViewComponentActions", LOCTEXT("ViewEventHeader", "View Existing Events"));
	for (auto ItemIter = NodeActions.CreateConstIterator(); ItemIter; ++ItemIter )
	{
		Menu.AddMenuEntry( ItemIter->Label, ItemIter->ToolTip, FSlateIcon(), ItemIter->UIAction );
	}
	Menu.EndSection();
}

void SSCSEditor::CreateEventsForSelection(TSharedPtr<FBlueprintEditor> BlueprintEditor, FName EventName, FGetSelectedObjectsDelegate GetSelectedObjectsDelegate)
{	
	if (EventName != NAME_None)
	{
		UBlueprint* Blueprint = BlueprintEditor->GetBlueprintObj();
		TArray<FComponentEventConstructionData> SelectedNodes;
		GetSelectedObjectsDelegate.ExecuteIfBound(SelectedNodes);

		for (auto SelectionIter = SelectedNodes.CreateConstIterator(); SelectionIter; ++SelectionIter)
		{
			ConstructEvent( Blueprint, EventName, *SelectionIter );
		}
	}
}

void SSCSEditor::ConstructEvent(UBlueprint* Blueprint, const FName EventName, const FComponentEventConstructionData EventData)
{
	// Find the corresponding variable property in the Blueprint
	UObjectProperty* VariableProperty = FindField<UObjectProperty>(Blueprint->SkeletonGeneratedClass, EventData.VariableName );

	if( VariableProperty )
	{
		if (!FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, EventName, VariableProperty->GetFName()))
		{
			FKismetEditorUtilities::CreateNewBoundEventForComponent(EventData.Component.Get(), EventName, Blueprint, VariableProperty);
		}
	}
}

void SSCSEditor::ViewEvent(UBlueprint* Blueprint, const FName EventName, const FComponentEventConstructionData EventData)
{
	// Find the corresponding variable property in the Blueprint
	UObjectProperty* VariableProperty = FindField<UObjectProperty>(Blueprint->SkeletonGeneratedClass, EventData.VariableName );

	if( VariableProperty )
	{
		const UK2Node_ComponentBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForComponent(Blueprint, EventName, VariableProperty->GetFName());
		if (ExistingNode)
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ExistingNode);
		}
	}
}

bool SSCSEditor::CanDuplicateComponent() const
{
	if(!InEditingMode())
	{
		return false;
	}

	return CanCopyNodes();
}

void SSCSEditor::OnDuplicateComponent()
{
	TArray<FSCSEditorTreeNodePtrType> SelectedNodes = SCSTreeWidget->GetSelectedItems();
	if(SelectedNodes.Num() > 0)
	{
		const FScopedTransaction Transaction(SelectedNodes.Num() > 1 ? LOCTEXT("DuplicateComponents", "Duplicate Components") : LOCTEXT("DuplicateComponent", "Duplicate Component"));

		for (int32 i = 0; i < SelectedNodes.Num(); ++i)
		{
			UActorComponent* ComponentTemplate = SelectedNodes[i]->GetComponentTemplate();
			if(ComponentTemplate != NULL)
			{
				UActorComponent* CloneComponent = AddNewComponent(ComponentTemplate->GetClass(), NULL);
				UActorComponent* OriginalComponent = ComponentTemplate;

				//Serialize object properties using write/read operations.
				TArray<uint8> SavedProperties;
				FObjectWriter Writer(OriginalComponent, SavedProperties);
				FObjectReader(CloneComponent, SavedProperties);

				// If we've duplicated a scene component, attempt to reposition the duplicate in the hierarchy if the original
				// was attached to another scene component as a child. By default, the duplicate is attached to the scene root node.
				USceneComponent* NewSceneComponent = Cast<USceneComponent>(CloneComponent);
				if(NewSceneComponent != NULL)
				{
					// Ensure that any native attachment relationship inherited from the original copy is removed (to prevent a GLEO assertion)
					NewSceneComponent->DetachFromParent(true);

					// Attempt to locate the original node in the SCS tree
					FSCSEditorTreeNodePtrType OriginalNodePtr = FindTreeNode(OriginalComponent);
					if(OriginalNodePtr.IsValid())
					{
						// If the original node was parented, attempt to add the duplicate as a child of the same parent node
						FSCSEditorTreeNodePtrType ParentNodePtr = OriginalNodePtr->GetParent();
						if(ParentNodePtr.IsValid() && ParentNodePtr != SceneRootNodePtr)
						{
							// Locate the duplicate node (as a child of the current scene root node), and switch it to be a child of the original node's parent
							FSCSEditorTreeNodePtrType NewChildNodePtr = SceneRootNodePtr->FindChild(NewSceneComponent);
							if(NewChildNodePtr.IsValid())
							{
								// Note: This method will handle removal from the scene root node as well
								ParentNodePtr->AddChild(NewChildNodePtr);
							}
						}
					}
				}
			}
		}
	}
}

void SSCSEditor::OnGetChildrenForTree( FSCSEditorTreeNodePtrType InNodePtr, TArray<FSCSEditorTreeNodePtrType>& OutChildren )
{
	OutChildren.Empty();

	if(InNodePtr.IsValid())
	{
		OutChildren = InNodePtr->GetChildren();
	}
}


void SSCSEditor::PerformComboAddClass(TSubclassOf<UActorComponent> ComponentClass)
	{
	UClass* NewClass = ComponentClass;

	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	USelection* Selection =  GEditor->GetSelectedObjects();

	bool bAddedComponent = false;

	// This adds components according to the type selected in the drop down. If the user
	// has the appropriate objects selected in the content browser then those are added,
	// else we go down the previous route of adding components by type.
	if ( Selection->Num() > 0 )
	{
		for(FSelectionIterator ObjectIter(*Selection);ObjectIter;++ObjectIter)
		{
			UObject* Object = *ObjectIter;
			UClass*  Class	= Object->GetClass();

			TArray< TSubclassOf<UActorComponent> > ComponentClasses = FComponentAssetBrokerage::GetComponentsForAsset(Object);

			// if the selected asset supports the selected component type then go ahead and add it
			for ( int32 ComponentIndex = 0; ComponentIndex < ComponentClasses.Num(); ComponentIndex++ )
			{
				if ( ComponentClasses[ComponentIndex]->IsChildOf( NewClass ) )
				{
					AddNewComponent( NewClass, Object );
					bAddedComponent = true;
					break;
				}
			}
		}
	}

	if ( !bAddedComponent )
	{
		// As the SCS splits up the scene and actor components, can now add directly
		AddNewComponent(ComponentClass, NULL);
	}
}

TArray<FSCSEditorTreeNodePtrType>  SSCSEditor::GetSelectedNodes() const
{
	TArray<FSCSEditorTreeNodePtrType> SelectedTreeNodes = SCSTreeWidget->GetSelectedItems();

	struct FCompareSelectedSCSEditorTreeNodes
	{
		FORCEINLINE bool operator()(const FSCSEditorTreeNodePtrType& A, const FSCSEditorTreeNodePtrType& B) const
		{
			return B.IsValid() && B->IsAttachedTo(A);
		}
	};

	// Ensure that nodes are ordered from parent to child (otherwise they are sorted in the order that they were selected)
	SelectedTreeNodes.Sort(FCompareSelectedSCSEditorTreeNodes());

	return SelectedTreeNodes;
}

FSCSEditorTreeNodePtrType SSCSEditor::GetNodeFromActorComponent(const UActorComponent* ActorComponent, bool bIncludeAttachedComponents) const
{
	FSCSEditorTreeNodePtrType NodePtr;

	if(ActorComponent)
	{
		// If the given component instance is not already an archetype object
		if(!ActorComponent->IsTemplate())
		{
			// Get the component owner's class object
			check(ActorComponent->GetOwner() != NULL);
			UClass* OwnerClass = ActorComponent->GetOwner()->GetActorClass();

			// If the given component is one that's created during Blueprint construction
			if(ActorComponent->bCreatedByConstructionScript)
			{
				// Get the Blueprint object associated with the owner's class
				UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(OwnerClass);
				if(Blueprint && Blueprint->SimpleConstructionScript)
				{
					// Attempt to locate an SCS node with a variable name that matches the name of the given component
					TArray<USCS_Node*> AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
					for(int32 i = 0; i < AllNodes.Num(); ++i)
					{
						USCS_Node* SCS_Node = AllNodes[i];
						
						check(SCS_Node != NULL);
						if(SCS_Node->VariableName == ActorComponent->GetFName())
						{
							// We found a match; redirect to the component archetype instance that may be associated with a tree node
							ActorComponent = SCS_Node->ComponentTemplate;
							break;
						}
					}
				}
			}
			else
			{
				// Get the class default object
				const AActor* CDO = Cast<AActor>(OwnerClass->GetDefaultObject());
				if(CDO)
				{
					// Iterate over the Components array and attempt to find a component with a matching name
					TArray<UActorComponent*> Components;
					CDO->GetComponents(Components);

					for(auto It = Components.CreateConstIterator(); It; ++It)
					{
						UActorComponent* ComponentTemplate = *It;
						if(ComponentTemplate->GetFName() == ActorComponent->GetFName())
						{
							// We found a match; redirect to the component archetype instance that may be associated with a tree node
							ActorComponent = ComponentTemplate;
							break;
						}
					}
				}
			}
		}

		// If we have a valid component archetype instance, attempt to find a tree node that corresponds to it
		if(ActorComponent->IsTemplate())
		{
			for(int32 i = 0; i < RootNodes.Num() && !NodePtr.IsValid(); i++)
			{
				NodePtr = FindTreeNode(ActorComponent, RootNodes[i]);
			}
		}

		// If we didn't find it in the tree, step up the chain to the parent of the given component and recursively see if that is in the tree (unless the flag is false)
		if(!NodePtr.IsValid() && bIncludeAttachedComponents)
		{
			const USceneComponent* SceneComponent = Cast<const USceneComponent>(ActorComponent);
			if(SceneComponent && SceneComponent->AttachParent)
			{
				return GetNodeFromActorComponent(SceneComponent->AttachParent, bIncludeAttachedComponents);
			}
		}
	}

	return NodePtr;
}

void SSCSEditor::SelectNode(FSCSEditorTreeNodePtrType InNodeToSelect, bool IsCntrlDown) 
{
	if(SCSTreeWidget.IsValid() && InNodeToSelect.IsValid())
	{
		if(!IsCntrlDown)
		{
			SCSTreeWidget->SetSelection(InNodeToSelect);
		}
		else
		{
			SCSTreeWidget->SetItemSelection(InNodeToSelect, !SCSTreeWidget->IsItemSelected(InNodeToSelect));
		}
	}
}

static FSCSEditorTreeNode* FindRecursive( FSCSEditorTreeNode* Node, FName Name )
{
	if (Node->GetVariableName() == Name)
	{
		return Node;
	}
	else
	{
		for (const auto& Child : Node->GetChildren())
		{
			if (auto Result = FindRecursive(Child.Get(), Name))
			{
				return Result;
			}
		}
	}

	return nullptr;
}

void SSCSEditor::HighlightTreeNode(FName TreeNodeName, const class FPropertyPath& Property)
{
	for( const auto& Node : RootNodes )
	{
		if( auto FoundNode = FindRecursive( Node.Get(), TreeNodeName ) )
		{
			SelectNode(FoundNode->AsShared(), false);

			if (Property != FPropertyPath())
			{
				auto KismetInspectorSPtr = KismetInspectorPtr.Pin();
				check(KismetInspectorSPtr.IsValid());
				KismetInspectorSPtr->GetPropertyView()->HighlightProperty(Property);
			}

			return;
		}
	}
	
	ClearSelection();
}

void SSCSEditor::HighlightTreeNode(const USCS_Node* Node, FName Property)
{
	check(Node);
	auto TreeNode = FindTreeNode( Node );
	check( TreeNode.IsValid() );
	SelectNode( TreeNode, false );
	if( Property != FName() )
	{
		auto KismetInspectorSPtr = KismetInspectorPtr.Pin();
		check( KismetInspectorSPtr.IsValid() );
		UActorComponent* Component = TreeNode->GetComponentTemplate();
		UProperty* CurrentProp = FindField<UProperty>(Component->GetClass(), Property);
		FPropertyPath Path;
		if( CurrentProp )
		{
			FPropertyInfo NewInfo = { CurrentProp, -1 };
			Path.ExtendPath(NewInfo);
		}
		KismetInspectorSPtr->GetPropertyView()->HighlightProperty( Path );
	}
}

void SSCSEditor::UpdateTree(bool bRegenerateTreeNodes)
{
	check(SCSTreeWidget.IsValid());

	if(bRegenerateTreeNodes)
	{
		// Obtain the set of expandable tree nodes that are currently collapsed
		TSet<FSCSEditorTreeNodePtrType> CollapsedTreeNodes;
		GetCollapsedNodes(SceneRootNodePtr, CollapsedTreeNodes);

		// Obtain the list of selected items
		TArray<FSCSEditorTreeNodePtrType> SelectedTreeNodes = SCSTreeWidget->GetSelectedItems();

		// Clear the current tree
		RootNodes.Empty();

		// Reset the scene root node
		SceneRootNodePtr.Reset();

		// Get the Blueprint class default object as well as the inheritance stack
		AActor* CDO = NULL;
		TArray<UBlueprint*> ParentBPStack;
		if(Blueprint->GeneratedClass != NULL)
		{
			CDO = Blueprint->GeneratedClass->GetDefaultObject<AActor>();
			UBlueprint::GetBlueprintHierarchyFromClass(Blueprint->GeneratedClass, ParentBPStack);
		}

		if(CDO != NULL)
		{
			// Add native ActorComponent nodes to the root set first
			TArray<UActorComponent*> Components;
			CDO->GetComponents(Components);

			for(auto CompIter = Components.CreateIterator(); CompIter; ++CompIter)
			{
				UActorComponent* ActorComp = *CompIter;
				if (!ActorComp->IsA<USceneComponent>())
				{
					RootNodes.Add(MakeShareable(new FSCSEditorTreeNode(ActorComp)));
				}
			}

		// Add the native base class SceneComponent hierarchy
			for(auto CompIter = Components.CreateIterator(); CompIter; ++CompIter)
			{
				USceneComponent* SceneComp = Cast<USceneComponent>(*CompIter);
				if(SceneComp != NULL)
				{
					AddTreeNode(SceneComp);
				}
			}
		}

		// Add the full SCS tree node hierarchy (including SCS nodes inherited from parent blueprints)
		for(int32 StackIndex = ParentBPStack.Num() - 1; StackIndex >= 0; --StackIndex)
		{
			if(ParentBPStack[StackIndex]->SimpleConstructionScript != NULL)
			{
				const TArray<USCS_Node*>& SCS_RootNodes = ParentBPStack[StackIndex]->SimpleConstructionScript->GetRootNodes();
				for(int32 NodeIndex = 0; NodeIndex < SCS_RootNodes.Num(); ++NodeIndex)
				{
					USCS_Node* SCS_Node = SCS_RootNodes[NodeIndex];
					check(SCS_Node != NULL);

					if(SCS_Node->ParentComponentOrVariableName != NAME_None)
					{
						USceneComponent* ParentComponent = SCS_Node->GetParentComponentTemplate(Blueprint);
						if(ParentComponent != NULL)
						{
							FSCSEditorTreeNodePtrType ParentNodePtr = FindTreeNode(ParentComponent);
							if(ParentNodePtr.IsValid())
							{
								AddTreeNode(SCS_Node, ParentNodePtr);
							}
						}
					}
					else
					{
						AddTreeNode(SCS_Node, SceneRootNodePtr);
					}
				}
			}
		}

		// Restore the previous expansion state on the new tree nodes
		TArray<FSCSEditorTreeNodePtrType> CollapsedTreeNodeArray = CollapsedTreeNodes.Array();
		for(int i = 0; i < CollapsedTreeNodeArray.Num(); ++i)
		{
			// Look for a component match in the new hierarchy; if found, mark it as collapsed to match the previous setting
			FSCSEditorTreeNodePtrType NodeToExpandPtr = FindTreeNode(CollapsedTreeNodeArray[i]->GetComponentTemplate());
			if(NodeToExpandPtr.IsValid())
			{
				SCSTreeWidget->SetItemExpansion(NodeToExpandPtr, false);
			}
		}

		// Restore the previous selection state on the new tree nodes
		for(int i = 0; i < SelectedTreeNodes.Num(); ++i)
		{
			FSCSEditorTreeNodePtrType NodeToSelectPtr = FindTreeNode(SelectedTreeNodes[i]->GetComponentTemplate());
			if(NodeToSelectPtr.IsValid())
			{
				SCSTreeWidget->SetItemSelection(NodeToSelectPtr, true);
			}
		}

		// If we have a pending deferred rename request, redirect it to the new tree node
		if(DeferredRenameRequest.IsValid())
		{
			FSCSEditorTreeNodePtrType NodeToRenamePtr = FindTreeNode(DeferredRenameRequest.Get());
			if(NodeToRenamePtr.IsValid())
			{
				SCSTreeWidget->RequestScrollIntoView(NodeToRenamePtr);
			}
		}
	}

	// refresh widget
	SCSTreeWidget->RequestTreeRefresh();
}

void SSCSEditor::ClearSelection()
{
	check(SCSTreeWidget.IsValid());
	SCSTreeWidget->ClearSelection();
	
	if( KismetInspectorPtr.IsValid() )
	{
		KismetInspectorPtr.Pin()->ShowDetailsForObjects(TArray<UObject*>());
	}
}



void SSCSEditor::SaveSCSCurrentState( USimpleConstructionScript* SCSObj )
{
	if( SCSObj )
	{
		SCSObj->Modify();

		const TArray<USCS_Node*>& SCS_RootNodes = SCSObj->GetRootNodes();
		for(int32 i = 0; i < SCS_RootNodes.Num(); ++i)
		{
			SaveSCSNode( SCS_RootNodes[i] );
		}
	}
}

void SSCSEditor::SaveSCSNode( USCS_Node* Node )
{
	if( Node )
	{
		Node->Modify();

		for( int32 i=0; i<Node->ChildNodes.Num(); i++ )
		{
			SaveSCSNode( Node->ChildNodes[i] );
		}
	}
}

bool SSCSEditor::InEditingMode() const
{
	return Kismet2Ptr.IsValid() && NULL == GEditor->PlayWorld;
}

UActorComponent* SSCSEditor::AddNewComponent( UClass* NewComponentClass, UObject* Asset  )
{
	const FScopedTransaction Transaction( LOCTEXT("AddComponent", "Add Component") );

	// Create a new template and add it to the blueprint
	Blueprint->Modify();

	SaveSCSCurrentState( Blueprint->SimpleConstructionScript );

	USCS_Node* NewNode = SCS->CreateNode(NewComponentClass);
	return AddNewNode(NewNode, Asset, true);
}

UActorComponent* SSCSEditor::AddNewNode(USCS_Node* NewNode,  UObject* Asset, bool bMarkBlueprintModified, bool bSetFocusToNewItem)
{
	if(Asset)
	{
		FComponentAssetBrokerage::AssignAssetToComponent(NewNode->ComponentTemplate, Asset);
	}

	FSCSEditorTreeNodePtrType NewNodePtr;

	// Reset the scene root node if it's set to the default one that's managed by the SCS
	if(SceneRootNodePtr.IsValid() && SceneRootNodePtr->GetSCSNode() == SCS->GetDefaultSceneRootNode())
	{
		SceneRootNodePtr.Reset();
	}

	// Add the new node to the editor tree
	NewNodePtr = AddTreeNode(NewNode, SceneRootNodePtr);

	// Potentially adjust variable names for any child blueprints
	if(NewNode->VariableName != NAME_None)
	{
		FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, NewNode->VariableName);
	}
	
	if(bSetFocusToNewItem)
	{
		// Select and request a rename on the new component
		SCSTreeWidget->SetSelection(NewNodePtr);
		OnRenameComponent(false);
	}

	// Will call UpdateTree as part of OnBlueprintChanged handling
	if(bMarkBlueprintModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	else
	{
		UpdateTree();
	}

	return NewNode->ComponentTemplate;
}

bool SSCSEditor::IsComponentSelected(const UPrimitiveComponent* PrimComponent) const
{
	check(PrimComponent != NULL);

	FSCSEditorTreeNodePtrType NodePtr = GetNodeFromActorComponent(PrimComponent);
	if(NodePtr.IsValid() && SCSTreeWidget.IsValid())
	{
		return SCSTreeWidget->IsItemSelected(NodePtr);
	}

	return false;
}

void SSCSEditor::SetSelectionOverride(UPrimitiveComponent* PrimComponent) const
{
	PrimComponent->SelectionOverrideDelegate = UPrimitiveComponent::FSelectionOverride::CreateSP(this, &SSCSEditor::IsComponentSelected);
	PrimComponent->PushSelectionToProxy();
}

bool SSCSEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void SSCSEditor::CutSelectedNodes()
{
	TArray<FSCSEditorTreeNodePtrType> SelectedNodes = GetSelectedNodes();
	const FScopedTransaction Transaction( SelectedNodes.Num() > 1 ? LOCTEXT("CutComponents", "Cut Components") : LOCTEXT("CutComponent", "Cut Component") );

	CopySelectedNodes();
	OnDeleteNodes();
}

bool SSCSEditor::CanCopyNodes() const
{
	TArray<FSCSEditorTreeNodePtrType> SelectedNodes = SCSTreeWidget->GetSelectedItems();
	bool bCanCopy = SelectedNodes.Num() > 0;
	if(bCanCopy)
	{
		for (int32 i = 0; i < SelectedNodes.Num() && bCanCopy; ++i)
		{
			// Check for the default scene root; that cannot be copied/duplicated
			UActorComponent* ComponentTemplate = SelectedNodes[i]->GetComponentTemplate();
			bCanCopy = ComponentTemplate != nullptr && !SelectedNodes[i]->IsDefaultSceneRoot();
			if (bCanCopy)
			{
				UClass* ComponentTemplateClass = ComponentTemplate->GetClass();
				check(ComponentTemplateClass != nullptr);

				// Component class cannot be abstract and must also be tagged as BlueprintSpawnable
				bCanCopy = !ComponentTemplateClass->HasAnyClassFlags(CLASS_Abstract)
					&& ComponentTemplateClass->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent);
			}
		}
	}

	return bCanCopy;
}

void SSCSEditor::CopySelectedNodes()
{
	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;
	TArray<FSCSEditorTreeNodePtrType> SelectedNodes = GetSelectedNodes();

	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp|OBJECTMARK_TagImp));

	// Duplicate the selected component templates into temporary objects that we can modify
	TMap<FName, FName> ParentMap;
	TMap<FName, UActorComponent*> ObjectMap;
	for (int32 i = 0; i < SelectedNodes.Num(); ++i)
	{
		// Get the current selected node reference
		FSCSEditorTreeNodePtrType SelectedNodePtr = SelectedNodes[i];
		check(SelectedNodePtr.IsValid());

		// Get the component template associated with the selected node
		UObject* ObjectToCopy = SelectedNodePtr->GetComponentTemplate();
		if(ObjectToCopy)
		{
			// If valid, duplicate the component template into a temporary object
			ObjectToCopy = StaticDuplicateObject(ObjectToCopy, GetTransientPackage(), *SelectedNodePtr->GetVariableName().ToString(), RF_AllFlags & ~RF_ArchetypeObject);
			if(ObjectToCopy)
			{
				// Get the closest parent node of the current node selection within the selected set
				FSCSEditorTreeNodePtrType ParentNodePtr = SelectedNodePtr->FindClosestParent(SelectedNodes);
				if(ParentNodePtr.IsValid())
				{
					// If valid, record the parent node's variable name into the node->parent map
					ParentMap.Add(SelectedNodePtr->GetVariableName(), ParentNodePtr->GetVariableName());
				}

				// Record the temporary object into the name->object map
				ObjectMap.Add(SelectedNodePtr->GetVariableName(), CastChecked<UActorComponent>(ObjectToCopy));
			}
		}
	}

	// Export the component object(s) to text for copying
	for(auto ObjectIt = ObjectMap.CreateIterator(); ObjectIt; ++ObjectIt)
	{
		// Get the component object to be copied
		UActorComponent* ComponentToCopy = ObjectIt->Value;
		check(ComponentToCopy);

		// If this component object had a parent within the selected set
		if(ParentMap.Contains(ComponentToCopy->GetFName()))
		{
			// Get the name of the parent component
			FName ParentName = ParentMap[ComponentToCopy->GetFName()];
			if(ObjectMap.Contains(ParentName))
			{
				// Ensure that this component is a scene component
				USceneComponent* SceneComponent = Cast<USceneComponent>(ComponentToCopy);
				if(SceneComponent)
				{
					// Set the attach parent to the matching parent object in the temporary set. This allows us to preserve hierarchy in the copied set.
					SceneComponent->AttachParent = Cast<USceneComponent>(ObjectMap[ParentName]);
				}
			}
		}

		// Export the component object to the given string
		UExporter::ExportToOutputDevice(&Context, ComponentToCopy, NULL, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified|PPF_Copy|PPF_Delimited, false, ComponentToCopy->GetOuter());
	}

	// Copy text to clipboard
	FString ExportedText = Archive;
	FPlatformMisc::ClipboardCopy(*ExportedText);
}

bool SSCSEditor::CanPasteNodes() const
{
	if(!InEditingMode())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformMisc::ClipboardPaste(ClipboardContent);

	// Obtain the component object text factory for the clipboard content and return whether or not we can use it
	TSharedRef<FSCSEditorComponentObjectTextFactory> Factory = FSCSEditorComponentObjectTextFactory::Get(ClipboardContent);
	return Factory->NewObjectMap.Num() > 0
		&& (SceneRootNodePtr->IsDefaultSceneRoot() || Factory->CanAttachComponentsTo(Cast<USceneComponent>(SceneRootNodePtr->GetComponentTemplate())));
}

void SSCSEditor::PasteNodes()
{
	const FScopedTransaction Transaction(LOCTEXT("PasteComponents", "Paste Component(s)"));

	// Mark the Blueprint as about to be modified
	Blueprint->Modify();

	// Mark the SCS and all SCS nodes as about to be modified
	SaveSCSCurrentState(Blueprint->SimpleConstructionScript);

	// Get the text from the clipboard
	FString TextToImport;
	FPlatformMisc::ClipboardPaste(TextToImport);

	// Get a new component object factory for the clipboard content
	TSharedRef<FSCSEditorComponentObjectTextFactory> Factory = FSCSEditorComponentObjectTextFactory::Get(TextToImport);

	// Clear the current selection
	SCSTreeWidget->ClearSelection();

	// Create a new tree node for each new (pasted) component
	TMap<FName, FSCSEditorTreeNodePtrType> NewNodeMap;
	for(auto NewObjectIt = Factory->NewObjectMap.CreateIterator(); NewObjectIt; ++NewObjectIt)
	{
		// Get the component object instance
		UActorComponent* NewActorComponent = NewObjectIt->Value;
		check(NewActorComponent);

		// Relocate the instance from the transient package to the BPGC and assign it a unique object name
		NewActorComponent->Rename(NULL, Blueprint->GeneratedClass, REN_DontCreateRedirectors|REN_DoNotDirty);

		// Create a new SCS node to contain the new component and add it to the tree
		NewActorComponent = AddNewNode(SCS->CreateNode(NewActorComponent), NULL, false, false);
		if(NewActorComponent)
		{
			// Locate the node that corresponds to the new component template
			FSCSEditorTreeNodePtrType NewNodePtr = FindTreeNode(NewActorComponent);
			if(NewNodePtr.IsValid())
			{
				// Add the new node to the node map
				NewNodeMap.Add(NewObjectIt->Key, NewNodePtr);

				// Update the selection to include the new node
				SCSTreeWidget->SetItemSelection(NewNodePtr, true);
			}
		}
	}

	// Restore the node hierarchy from the original copy
	for(auto NodeIt = NewNodeMap.CreateConstIterator(); NodeIt; ++NodeIt)
	{
		// If an entry exists in the set of known parent nodes for the current node
		if(Factory->ParentMap.Contains(NodeIt->Key))
		{
			// Get the parent node name
			FName ParentName = Factory->ParentMap[NodeIt->Key];
			if(NewNodeMap.Contains(ParentName))
			{
				// Reattach the current node to the parent node (this will also handle detachment from the scene root node)
				NewNodeMap[ParentName]->AddChild(NodeIt->Value);

				// Ensure that the new node is expanded to show the child node(s)
				SCSTreeWidget->SetItemExpansion(NewNodeMap[ParentName], true);
			}
		}
	}

	// Modify the Blueprint generated class structure (this will also call UpdateTree() as a result)
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

bool SSCSEditor::CanDeleteNodes() const
{
	if(!InEditingMode())
	{
		return false;
	}

	TArray<FSCSEditorTreeNodePtrType> SelectedNodes = SCSTreeWidget->GetSelectedItems();
	for (int32 i = 0; i < SelectedNodes.Num(); ++i)
	{
		if (!SelectedNodes[i]->CanDelete()) {return false;}
	}
	return SelectedNodes.Num() > 0;
}

void SSCSEditor::OnDeleteNodes()
{
	const FScopedTransaction Transaction( LOCTEXT("RemoveComponent", "Remove Component") );

	// Get the current render info for the blueprint. If this is NULL then the blueprint is not currently visualizable (no visible primitive components)
	FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( Blueprint );

	// Saving objects for restoring purpose.
	Blueprint->Modify();
	SSCSEditor::SaveSCSCurrentState( Blueprint->SimpleConstructionScript );

	// Remove node from SCS
	TArray<FSCSEditorTreeNodePtrType> SelectedNodes = SCSTreeWidget->GetSelectedItems();
	for (int32 i = 0; i < SelectedNodes.Num(); ++i)
	{
		auto Node = SelectedNodes[i];

		RemoveComponentNode(Node);
	}

	// Will call UpdateTree as part of OnBlueprintChanged handling
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	// Do this AFTER marking the Blueprint as modified
	UpdateSelectionFromNodes(SCSTreeWidget->GetSelectedItems());

	// If we had a thumbnail before we deleted any components, check to see if we should clear it
	// If we deleted the final visualizable primitive from the blueprint, GetRenderingInfo should return NULL
	FThumbnailRenderingInfo* NewRenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( Blueprint );
	if ( RenderInfo && !NewRenderInfo )
	{
		// We removed the last visible primitive component, clear the thumbnail
		const FString BPFullName = FString::Printf(TEXT("%s %s"), *Blueprint->GetClass()->GetName(), *Blueprint->GetPathName());
		UPackage* BPPackage = Blueprint->GetOutermost();
		ThumbnailTools::CacheEmptyThumbnail( BPFullName, BPPackage );
	}
}

void SSCSEditor::RemoveComponentNode(FSCSEditorTreeNodePtrType InNodePtr)
{
	check(InNodePtr.IsValid());

	// Remove any instances of variable accessors from the blueprint graphs
	FBlueprintEditorUtils::RemoveVariableNodes(Blueprint, InNodePtr->GetVariableName());

	// Clear selection if current
	if(SCSTreeWidget->GetSelectedItems().Contains(InNodePtr))
	{
		SCSTreeWidget->ClearSelection();
	}

	USCS_Node* SCS_Node = InNodePtr->GetSCSNode();
	if(SCS_Node != NULL)
	{
		// Remove node from SCS tree
		check(SCS_Node->GetSCS() == SCS);
		SCS->RemoveNodeAndPromoteChildren(SCS_Node);

		// Clear the delegate
		SCS_Node->SetOnNameChanged(FSCSNodeNameChanged());
	}
}

void SSCSEditor::UpdateSelectionFromNodes(const TArray<FSCSEditorTreeNodePtrType> &SelectedNodes )
{
	// Convert the selection set to an array of UObject* pointers
	FString InspectorTitle;
	TArray<UObject*> InspectorObjects;
	InspectorObjects.Empty(SelectedNodes.Num());
	for (auto NodeIt = SelectedNodes.CreateConstIterator(); NodeIt; ++NodeIt)
	{
		auto NodePtr = *NodeIt;
		if(NodePtr.IsValid() && NodePtr->CanEditDefaults())
		{
			InspectorTitle = NodePtr->GetDisplayString();
			InspectorObjects.Add(NodePtr->GetComponentTemplate());
		}
	}

	// Update the details panel
	if (KismetInspectorPtr.IsValid())
	{
		SKismetInspector::FShowDetailsOptions Options(InspectorTitle, true);
		KismetInspectorPtr.Pin()->ShowDetailsForObjects(InspectorObjects, Options);
	}
}

void SSCSEditor::RefreshSelectionDetails()
{
	UpdateSelectionFromNodes(SCSTreeWidget->GetSelectedItems());
}

void SSCSEditor::OnTreeSelectionChanged(FSCSEditorTreeNodePtrType, ESelectInfo::Type /*SelectInfo*/)
{
	UpdateSelectionFromNodes(SCSTreeWidget->GetSelectedItems());

	if( Kismet2Ptr.IsValid() )
	{
		TSharedPtr<class SSCSEditorViewport> ViewportPtr = Kismet2Ptr.Pin()->GetSCSViewport();
		if (ViewportPtr.IsValid())
		{
			ViewportPtr->OnComponentSelectionChanged();
		}
	}

	// Update the selection visualization
	AActor* EditorActorInstance = SCS->GetComponentEditorActorInstance();
	if (EditorActorInstance != NULL)
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		EditorActorInstance->GetComponents(PrimitiveComponents);

		for (int32 Idx = 0; Idx < PrimitiveComponents.Num(); ++Idx)
		{
			PrimitiveComponents[Idx]->PushSelectionToProxy();
		}
	}
}

bool SSCSEditor::IsNodeInSimpleConstructionScript( USCS_Node* Node ) const
{
	check(Node);

	USimpleConstructionScript* NodeSCS = Node->GetSCS();
	if(NodeSCS != NULL)
	{
		return NodeSCS->GetAllNodes().Contains(Node);
	}
	
	return false;
}

FSCSEditorTreeNodePtrType SSCSEditor::AddTreeNode(USCS_Node* InSCSNode, FSCSEditorTreeNodePtrType InParentNodePtr)
{
	FSCSEditorTreeNodePtrType NewNodePtr;

	check(InSCSNode != NULL);
	check(InSCSNode->ComponentTemplate != NULL);
	checkf(InSCSNode->ParentComponentOrVariableName == NAME_None
		|| (!InSCSNode->bIsParentComponentNative && InParentNodePtr->GetSCSNode() != NULL && InParentNodePtr->GetSCSNode()->VariableName == InSCSNode->ParentComponentOrVariableName)
		|| (InSCSNode->bIsParentComponentNative && InParentNodePtr->GetComponentTemplate() != NULL && InParentNodePtr->GetComponentTemplate()->GetFName() == InSCSNode->ParentComponentOrVariableName),
			TEXT("Failed to add SCS node %s to tree:\n- bIsParentComponentNative=%d\n- Stored ParentComponentOrVariableName=%s\n- Actual ParentComponentOrVariableName=%s"),
				*InSCSNode->VariableName.ToString(),
				!!InSCSNode->bIsParentComponentNative,
				*InSCSNode->ParentComponentOrVariableName.ToString(),
				!InSCSNode->bIsParentComponentNative
					? (InParentNodePtr->GetSCSNode() != NULL ? *InParentNodePtr->GetSCSNode()->VariableName.ToString() : TEXT("NULL"))
					: (InParentNodePtr->GetComponentTemplate() != NULL ? *InParentNodePtr->GetComponentTemplate()->GetFName().ToString() : TEXT("NULL")));
	
	// Determine whether or not the given node is inherited from a parent Blueprint
	USimpleConstructionScript* NodeSCS = InSCSNode->GetSCS();
	const bool bIsInherited = NodeSCS != Blueprint->SimpleConstructionScript;

	if(InSCSNode->ComponentTemplate->IsA(USceneComponent::StaticClass()))
	{
		FSCSEditorTreeNodePtrType ParentPtr = InParentNodePtr.IsValid() ? InParentNodePtr : SceneRootNodePtr;
		if(ParentPtr.IsValid())
		{
			// do this first, because we need a FSCSEditorTreeNodePtrType for the new node
			NewNodePtr = ParentPtr->AddChild(InSCSNode, bIsInherited);

			bool bParentIsEditorOnly = ParentPtr->GetComponentTemplate()->IsEditorOnly();
			// if you can't nest this new node under the proposed parent (then swap the two)
			if (bParentIsEditorOnly && !InSCSNode->ComponentTemplate->IsEditorOnly() && ParentPtr->CanReparent())
			{
				FSCSEditorTreeNodePtrType OldParentPtr = ParentPtr;
				ParentPtr = OldParentPtr->GetParent();

				OldParentPtr->RemoveChild(NewNodePtr);
				NodeSCS->RemoveNode(OldParentPtr->GetSCSNode());

				// if the grandparent node is invalid (assuming this means that the parent node was the scene-root)
				if (!ParentPtr.IsValid())
				{
					check(OldParentPtr == SceneRootNodePtr);
					SceneRootNodePtr = NewNodePtr;
					NodeSCS->AddNode(SceneRootNodePtr->GetSCSNode());
				}
				else 
				{
					ParentPtr->AddChild(NewNodePtr);
				}

				// move the proposed parent in as a child to the new node
				NewNodePtr->AddChild(OldParentPtr);
			} // if bParentIsEditorOnly...

			// Expand parent nodes by default
			SCSTreeWidget->SetItemExpansion(ParentPtr, true);
		}
		//else, if !SceneRootNodePtr.IsValid(), make it the scene root node if it has not been set yet
		else 
		{
			// Create a new root node
			NewNodePtr = MakeShareable(new FSCSEditorTreeNode(InSCSNode, bIsInherited));

			// Add it to the root set
			NodeSCS->AddNode(InSCSNode);
			RootNodes.Add(NewNodePtr);

			// Make it the scene root node
			SceneRootNodePtr = NewNodePtr;

			// Expand the scene root node by default
			SCSTreeWidget->SetItemExpansion(SceneRootNodePtr, true);
		}
	}
	else
	{
		// If the given SCS node does not contain a scene component template, we create a new root node
		NewNodePtr = MakeShareable(new FSCSEditorTreeNode(InSCSNode, bIsInherited));

		// Add it to the root set prior to the scene root node that represents the start of the scene hierarchy
		int32 SceneRootNodeIndex = RootNodes.Find(SceneRootNodePtr);
		if(SceneRootNodeIndex != INDEX_NONE)
		{
			RootNodes.Insert(NewNodePtr, SceneRootNodeIndex);
		}
		else
		{
			RootNodes.Add(NewNodePtr);
		}

		// If the SCS root node array does not already contain the given node, this will add it (this should only occur after node creation)
		if(NodeSCS != NULL)
		{
			NodeSCS->AddNode(InSCSNode);
		}
	}

	// Recursively add the given SCS node's child nodes
	for(int32 NodeIndex = 0; NodeIndex < InSCSNode->ChildNodes.Num(); ++NodeIndex)
	{
		AddTreeNode(InSCSNode->ChildNodes[NodeIndex], NewNodePtr);
	}

	return NewNodePtr;
}

FSCSEditorTreeNodePtrType SSCSEditor::AddTreeNode(USceneComponent* InSceneComponent)
{
	FSCSEditorTreeNodePtrType NewNodePtr;

	check(InSceneComponent != NULL);

	// If the given component has a parent
	if(InSceneComponent->AttachParent != NULL)
	{
		// Attempt to find the parent node in the current tree
		FSCSEditorTreeNodePtrType ParentNodePtr = FindTreeNode(InSceneComponent->AttachParent);
		if(!ParentNodePtr.IsValid())
		{
			// Recursively add the parent node to the tree if it does not exist yet
			ParentNodePtr = AddTreeNode(InSceneComponent->AttachParent);
		}

		// Add a new tree node for the given scene component
		check(ParentNodePtr.IsValid());
		NewNodePtr = ParentNodePtr->AddChild(InSceneComponent);

		// Expand parent nodes by default
		SCSTreeWidget->SetItemExpansion(ParentNodePtr, true);
	}
	else
	{
		// Make it the scene root node if it has not been set yet
		if(!SceneRootNodePtr.IsValid())
		{
			// Create a new root node
			NewNodePtr = MakeShareable(new FSCSEditorTreeNode(InSceneComponent));

			// Add it to the root set
			RootNodes.Add(NewNodePtr);

			// Make it the scene root node
			SceneRootNodePtr = NewNodePtr;

			// Expand the scene root node by default
			SCSTreeWidget->SetItemExpansion(SceneRootNodePtr, true);
		}
		else
		{
			NewNodePtr = SceneRootNodePtr->AddChild(InSceneComponent);
		}
	}

	return NewNodePtr;
}

FSCSEditorTreeNodePtrType SSCSEditor::FindTreeNode(const USCS_Node* InSCSNode, FSCSEditorTreeNodePtrType InStartNodePtr) const
{
	FSCSEditorTreeNodePtrType NodePtr;
	if(InSCSNode != NULL)
	{
		// Start at the scene root node if none was given
		if(!InStartNodePtr.IsValid())
		{
			InStartNodePtr = SceneRootNodePtr;
		}

		if(InStartNodePtr.IsValid())
		{
			// Check to see if the given SCS node matches the given tree node
			if(InStartNodePtr->GetSCSNode() == InSCSNode)
			{
				NodePtr = InStartNodePtr;
			}
			else
			{
				// Recursively search for the node in our child set
				NodePtr = InStartNodePtr->FindChild(InSCSNode);
				if(!NodePtr.IsValid())
				{
					for(int32 i = 0; i < InStartNodePtr->GetChildren().Num() && !NodePtr.IsValid(); ++i)
					{
						NodePtr = FindTreeNode(InSCSNode, InStartNodePtr->GetChildren()[i]);
					}
				}
			}
		}
	}

	return NodePtr;
}

FSCSEditorTreeNodePtrType SSCSEditor::FindTreeNode(const UActorComponent* InComponent, FSCSEditorTreeNodePtrType InStartNodePtr) const
{
	FSCSEditorTreeNodePtrType NodePtr;
	if(InComponent != NULL)
	{
		// Start at the scene root node if none was given
		if(!InStartNodePtr.IsValid())
		{
			InStartNodePtr = SceneRootNodePtr;
		}

		if(InStartNodePtr.IsValid())
		{
			// Check to see if the given component template matches the given tree node
			if(InStartNodePtr->GetComponentTemplate() == InComponent)
			{
				NodePtr = InStartNodePtr;
			}
			else
			{
				// Recursively search for the node in our child set
				NodePtr = InStartNodePtr->FindChild(InComponent);
				if(!NodePtr.IsValid())
				{
					for(int32 i = 0; i < InStartNodePtr->GetChildren().Num() && !NodePtr.IsValid(); ++i)
					{
						NodePtr = FindTreeNode(InComponent, InStartNodePtr->GetChildren()[i]);
					}
				}
			}
		}
	}

	return NodePtr;
}

void SSCSEditor::OnItemScrolledIntoView( FSCSEditorTreeNodePtrType InItem, const TSharedPtr<ITableRow>& InWidget)
{
	USCS_Node* SCS_Node = DeferredRenameRequest.Get();
	if( SCS_Node && SCS_Node == InItem->GetSCSNode() )
	{
		DeferredRenameRequest.Reset();
		InItem->OnRequestRename(bIsDeferredRenameRequestTransactional);
	}
}

void SSCSEditor::OnRenameComponent(bool bTransactional)
{
	TArray< FSCSEditorTreeNodePtrType > SelectedItems = SCSTreeWidget->GetSelectedItems();

	// Should already be prevented from making it here.
	check(SelectedItems.Num() == 1);

	SCSTreeWidget->RequestScrollIntoView(SelectedItems[0]);
	DeferredRenameRequest = SelectedItems[0]->GetSCSNode();
	bIsDeferredRenameRequestTransactional = bTransactional;
}

bool SSCSEditor::CanRenameComponent() const
{
	return InEditingMode() && SCSTreeWidget->GetSelectedItems().Num() == 1 && SCSTreeWidget->GetSelectedItems()[0]->CanRename();
}

void SSCSEditor::GetCollapsedNodes(const FSCSEditorTreeNodePtrType& InNodePtr, TSet<FSCSEditorTreeNodePtrType>& OutCollapsedNodes) const
{
	if(InNodePtr.IsValid())
	{
		const TArray<FSCSEditorTreeNodePtrType>& Children = InNodePtr->GetChildren();
		if(Children.Num() > 0)
		{
			if(!SCSTreeWidget->IsItemExpanded(InNodePtr))
			{
				OutCollapsedNodes.Add(InNodePtr);
			}

			for(int32 i = 0; i < Children.Num(); ++i)
			{
				GetCollapsedNodes(Children[i], OutCollapsedNodes);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

