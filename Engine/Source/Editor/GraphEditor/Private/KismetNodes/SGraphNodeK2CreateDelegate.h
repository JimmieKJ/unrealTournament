// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphNodeK2CreateDelegate : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2CreateDelegate){}
	SLATE_END_ARGS()

	struct FFunctionItemData
	{
		FName Name;
		FString Description;
	};
	TArray<TSharedPtr<FFunctionItemData>> FunctionDataItems;
	TWeakPtr<SComboButton> SelectFunctionWidget;
public:
	static FString FunctionDescription(const UFunction* Function);

	virtual ~SGraphNodeK2CreateDelegate();
	void Construct(const FArguments& InArgs, UK2Node* InNode);
	virtual void CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox) override;

protected:
	FText GetCurrentFunctionDescription() const;
	TSharedRef<ITableRow> HandleGenerateRowFunction(TSharedPtr<FFunctionItemData> FunctionItemData, const TSharedRef<STableViewBase>& OwnerTable);
	void OnFunctionSelected(TSharedPtr<FFunctionItemData> FunctionItemData, ESelectInfo::Type SelectInfo);
};

