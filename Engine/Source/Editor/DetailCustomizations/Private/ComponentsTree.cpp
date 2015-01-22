// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "ComponentsTree.h"
#include "Editor/UnrealEd/Public/SComponentClassCombo.h"
#include "Editor/UnrealEd/Public/ClassIconFinder.h"

//////////////////////////////////////////////////////////////////////////

FComponentTreeNode::FComponentTreeNode(UActorComponent* InComponent)
{
	Component = InComponent;

	USceneComponent* SceneComp = Cast<USceneComponent>(InComponent);
	if(SceneComp != NULL)
	{
		for(int32 ChildIdx=0; ChildIdx<SceneComp->AttachChildren.Num(); ChildIdx++)
		{
			USceneComponent* ChildComp = SceneComp->AttachChildren[ChildIdx];
			if(ChildComp && !ChildComp->IsPendingKill())
			{
				FComponentTreeNodePtrType ChildNode = MakeShareable(new FComponentTreeNode(ChildComp));
				ChildNodes.Add(ChildNode);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void SComponentRowWidget::Construct( const FArguments& InArgs, FComponentTreeNodePtrType InNodePtr )
{
	NodePtr = InNodePtr;

	FString ComponentName;
	const FSlateBrush* ComponentIcon = FEditorStyle::GetBrush("SCS.NativeComponent");
	FSlateColor ComponentColor = FLinearColor::White;

	TSharedPtr<FComponentTreeNode> Node = NodePtr.Pin();
	if(Node.IsValid() && Node->Component.IsValid())
	{
		ComponentName = Node->Component->GetName();
		ComponentIcon = FClassIconFinder::FindIconForClass( Node->Component->GetClass(), TEXT("SCS.Component") );

		// Color native and BP-made components in different colors
		if(Node->Component->IsDefaultSubobject())
		{
			ComponentColor = FLinearColor(0.08f,0.15f,0.6f);
		}
		else if(Node->Component->bCreatedByConstructionScript)
		{
			ComponentColor = FLinearColor(0.08f,0.35f,0.6f);
		}
	}

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(ComponentIcon)
			.ColorAndOpacity(ComponentColor)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4,0,4,0)
		[
			SNew(STextBlock)
			.Text(ComponentName)
		]
	];
}

//////////////////////////////////////////////////////////////////////////

// Don't show any 'disable on instance' properties, these are instances of components we are seeing
static bool IsPropertyVisible( const FPropertyAndParent& PropertyAndParent )
{

	if ( PropertyAndParent.Property.HasAllPropertyFlags(CPF_DisableEditOnInstance) )
	{
		return false;
	}

	return true;
}

void SComponentsTree::Construct( const FArguments& InArgs, AActor* InActor )
{
	Actor = InActor;

	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs( /*bUpdateFromSelection=*/ false, /*bLockable=*/ false, /*bAllowSearch=*/ true, /*bObjectsUseNameArea=*/ true, /*bHideSelectionTip=*/ true );
	DetailsViewArgs.bHideActorNameArea = true;
	PropertyView = EditModule.CreateDetailView( DetailsViewArgs );
	// Setup delegate to hide 'edit default only' properties
	PropertyView->SetIsPropertyVisibleDelegate( FIsPropertyVisible::CreateStatic(&IsPropertyVisible) );

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding( 2 )
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
		.Content()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(TreeWidget, SComponentTreeType)
				.TreeItemsSource( &RootNodes )
				.OnGenerateRow( this, &SComponentsTree::MakeTableRowWidget )
				.OnGetChildren( this, &SComponentsTree::OnGetChildrenForTree )
				.OnSelectionChanged( this, &SComponentsTree::OnTreeSelectionChanged )
				.ItemHeight( 24 )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f, 0.f, 0.f)
			[
				SNew(SComponentClassCombo)
				.OnComponentClassSelected(this, &SComponentsTree::OnSelectedCompClass)
			]
			+SVerticalBox::Slot()
			.Padding(0.f, 2.f, 0.f, 0.f)
			.AutoHeight()
			[
				PropertyView.ToSharedRef()
			]
		]
	];

	UpdateTree();
}

TSharedRef<ITableRow> SComponentsTree::MakeTableRowWidget( FComponentTreeNodePtrType InNodePtr, const TSharedRef<STableViewBase>& OwnerTable )
{
	return 
		SNew(STableRow<FComponentTreeNodePtrType>, OwnerTable)
		[
			SNew(SComponentRowWidget, InNodePtr)
		];
}

void SComponentsTree::OnGetChildrenForTree( FComponentTreeNodePtrType InNodePtr, TArray<FComponentTreeNodePtrType>& OutChildren )
{
	if(InNodePtr.IsValid())
	{
		OutChildren = InNodePtr->ChildNodes;
	}
}

void SComponentsTree::OnTreeSelectionChanged(FComponentTreeNodePtrType InSelectedNodePtr, ESelectInfo::Type SelectInfo)
{
	if(InSelectedNodePtr.IsValid() && InSelectedNodePtr->Component.IsValid())
	{
		TArray<UObject*> Objects;
		Objects.Add(InSelectedNodePtr->Component.Get());

		PropertyView->SetObjects(Objects, false);
	}
}

void SComponentsTree::OnSelectedCompClass(TSubclassOf<UActorComponent> CompClass)
{
	if(Actor.IsValid())
	{
		// Create new component
		UActorComponent* NewComp = NewObject<UActorComponent>(Actor.Get(), CompClass);
		check(NewComp);

		// Add to SerializedComponents array so it gets saved
		Actor->SerializedComponents.Add(NewComp);

		// If a scene component, and actor has a root component, attach it to root
		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewComp);
		USceneComponent* RootComp = Actor->GetRootComponent();
		if(NewSceneComp && RootComp)
		{
			NewSceneComp->AttachTo(RootComp);
		}

		// Register component
		NewSceneComp->RegisterComponent();

		// Update tree to see result
		UpdateTree();
	}
}


void SComponentsTree::UpdateTree()
{
	check(TreeWidget.IsValid());

	RootNodes.Empty();

	if(Actor.IsValid())
	{
		// Add root component (which will add its children)
		USceneComponent* RootComp = Actor->GetRootComponent();
		if(RootComp && !RootComp->IsPendingKill())
		{
			FComponentTreeNodePtrType RootNode = MakeShareable(new FComponentTreeNode(RootComp));
			RootNodes.Add(RootNode);
		}

		// Add non-scene components in Components array
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		for(int32 CompIdx=0; CompIdx<Components.Num(); CompIdx++)
		{
			UActorComponent* Comp = Components[CompIdx];
			if(!Comp->IsA(USceneComponent::StaticClass()) && !Comp->IsPendingKill())
			{
				FComponentTreeNodePtrType Node = MakeShareable(new FComponentTreeNode(Comp));
				RootNodes.Add(Node);
			}
		}
	}

	// refresh widget
	TreeWidget->RequestTreeRefresh();
}
