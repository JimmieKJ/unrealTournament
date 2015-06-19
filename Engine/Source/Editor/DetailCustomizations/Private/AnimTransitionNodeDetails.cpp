// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"

#include "AnimationTransitionGraph.h"
#include "AnimGraphDefinitions.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateConduitNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"
#include "AnimTransitionNodeDetails.h"
#include "KismetEditorUtilities.h"
#include "SKismetLinearExpression.h"
#include "STextEntryPopup.h"
#include "SExpandableArea.h"
#include "BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "FAnimStateNodeDetails"

/////////////////////////////////////////////////////////////////////////

TSharedRef<IDetailCustomization> FAnimTransitionNodeDetails::MakeInstance()
{
	return MakeShareable( new FAnimTransitionNodeDetails );
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FAnimTransitionNodeDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Get a handle to the node we're viewing
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailBuilder.GetDetailsView().GetSelectedObjects();
	for (int32 ObjectIndex = 0; !TransitionNode.IsValid() && (ObjectIndex < SelectedObjects.Num()); ++ObjectIndex)
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if (CurrentObject.IsValid())
		{
			TransitionNode = Cast<UAnimStateTransitionNode>(CurrentObject.Get());
		}
	}

	bool bTransitionToConduit = false;
	if (UAnimStateTransitionNode* TransitionNodePtr = TransitionNode.Get())
	{
		UAnimStateNodeBase* NextState = TransitionNodePtr->GetNextState();
		bTransitionToConduit = (NextState != NULL) && (NextState->IsA<UAnimStateConduitNode>());
	}

	////////////////////////////////////////

	IDetailCategoryBuilder& TransitionCategory = DetailBuilder.EditCategory("Transition", LOCTEXT("TransitionCategoryTitle", "Transition") );

	if (bTransitionToConduit)
	{
		// Transitions to conduits are just shorthand for some other real transition;
		// All of the blend related settings are ignored, so hide them.
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, Bidirectional));
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, CrossfadeDuration));
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, CrossfadeMode));
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, LogicType));
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, PriorityOrder));
	}
	else
	{
		TransitionCategory.AddCustomRow( LOCTEXT("TransitionEventPropertiesCategoryLabel", "Transition") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT("TransitionEventPropertiesCategoryLabel", "Transition") )
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		];

		TransitionCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, PriorityOrder)).DisplayName(LOCTEXT("PriorityOrderLabel", "Priority Order"));
		TransitionCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, Bidirectional)).DisplayName(LOCTEXT("BidirectionalLabel", "Bidirectional"));
		TransitionCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, LogicType)).DisplayName(LOCTEXT("BlendLogicLabel", "Blend Logic") );

		UAnimStateTransitionNode* TransNode = TransitionNode.Get();
		if (TransitionNode != NULL)
		{
			// The sharing option for the rule
			TransitionCategory.AddCustomRow( LOCTEXT("TransitionRuleSharingLabel", "Transition Rule Sharing") )
			[
				GetWidgetForInlineShareMenu(TEXT("Transition Rule Sharing"), TransNode->SharedRulesName, TransNode->bSharedRules,
					FOnClicked::CreateSP(this, &FAnimTransitionNodeDetails::OnPromoteToSharedClick, true),
					FOnClicked::CreateSP(this, &FAnimTransitionNodeDetails::OnUnshareClick, true), 
					FOnGetContent::CreateSP(this, &FAnimTransitionNodeDetails::OnGetShareableNodesMenu, true))
			];


// 			TransitionCategory.AddRow()
// 				[
// 					SNew( STextBlock )
// 					.Text( TEXT("Crossfade Settings") )
// 					.Font( IDetailLayoutBuilder::GetDetailFontBold() )
// 				];


			// Show the rule itself
			UEdGraphPin* CanExecPin = NULL;
			if (UAnimationTransitionGraph* TransGraph = Cast<UAnimationTransitionGraph>(TransNode->BoundGraph))
			{
				if (UAnimGraphNode_TransitionResult* ResultNode = TransGraph->GetResultNode())
				{
					CanExecPin = ResultNode->FindPin(TEXT("bCanEnterTransition"));
				}
			}

			// indicate if a native transition rule applies to this
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TransitionNode.Get());
			if(Blueprint && Blueprint->ParentClass)
			{
				UAnimInstance* AnimInstance = CastChecked<UAnimInstance>(Blueprint->ParentClass->GetDefaultObject());
				if(AnimInstance)
				{
					UEdGraph* ParentGraph = TransitionNode->GetGraph();
					UAnimStateNodeBase* PrevState = TransitionNode->GetPreviousState();
					UAnimStateNodeBase* NextState = TransitionNode->GetNextState();
					if(PrevState != nullptr && NextState != nullptr && ParentGraph != nullptr)
					{
						FName FunctionName;
						if(AnimInstance->HasNativeTransitionBinding(ParentGraph->GetFName(), FName(*PrevState->GetStateName()), FName(*NextState->GetStateName()), FunctionName))
						{
							TransitionCategory.AddCustomRow( LOCTEXT("NativeBindingPresent", "Transition has native binding") )
							[
								SNew(STextBlock)
								.Text(FText::Format(LOCTEXT("NativeBindingPresent", "Transition has native binding to {0}()"), FText::FromName(FunctionName)))
								.Font( IDetailLayoutBuilder::GetDetailFontBold() )
							];
						}
					}
				}
			}

			TransitionCategory.AddCustomRow( CanExecPin ? CanExecPin->PinFriendlyName : FText::GetEmpty() )
			[
				SNew(SKismetLinearExpression, CanExecPin)
			];
		}




		IDetailCategoryBuilder& CrossfadeCategory = DetailBuilder.EditCategory("BlendSettings", LOCTEXT("BlendSettingsCategoryTitle", "BlendSettings") );
		if (TransitionNode != NULL)
		{
			// The sharing option for the crossfade settings
			CrossfadeCategory.AddCustomRow( LOCTEXT("TransitionCrossfadeSharingLabel", "Transition Crossfade Sharing") )
			[
				GetWidgetForInlineShareMenu(TEXT("Transition Crossfade Sharing"), TransNode->SharedCrossfadeName, TransNode->bSharedCrossfade,
					FOnClicked::CreateSP(this, &FAnimTransitionNodeDetails::OnPromoteToSharedClick, false), 
					FOnClicked::CreateSP(this, &FAnimTransitionNodeDetails::OnUnshareClick, false), 
					FOnGetContent::CreateSP(this, &FAnimTransitionNodeDetails::OnGetShareableNodesMenu, false))
			];
		}

		//@TODO: Gate editing these on shared non-authorative ones
		CrossfadeCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, CrossfadeDuration)).DisplayName( LOCTEXT("DurationLabel", "Duration") );
		CrossfadeCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, CrossfadeMode)).DisplayName( LOCTEXT("ModeLabel", "Mode") );

		// Add a button that is only visible when blend logic type is custom
		CrossfadeCategory.AddCustomRow( LOCTEXT("EditBlendGraph", "Edit Blend Graph") )
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.FillWidth(1)
			.Padding(0,0,10.0f,0)
			[
				SNew(SButton)
				.HAlign(HAlign_Right)
				.OnClicked(this, &FAnimTransitionNodeDetails::OnClickEditBlendGraph)
				.Visibility( this, &FAnimTransitionNodeDetails::GetBlendGraphButtonVisibility )
				.Text(LOCTEXT("EditBlendGraph", "Edit Blend Graph"))
			]
		];





		IDetailCategoryBuilder& NotificationCategory = DetailBuilder.EditCategory("Notifications", LOCTEXT("NotificationsCategoryTitle", "Notifications") );

		NotificationCategory.AddCustomRow( LOCTEXT("StartTransitionEventPropertiesCategoryLabel", "Start Transition Event") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT("StartTransitionEventPropertiesCategoryLabel", "Start Transition Event") )
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		];
		CreateTransitionEventPropertyWidgets(NotificationCategory, TEXT("TransitionStart"));


		NotificationCategory.AddCustomRow( LOCTEXT("EndTransitionEventPropertiesCategoryLabel", "End Transition Event" ) ) 
		[
			SNew( STextBlock )
			.Text( LOCTEXT("EndTransitionEventPropertiesCategoryLabel", "End Transition Event" ) )
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		];
		CreateTransitionEventPropertyWidgets(NotificationCategory, TEXT("TransitionEnd"));

		NotificationCategory.AddCustomRow( LOCTEXT("InterruptTransitionEventPropertiesCategoryLabel", "Interrupt Transition Event") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT("InterruptTransitionEventPropertiesCategoryLabel", "Interrupt Transition Event") )
			.Font( IDetailLayoutBuilder::GetDetailFontBold() )
		];
		CreateTransitionEventPropertyWidgets(NotificationCategory, TEXT("TransitionInterrupt"));
	}

	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, TransitionStart));
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAnimStateTransitionNode, TransitionEnd));
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

