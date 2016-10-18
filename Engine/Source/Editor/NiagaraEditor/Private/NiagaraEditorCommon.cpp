// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"

#include "ActorFactoryNiagara.h"
#include "NiagaraActor.h"

#define LOCTEXT_NAMESPACE "NiagaraEditor"

DEFINE_LOG_CATEGORY(LogNiagaraEditor);

#define NiagaraOp(OpName) const FName FNiagaraOpInfo::OpName = FName(TEXT(#OpName));
NiagaraOpList
#undef NiagaraOp

void FNiagaraOpInfo::Init()
{
	//Common input and output names.
	FName Result(TEXT("Result"));	FText ResultText = NSLOCTEXT("NiagaraOpInfo", "Operation Result", "Result");
	FName A(TEXT("A"));	FText AText = NSLOCTEXT("NiagaraOpInfo", "First Function Param", "A");
	FName B(TEXT("B"));	FText BText = NSLOCTEXT("NiagaraOpInfo", "Second Function Param", "B");
	FName C(TEXT("C"));	FText CText = NSLOCTEXT("NiagaraOpInfo", "Third Function Param", "C");
	FName D(TEXT("D"));	FText DText = NSLOCTEXT("NiagaraOpInfo", "Fourth Function Param", "D");
	FName X(TEXT("X"));	FText XText = NSLOCTEXT("NiagaraOpInfo", "First Vector Component", "X");
	FName Y(TEXT("Y"));	FText YText = NSLOCTEXT("NiagaraOpInfo", "Second Vector Component", "Y");
	FName Z(TEXT("Z"));	FText ZText = NSLOCTEXT("NiagaraOpInfo", "Third Vector Component", "Z");
	FName W(TEXT("W"));	FText WText = NSLOCTEXT("NiagaraOpInfo", "Fourth Vector Component", "W");

	FName Selection(TEXT("Selection"));	FText SelectionText = NSLOCTEXT("NiagaraOpInfo", "Selection", "Selection");
	FName Mask(TEXT("Mask"));	FText MaskText = NSLOCTEXT("NiagaraOpInfo", "Mask", "Mask");
	
	FText MinText = NSLOCTEXT("NiagaraOpInfo", "Min", "Min");
	FText MaxText = NSLOCTEXT("NiagaraOpInfo", "Max", "Max");

	TMap<FName, FNiagaraOpInfo>& OpInfos = GetOpInfoMap();

	FString DefaultStr_Zero(TEXT("0.0"));
	FString DefaultStr_One(TEXT("1.0"));
	FString DefaultStr_VecZero(TEXT("0.0,0.0,0.0,0.0"));
	FString DefaultStr_VecOne(TEXT("1.0,1.0,1.0,1.0"));

	FNiagaraOpInfo* Op = &OpInfos.Add(Add);
	Op->Name = Add;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Add Name", "Add");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Add Desc", "Result = A + B");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Add);

	Op = &OpInfos.Add(Subtract);
	Op->Name = Subtract;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Subtract Name", "Subtract");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Subtract Desc", "Result = A - B");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Subtract);

	Op = &OpInfos.Add(Multiply);
	Op->Name = Multiply;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Multiply Name", "Multiply");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Multiply Desc", "Result = A * B = {{A.x * B.x, A.y * B.y, A.z * B.z, A.w * B.w}}");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Multiply);

	Op = &OpInfos.Add(Divide);
	Op->Name = Divide;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Divide Name", "Divide");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Divide Desc", "Result = A / B = {{A.x / B.x, A.y / B.y, A.z / B.z, A.w / B.w}}");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Divide);

	Op = &OpInfos.Add(MultiplyAdd);
	Op->Name = MultiplyAdd;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "MultiplyAdd Name", "MultiplyAdd");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "MultiplyAdd Desc", "Result = (A * B) + C");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(C, ENiagaraDataType::Vector, CText, CText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::MultiplyAdd);

	Op = &OpInfos.Add(Lerp);
	Op->Name = Lerp;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Lerp Name", "Lerp");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Lerp Desc", "Result = (A * (1 - C)) + (B * C)");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(C, ENiagaraDataType::Vector, CText, CText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Lerp);

	Op = &OpInfos.Add(Reciprocal);
	Op->Name = Reciprocal;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Reciprocal Name", "Reciprocal");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Reciprocal Desc", "Result = 1 / A");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Reciprocal);

	Op = &OpInfos.Add(ReciprocalSqrt);
	Op->Name = ReciprocalSqrt;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ReciprocalSqrt Name", "ReciprocalSqrt");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ReciprocalSqrt Desc", "Result = 1 / Sqrt(A)");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ReciprocalSqrt);

	Op = &OpInfos.Add(Sqrt);
	Op->Name = Sqrt;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Sqrt Name", "Sqrt");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Sqrt Desc", "Result = A ^ 0.5");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Sqrt);

	Op = &OpInfos.Add(Negate);
	Op->Name = Negate;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Negate Name", "Negate");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Negate Desc", "Result = 1 - A");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Negate);

	Op = &OpInfos.Add(Abs);
	Op->Name = Abs;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Abs Name", "Abs");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Abs Desc", "Result = |A|");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Abs);

	Op = &OpInfos.Add(Exp);
	Op->Name = Exp;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Exp Name", "Exp");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Exp Desc", "Exp");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Exp);

	Op = &OpInfos.Add(Exp2);
	Op->Name = Exp2;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Exp2 Name", "Exp2");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Exp2 Desc", "Exp2");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Exp2);

	Op = &OpInfos.Add(Log);
	Op->Name = Log;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Log Name", "Log");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Log Desc", "Log");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Log);

	Op = &OpInfos.Add(LogBase2);
	Op->Name = LogBase2;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "LogBase2 Name", "LogBase2");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "LogBase2 Desc", "LogBase2");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::LogBase2);

	Op = &OpInfos.Add(Sin);
	Op->Name = Sin;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Sin Name", "Sin");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Sin Desc", "Sin");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Sin);

	Op = &OpInfos.Add(Cos);
	Op->Name = Cos;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Cos Name", "Cos");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Cos Desc", "Cos");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Cos);

	Op = &OpInfos.Add(Tan);
	Op->Name = Tan;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Tan Name", "Tan");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Tan Desc", "Tan");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Tan);

	Op = &OpInfos.Add(ASin);
	Op->Name = ASin;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ASin Name", "ASin");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ASin Desc", "ASin");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ASin);

	Op = &OpInfos.Add(ACos);
	Op->Name = ACos;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ACos Name", "ACos");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ACos Desc", "ACos");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ACos);

	Op = &OpInfos.Add(ATan);
	Op->Name = ATan;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ATan Name", "ATan");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ATan Desc", "ATan");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ATan);

	Op = &OpInfos.Add(ATan2);
	Op->Name = ATan2;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ATan2 Name", "ATan2");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ATan2 Desc", "ATan2");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecZero));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecZero));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ATan2);

	Op = &OpInfos.Add(Ceil);
	Op->Name = Ceil;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Ceil Name", "Ceil");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Ceil Desc", "Ceil");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Ceil);

	Op = &OpInfos.Add(Floor);
	Op->Name = Floor;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Floor Name", "Floor");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Floor Desc", "Floor");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Floor);

	Op = &OpInfos.Add(Modulo);
	Op->Name = Modulo;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Modulo Name", "Modulo");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Modulo Desc", "Modulo");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Modulo);

	Op = &OpInfos.Add(Fractional);
	Op->Name = Fractional;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Fractional Name", "Fractional");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Fractional Desc", "Fractional");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Fractional);

	Op = &OpInfos.Add(Truncate);
	Op->Name = Truncate;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Truncate Name", "Truncate");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Truncate Desc", "Truncate");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Truncate);

	Op = &OpInfos.Add(Clamp);
	Op->Name = Clamp;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Clamp Name", "Clamp");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Clamp Desc", "Clamp");
	Op->Inputs.Add(FNiagaraOpInOutInfo(X, ENiagaraDataType::Vector, XText, XText, DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(Min, ENiagaraDataType::Vector, MinText, MinText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(Max, ENiagaraDataType::Vector, MaxText, MaxText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Clamp);

	Op = &OpInfos.Add(Min);
	Op->Name = Min;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Min Name", "Min");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Min Desc", "Min");
	Op->Inputs.Add(FNiagaraOpInOutInfo(X, ENiagaraDataType::Vector, XText, XText, DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(Min, ENiagaraDataType::Vector, MinText, MinText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Min);

	Op = &OpInfos.Add(Max);
	Op->Name = Max;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Max Name", "Max");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Max Desc", "Max");
	Op->Inputs.Add(FNiagaraOpInOutInfo(X, ENiagaraDataType::Vector, XText, XText, DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(Max, ENiagaraDataType::Vector, MaxText, MaxText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Max);

	Op = &OpInfos.Add(Pow);
	Op->Name = Pow;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Pow Name", "Pow");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Pow Desc", "Pow");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Pow);

	Op = &OpInfos.Add(Sign);
	Op->Name = Sign;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Sign Name", "Sign");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Sign Desc", "Sign");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Sign);

	Op = &OpInfos.Add(Step);
	Op->Name = Step;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Step Name", "Step");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Step Desc", "Step");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Step);

	Op = &OpInfos.Add(Dot);
	Op->Name = Dot;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Dot Name", "Dot");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Dot Desc", "Dot");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Dot);

	Op = &OpInfos.Add(Cross);
	Op->Name = Cross;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Cross Name", "Cross");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Cross Desc", "Cross");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Cross);

	Op = &OpInfos.Add(Normalize);
	Op->Name = Normalize;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Normalize Name", "Normalize");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Normalize Desc", "Normalize");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Normalize);

	Op = &OpInfos.Add(Random);
	Op->Name = Random;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Random Name", "Random");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Random Desc", "Random");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Random);

	Op = &OpInfos.Add(Length);
	Op->Name = Length;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Length Name", "Length");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Length Desc", "Length");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Length);

	Op = &OpInfos.Add(Noise);
	Op->Name = Noise;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Noise Name", "Noise");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Noise Desc", "Noise");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Noise);

	Op = &OpInfos.Add(Scalar);
	Op->Name = Scalar;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Scalar Name", "Scalar");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Scalar Desc", "Result is a vector with X in each of it's components.");
	Op->Inputs.Add(FNiagaraOpInOutInfo(X, ENiagaraDataType::Scalar, XText, XText, DefaultStr_One));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Scalar);

	Op = &OpInfos.Add(Vector);
	Op->Name = Vector;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Vector Name", "Vector");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Vector Desc", "Builds a constant vector.");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Vector);

	Op = &OpInfos.Add(Matrix);
	FName Row0(TEXT("Row0"));	FText Row0Text = NSLOCTEXT("NiagaraOpInfo", "First Matrix Row", "Row 0");
	FName Row1(TEXT("Row1"));	FText Row1Text = NSLOCTEXT("NiagaraOpInfo", "Second Matrix Row", "Row 1");
	FName Row2(TEXT("Row2"));	FText Row2Text = NSLOCTEXT("NiagaraOpInfo", "Third Matrix Row", "Row 2");
	FName Row3(TEXT("Row3"));	FText Row3Text = NSLOCTEXT("NiagaraOpInfo", "Fourth Matrix Row", "Row 3");
	FString Identity = TEXT("	1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0");
	Op->Name = Matrix;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Matrix Name", "Matrix");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Matrix Desc", "Matrix");
	Op->Inputs.Add(FNiagaraOpInOutInfo(Row0, ENiagaraDataType::Vector, Row0Text, Row0Text, TEXT("1.0,0.0,0.0,0.0")));
	Op->Inputs.Add(FNiagaraOpInOutInfo(Row1, ENiagaraDataType::Vector, Row1Text, Row1Text, TEXT("0.0,1.0,0.0,0.0")));
	Op->Inputs.Add(FNiagaraOpInOutInfo(Row2, ENiagaraDataType::Vector, Row2Text, Row2Text, TEXT("0.0,0.0,1.0,0.0")));
	Op->Inputs.Add(FNiagaraOpInOutInfo(Row3, ENiagaraDataType::Vector, Row3Text, Row3Text, TEXT("0.0,0.0,0.0,1.0")));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Matrix, ResultText, ResultText, Identity));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Matrix);

	Op = &OpInfos.Add(Compose);
	Op->Name = Compose;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Compose Name", "Compose Vector XYZW");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Compose Desc", "Result = {{ A.x, B.y, C.z, D.w }}");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(C, ENiagaraDataType::Vector, CText, CText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(D, ENiagaraDataType::Vector, DText, DText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Compose);

	Op = &OpInfos.Add(ComposeX);
	Op->Name = ComposeX;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ComposeX Name", "Compose Vector X");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ComposeX Desc", "Result = {{ A.x, B.x, C.x, D.x }}");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(C, ENiagaraDataType::Vector, CText, CText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(D, ENiagaraDataType::Vector, DText, DText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ComposeX);

	Op = &OpInfos.Add(ComposeY);
	Op->Name = ComposeY;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ComposeY Name", "Compose Vector Y");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ComposeY Desc", "Result = {{ A.y, B.y, C.y, D.y }}");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(C, ENiagaraDataType::Vector, CText, CText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(D, ENiagaraDataType::Vector, DText, DText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ComposeY);

	Op = &OpInfos.Add(ComposeZ);
	Op->Name = ComposeZ;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ComposeZ Name", "Compose Vector Z");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ComposeZ Desc", "Result = {{ A.z, B.z, C.z, D.z }}");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(C, ENiagaraDataType::Vector, CText, CText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(D, ENiagaraDataType::Vector, DText, DText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ComposeZ);

	Op = &OpInfos.Add(ComposeW);
	Op->Name = ComposeW;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "ComposeW Name", "Compose Vector W");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "ComposeW Desc", "Result = {{ A.w, B.w, C.w, D.w }}");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(C, ENiagaraDataType::Vector, CText, CText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(D, ENiagaraDataType::Vector, DText, DText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::ComposeW);

	Op = &OpInfos.Add(Split);
	Op->Name = Split;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Split Name", "Split");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Split Desc", "X = A.xxxx, Y = A.yyyy, Z = A.zzzz, W = A.wwww");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(X, ENiagaraDataType::Vector, XText, XText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Y, ENiagaraDataType::Vector, YText, YText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Z, ENiagaraDataType::Vector, ZText, ZText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(W, ENiagaraDataType::Vector, WText, WText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Split);

	Op = &OpInfos.Add(GetX);
	Op->Name = GetX;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "GetX Name", "GetX");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "GetX Desc", "X = A.xxxx");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(X, ENiagaraDataType::Vector, XText, XText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::GetX);

	Op = &OpInfos.Add(GetY);
	Op->Name = GetY;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "GetY Name", "GetY");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "GetY Desc", "Y = A.yyyy");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Y, ENiagaraDataType::Vector, YText, YText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::GetY);

	Op = &OpInfos.Add(GetZ);
	Op->Name = GetZ;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "GetZ Name", "GetZ");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "GetZ Desc", "Z = A.zzzz");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Z, ENiagaraDataType::Vector, ZText, ZText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::GetZ);

	Op = &OpInfos.Add(GetW);
	Op->Name = GetW;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "GetW Name", "GetW");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "GetW Desc", "W = A.wwww");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(W, ENiagaraDataType::Vector, WText, WText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::GetW);

	Op = &OpInfos.Add(TransformVector);
	Op->Name = TransformVector;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "TransformVector Name", "Transform Vector");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "TransformVector Desc", "Transforms vector B by matrix A");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Matrix, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::TransformVector);

	Op = &OpInfos.Add(Transpose);
	Op->Name = Transpose;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Transpose Name", "Transpose");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Transpose Desc", "Result = Transpose of matrix A");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Matrix, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Matrix, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Transpose);

	Op = &OpInfos.Add(Inverse);
	Op->Name = Inverse;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Inverse Name", "Inverse");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Inverse Desc", "Result = Inverse of matrix A");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Matrix, AText, AText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Matrix, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Inverse);

	Op = &OpInfos.Add(LessThan);
	Op->Name = LessThan;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "LessThan Name", "A Less Than B");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "LessThan Desc", "Result = 1 if A<B, 0 otherwise");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::LessThan);

	Op = &OpInfos.Add(GreaterThan);
	Op->Name = GreaterThan;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "GreaterThan Name", "A Greater Than B");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "GreaterThan Desc", "Result = 1 if A>B, 0 otherwise");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::GreaterThan);

	Op = &OpInfos.Add(Select);
	Op->Name = Select;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "Select Name", "Select");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "Select Desc", "Selects A or B depending on Selection. Selection > 0.0f = A, otherwise B.");
	Op->Inputs.Add(FNiagaraOpInOutInfo(Selection, ENiagaraDataType::Vector, SelectionText, SelectionText, DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, AText, AText, DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(B, ENiagaraDataType::Vector, BText, BText, DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::Select);

	Op = &OpInfos.Add(EaseIn);
	Op->Name = EaseIn;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "EaseIn Name", "Ease In 0->1");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "EaseIn Desc", "Ease in for Time; A and B determine the ease length in [0:1]");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, NSLOCTEXT("NiagaraOpInfo", "A", "A"), NSLOCTEXT("NiagaraOpInfo", "A", "A"), DefaultStr_VecZero));
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, NSLOCTEXT("NiagaraOpInfo", "B", "B"), NSLOCTEXT("NiagaraOpInfo", "B", "B"), DefaultStr_VecOne));
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, NSLOCTEXT("NiagaraOpInfo", "Time", "Time"), NSLOCTEXT("NiagaraOpInfo", "Time", "Time"), DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::EaseIn);

	Op = &OpInfos.Add(EaseInOut);
	Op->Name = EaseInOut;
	Op->FriendlyName = NSLOCTEXT("NiagaraOpInfo", "EaseInOut Name", "Ease In/Out 0->1->0");
	Op->Description = NSLOCTEXT("NiagaraOpInfo", "EaseInOut Desc", "Ease in/out for Time; A and B determine the ease in/out lengths in [0:1]");
	Op->Inputs.Add(FNiagaraOpInOutInfo(A, ENiagaraDataType::Vector, NSLOCTEXT("NiagaraOpInfo", "Time", "Time"), NSLOCTEXT("NiagaraOpInfo", "Time", "Time"), DefaultStr_VecOne));
	Op->Outputs.Add(FNiagaraOpInOutInfo(Result, ENiagaraDataType::Vector, ResultText, ResultText, DefaultStr_VecOne));
	Op->OpDelegate.BindStatic(&INiagaraCompiler::EaseInOut);


	#define NiagaraOp(OpName) if (GetOpInfo(OpName) == NULL)\
	{\
	UE_LOG(LogNiagaraEditor, Fatal, TEXT("Couldn't find info for \"%s\". Have you added it to the NiagaraOpList but not provided the extra information in FNiagaraOpInfo::Init() ?"), TEXT(#OpName)); \
	}
	NiagaraOpList;
#undef NiagaraOp

#define NiagaraOp(OpName) (OpName == OpInfo.Key) || 

	for (auto& OpInfo : OpInfos)
	{
		if (!(
			NiagaraOpList
			false)
			)
		{
			UE_LOG(LogNiagaraEditor, Fatal, TEXT("Found info about \"%s\" in FNiagaraOpInfo::Init() that did not have an entry in the NiagaraOpList. You must add it to both."), *OpInfo.Key.ToString());
		}
	}
#undef NiagaraOp
}

const FNiagaraOpInfo* FNiagaraOpInfo::GetOpInfo(FName Name)
{
	if (GetOpInfoMap().Num() == 0)
	{
		Init();
	}
	return GetOpInfoMap().Find(Name);
}

TMap<FName, FNiagaraOpInfo>& FNiagaraOpInfo::GetOpInfoMap()
{
	static TMap<FName, FNiagaraOpInfo> OpInfos;

	return OpInfos;
}

//////////////////////////////////////////////////////////////////////////

//Debugging variant that checks inputs and outputs match expectations of FNiagaraOpInfo.
#if DO_CHECK
#define NiagaraOp(OpName) bool INiagaraCompiler::OpName(INiagaraCompiler* Compiler, TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)\
{\
	Compiler->CheckInputs(FNiagaraOpInfo::OpName, InputExpressions); \
	return Compiler->OpName##_Internal(InputExpressions, OutputExpressions); \
	Compiler->CheckOutputs(FNiagaraOpInfo::OpName, OutputExpressions); \
}
NiagaraOpList
#undef NiagaraOp

#else//DO_CHECK

#define NiagaraOp(OpName) bool INiagaraCompiler::OpName(INiagaraCompiler* Compiler, TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)\
{ \
	return Compiler->OpName##_Internal(InputExpressions, OutputExpressions); \
}
NiagaraOpList
#undef NiagaraOp

#endif

//////////////////////////////////////////////////////////////////////////


/*-----------------------------------------------------------------------------
UActorFactoryNiagara
-----------------------------------------------------------------------------*/
UActorFactoryNiagara::UActorFactoryNiagara(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("NiagaraEffectDisplayName", "NiagaraEffect");
	NewActorClass = ANiagaraActor::StaticClass();
}

bool UActorFactoryNiagara::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UNiagaraEffect::StaticClass()))
	{
		OutErrorMsg = NSLOCTEXT("CanCreateActor", "NoEffect", "A valid Niagara effect must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryNiagara::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UNiagaraEffect* Effect = CastChecked<UNiagaraEffect>(Asset);
	ANiagaraActor* NiagaraActor = CastChecked<ANiagaraActor>(NewActor);

	// Term Component
	NiagaraActor->GetNiagaraComponent()->UnregisterComponent();

	// Change properties
	NiagaraActor->GetNiagaraComponent()->SetAsset(Effect);

	// if we're created by Kismet on the server during gameplay, we need to replicate the emitter
	if (NiagaraActor->GetWorld()->HasBegunPlay() && NiagaraActor->GetWorld()->GetNetMode() != NM_Client)
	{
		NiagaraActor->SetReplicates(true);
		NiagaraActor->bAlwaysRelevant = true;
		NiagaraActor->NetUpdateFrequency = 0.1f; // could also set bNetTemporary but LD might further trigger it or something
	}

	// Init Component
	NiagaraActor->GetNiagaraComponent()->RegisterComponent();
}

UObject* UActorFactoryNiagara::GetAssetFromActorInstance(AActor* Instance)
{
	check(Instance->IsA(NewActorClass));
	ANiagaraActor* NewActor = CastChecked<ANiagaraActor>(Instance);
	if (NewActor->GetNiagaraComponent())
	{
		return NewActor->GetNiagaraComponent()->GetAsset();
	}
	else
	{
		return NULL;
	}
}

void UActorFactoryNiagara::PostCreateBlueprint(UObject* Asset, AActor* CDO)
{
	if (Asset != NULL && CDO != NULL)
	{
		UNiagaraEffect* Effect = CastChecked<UNiagaraEffect>(Asset);
		ANiagaraActor* Actor = CastChecked<ANiagaraActor>(CDO);
		Actor->GetNiagaraComponent()->SetAsset(Effect);
	}
}

#undef LOCTEXT_NAMESPACE