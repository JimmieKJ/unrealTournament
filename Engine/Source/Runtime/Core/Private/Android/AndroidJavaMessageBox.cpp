// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AndroidJavaMessageBox.h"

FJavaAndroidMessageBox::FJavaAndroidMessageBox()
	: FJavaClassObject(GetClassName(), "()V")
	, SetCaptionMethod(GetClassMethod("setCaption", "(Ljava/lang/String;)V"))
	, SetTextMethod(GetClassMethod("setText", "(Ljava/lang/String;)V"))
	, AddButtonMethod(GetClassMethod("addButton", "(Ljava/lang/String;)V"))
	, ClearMethod(GetClassMethod("clear", "()V"))
	, ShowMethod(GetClassMethod("show", "()I"))
{
}

void FJavaAndroidMessageBox::SetCaption(const FString & Text)
{
	BindObjectToThread();
	CallMethod<void>(SetCaptionMethod, GetJString(Text));
}

void FJavaAndroidMessageBox::SetText(const FString & Text)
{
	BindObjectToThread();
	CallMethod<void>(SetTextMethod, GetJString(Text));
}

void FJavaAndroidMessageBox::AddButton(const FString & Text)
{
	BindObjectToThread();
	CallMethod<void>(AddButtonMethod, GetJString(Text));
}

void FJavaAndroidMessageBox::Clear()
{
	BindObjectToThread();
	CallMethod<void>(ClearMethod);
}

int32 FJavaAndroidMessageBox::Show()
{
	BindObjectToThread();
	return CallMethod<int32>(ShowMethod);
}

FName FJavaAndroidMessageBox::GetClassName()
{
	if (FAndroidMisc::GetAndroidBuildVersion() >= 1)
	{
		return FName("com/epicgames/ue4/MessageBox01");
	}
	else
	{
		return FName("");
	}
}

