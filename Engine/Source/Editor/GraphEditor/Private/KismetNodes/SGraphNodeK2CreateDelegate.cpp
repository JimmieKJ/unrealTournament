// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SGraphNodeK2Base.h"
#include "SGraphNodeK2CreateDelegate.h"

FString SGraphNodeK2CreateDelegate::FunctionDescription(const UFunction* Function)
{
	if(!Function || !Function->GetOuter())
	{
		return NSLOCTEXT("GraphNodeK2Create", "Error", "Error").ToString();
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	//FString Result = Function->GetOuter()->GetName() + TEXT("::") + Function->GetName();
	//Result += TEXT("(");

	FString Result = Function->GetName() + TEXT("(");
	bool bFirst = true;
	for (TFieldIterator<UProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* Param = *PropIt;
		const bool bIsFunctionInput = Param && (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm));
		if(bIsFunctionInput)
		{
			if(!bFirst)
			{
				Result += TEXT(", ");
			}
			if(Result.Len() > 32)
			{
				Result += TEXT("...");
				break;
			}
			Result += Param->GetName();
			bFirst = false;
		}
	}

	Result += TEXT(")");
	return Result;
}

void SGraphNodeK2CreateDelegate::Construct( const FArguments& InArgs, UK2Node* InNode )
{
	GraphNode = InNode;
	UpdateGraphNode();
}

FText SGraphNodeK2CreateDelegate::GetCurrentFunctionDescription() const
{
	UK2Node_CreateDelegate* Node = Cast<UK2Node_CreateDelegate>(GraphNode);
	UFunction* FunctionSignature = Node ? Node->GetDelegateSignature() : NULL;
	UClass* ScopeClass = Node ? Node->GetScopeClass() : NULL;

	if(!FunctionSignature || !ScopeClass)
	{
		return NSLOCTEXT("GraphNodeK2Create", "NoneLabel", "None");
	}

	if (const auto Func = FindField<UFunction>(ScopeClass, Node->GetFunctionName()))
	{
		return FText::FromString(FunctionDescription(Func));
	}

	if (Node->GetFunctionName() != NAME_None)
	{
		return FText::Format(NSLOCTEXT("GraphNodeK2Create", "ErrorLabelFmt", "Error? {0}"), FText::FromName(Node->GetFunctionName()));
	}

	return NSLOCTEXT("GraphNodeK2Create", "SelectFunctionLabel", "Select Function");
}

TSharedRef<ITableRow> SGraphNodeK2CreateDelegate::HandleGenerateRowFunction(TSharedPtr<FFunctionItemData> FunctionItemData, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(FunctionItemData.IsValid());
	return SNew(STableRow< TSharedPtr<FFunctionItemData> >, OwnerTable).Content()
		[
			SNew( STextBlock ).Text( FText::FromString(FunctionItemData->Description) )
		];
}

void SGraphNodeK2CreateDelegate::OnFunctionSelected(TSharedPtr<FFunctionItemData> FunctionItemData, ESelectInfo::Type SelectInfo)
{
	if(FunctionItemData.IsValid())
	{
		if(UK2Node_CreateDelegate* Node = Cast<UK2Node_CreateDelegate>(GraphNode))
		{
			Node->SetFunction(FunctionItemData->Name);
			Node->HandleAnyChange(true);

			auto SelectFunctionWidgetPtr = SelectFunctionWidget.Pin();
			if(SelectFunctionWidgetPtr.IsValid())
			{
				SelectFunctionWidgetPtr->SetIsOpen(false);
			}
		}
	}
}

void SGraphNodeK2CreateDelegate::CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox)
{
	if(UK2Node_CreateDelegate* Node = Cast<UK2Node_CreateDelegate>(GraphNode))
	{
		UFunction* FunctionSignature = Node->GetDelegateSignature();
		UClass* ScopeClass = Node->GetScopeClass();

		if(FunctionSignature && ScopeClass)
		{
			FunctionDataItems.Empty();
			for(TFieldIterator<UFunction> It(ScopeClass); It; ++It)
			{
				UFunction* Func = *It;
				if (Func && FunctionSignature->IsSignatureCompatibleWith(Func) && 
					UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(Func))
				{
					TSharedPtr<FFunctionItemData> ItemData = MakeShareable(new FFunctionItemData());
					ItemData->Name = Func->GetFName();
					ItemData->Description = FunctionDescription(Func);
					FunctionDataItems.Add(ItemData);
				}
			}

			TSharedRef<SComboButton> SelectFunctionWidgetRef = SNew(SComboButton)
				.Method(EPopupMethod::UseCurrentWindow)
				.ButtonContent()
				[
					SNew(STextBlock)
						.Text(this, &SGraphNodeK2CreateDelegate::GetCurrentFunctionDescription)
				]
				.MenuContent()
				[
					SNew(SListView<TSharedPtr<FFunctionItemData> >)
						.ListItemsSource( &FunctionDataItems )
						.OnGenerateRow(this, &SGraphNodeK2CreateDelegate::HandleGenerateRowFunction)
						.OnSelectionChanged(this, &SGraphNodeK2CreateDelegate::OnFunctionSelected)
				];

			MainBox->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Fill)
				[
					SelectFunctionWidgetRef
				];

			SelectFunctionWidget = SelectFunctionWidgetRef;
		}
	}
}

SGraphNodeK2CreateDelegate::~SGraphNodeK2CreateDelegate()
{
	auto SelectFunctionWidgetPtr = SelectFunctionWidget.Pin();
	if(SelectFunctionWidgetPtr.IsValid())
	{
		SelectFunctionWidgetPtr->SetIsOpen(false);
	}
}