/** RuleShare = true if we are sharing the rules of this transition (else we are implied to be sharing the crossfade settings) */
FReply FAnimTransitionNodeDetails::OnPromoteToSharedClick(bool RuleShare)
{
	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if ( Parent.IsValid() )
	{
		// Show dialog to enter new track name
		TSharedRef<STextEntryPopup> TextEntry =
			SNew(STextEntryPopup)
			.Label( LOCTEXT("PromoteAnimTransitionNodeToSharedLabel", "Shared Transition Name") )
			.OnTextCommitted(this, &FAnimTransitionNodeDetails::PromoteToShared, RuleShare);

		// Show dialog to enter new event name
		FSlateApplication::Get().PushMenu(
			Parent.ToSharedRef(),
			TextEntry,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
			);
		TextEntryWidget = TextEntry;
	}

	return FReply::Handled();
}

void FAnimTransitionNodeDetails::PromoteToShared(const FText& NewTransitionName, ETextCommit::Type CommitInfo, bool bRuleShare)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		if (UAnimStateTransitionNode* TransNode = TransitionNode.Get())
		{
			if (bRuleShare)
			{
				TransNode->MakeRulesShareable(NewTransitionName.ToString());
				AssignUniqueColorsToAllSharedNodes(TransNode->GetGraph());
			}
			else
			{
				TransNode->MakeCrossfadeShareable(NewTransitionName.ToString());
			}
		}
	}

	FSlateApplication::Get().DismissAllMenus();
}

