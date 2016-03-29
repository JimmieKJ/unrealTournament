// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IViewportLayoutEntity;
struct FViewportConstructionArgs;

/** Definition of a custom viewport */
struct FViewportTypeDefinition
{
	typedef TFunction<TSharedRef<IViewportLayoutEntity>(const FViewportConstructionArgs&)> FFactoryFunctionType;

	template<typename T>
	static FViewportTypeDefinition FromType()
	{
		return FViewportTypeDefinition([](const FViewportConstructionArgs& Args) -> TSharedRef<IViewportLayoutEntity> {
			return MakeShareable(new T(Args));
		});
	}

	FViewportTypeDefinition(const FFactoryFunctionType& InFactoryFunction)
		: FactoryFunction(MoveTemp(InFactoryFunction))
	{}

	/** A UI command for toggling activation this viewport */
	TSharedPtr<FUICommandInfo> ToggleCommand;

	/** Function used to create a new instance of the viewport */
	FFactoryFunctionType FactoryFunction;
};