FReply FAnimTransitionNodeDetails::OnUnshareClick(bool bUnshareRule)
{
	if (UAnimStateTransitionNode* TransNode = TransitionNode.Get())
	{
		if (bUnshareRule)
		{
			TransNode->UnshareRules();
		}
		else
		{
			TransNode->UnshareCrossade();
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> FAnimTransitionNodeDetails::OnGetShareableNodesMenu(bool bShareRules)
{
	FMenuBuilder MenuBuilder(true, NULL);

	FText SectionText;

	if (bShareRules)
	{
		SectionText = LOCTEXT("PickSharedAnimTransition", "Shared Transition Rules");
	}
	else
	{
		SectionText = LOCTEXT("PickSharedAnimCrossfadeSettings", "Shared Settings");
	}

	MenuBuilder.BeginSection("AnimTransitionSharableNodes", SectionText);

	if (UAnimStateTransitionNode* TransNode = TransitionNode.Get())
	{
		const UEdGraph* CurrentGraph = TransNode->GetGraph();

		// Loop through the graph and build a list of the unique shared transitions
		TMap<FString, UAnimStateTransitionNode*> SharedTransitions;

		for (int32 NodeIdx=0; NodeIdx < CurrentGraph->Nodes.Num(); NodeIdx++)
		{
			if (UAnimStateTransitionNode* GraphTransNode = Cast<UAnimStateTransitionNode>(CurrentGraph->Nodes[NodeIdx]))
			{
				if (bShareRules && !GraphTransNode->SharedRulesName.IsEmpty())
				{
					SharedTransitions.Add(GraphTransNode->SharedRulesName, GraphTransNode);
				}

				if (!bShareRules && !GraphTransNode->SharedCrossfadeName.IsEmpty())
				{
					SharedTransitions.Add(GraphTransNode->SharedCrossfadeName, GraphTransNode);
				}
			}
		}

		for (auto Iter = SharedTransitions.CreateIterator(); Iter; ++Iter)
		{
			FUIAction Action = FUIAction( FExecuteAction::CreateSP(this, &FAnimTransitionNodeDetails::BecomeSharedWith, Iter.Value(), bShareRules) );
			MenuBuilder.AddMenuEntry( FText::FromString( Iter.Key() ), LOCTEXT("ShaerdTransitionToolTip", "Use this shared transition"), FSlateIcon(), Action);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FAnimTransitionNodeDetails::BecomeSharedWith(UAnimStateTransitionNode* NewNode, bool bShareRules)
{
	if (UAnimStateTransitionNode* TransNode = TransitionNode.Get())
	{
		if (bShareRules)
		{
			TransNode->UseSharedRules(NewNode);
		}
		else
		{
			TransNode->UseSharedCrossfade(NewNode);
		}
	}
}

void FAnimTransitionNodeDetails::AssignUniqueColorsToAllSharedNodes(UEdGraph* CurrentGraph)
{
	TArray<UEdGraph*> SourceList;
	for (int32 idx=0; idx < CurrentGraph->Nodes.Num(); idx++)
	{
		if (UAnimStateTransitionNode* Node = Cast<UAnimStateTransitionNode>(CurrentGraph->Nodes[idx]))
		{
			if (Node->bSharedRules)
			{
				int32 colorIdx = SourceList.AddUnique(Node->BoundGraph)+1;

				FLinearColor SharedColor;
				SharedColor.R = (colorIdx & 1 ? 1.0f : 0.15f);
				SharedColor.G = (colorIdx & 2 ? 1.0f : 0.15f);
				SharedColor.B = (colorIdx & 4 ? 1.0f : 0.15f);
				SharedColor.A = 0.25f;

				// Storing this on the UAnimStateTransitionNode really bugs me. But its a pain to iterate over all the widget nodes at once
				// and we may want the shared color to be customizable in the details view
				Node->SharedColor = SharedColor;
			}
		}
	}
}

FReply FAnimTransitionNodeDetails::OnClickEditBlendGraph()
{
	if (UAnimStateTransitionNode* TransitionNodePtr = TransitionNode.Get())
	{
		if (TransitionNodePtr->CustomTransitionGraph != NULL)
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(TransitionNodePtr->CustomTransitionGraph);
		}
	}

	return FReply::Handled();
}

EVisibility FAnimTransitionNodeDetails::GetBlendGraphButtonVisibility() const
{
	if (UAnimStateTransitionNode* TransitionNodePtr = TransitionNode.Get())
	{
		if (TransitionNodePtr->LogicType == ETransitionLogicType::TLT_Custom)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}


void FAnimTransitionNodeDetails::CreateTransitionEventPropertyWidgets(IDetailCategoryBuilder& TransitionCategory, FString TransitionName)
{
	TSharedPtr<IPropertyHandle> NameProperty = TransitionCategory.GetParentLayout().GetProperty(*(TransitionName + TEXT(".NotifyName")));

	TransitionCategory.AddProperty( NameProperty )
		.DisplayName( LOCTEXT("CreateTransition_CustomBlueprintEvent", "Custom Blueprint Event") );
}

TSharedRef<SWidget> FAnimTransitionNodeDetails::GetWidgetForInlineShareMenu(FString TypeName, FString SharedName, bool bIsCurrentlyShared,  FOnClicked PromoteClick, FOnClicked DemoteClick, FOnGetContent GetContentMenu)
{
	const FText SharedNameText = bIsCurrentlyShared ? FText::FromString(SharedName) : LOCTEXT("SharedTransition", "Use Shared");

	return
		SNew(SExpandableArea)
		.AreaTitle(FText::FromString(TypeName))
		.InitiallyCollapsed(true)
		.BodyContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f )
			[
				SNew(SButton)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Fill)
				.OnClicked(bIsCurrentlyShared ? DemoteClick : PromoteClick)
				.Text(bIsCurrentlyShared ? LOCTEXT("UnshareLabel", "Unshare") : LOCTEXT("ShareLabel", "Promote To Shared"))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 2.0f )
			[
				SNew( SBorder )
				.BorderImage( FEditorStyle::GetBrush( "ToolBar.Background" ) )
				[
					SNew( SComboButton )
					.ToolTipText(LOCTEXT("UseSharedAnimationTransition_ToolTip", "Use Shared Transition"))
					.OnGetMenuContent( GetContentMenu )
					.ContentPadding(0.0f)
					.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonContent()
					[
						SNew(SEditableTextBox)
						.IsReadOnly(true)
						.MinDesiredWidth(128)
						.Text( SharedNameText )
						.Font( FEditorStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
					]
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE