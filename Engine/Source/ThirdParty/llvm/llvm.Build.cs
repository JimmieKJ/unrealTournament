// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class llvm : ModuleRules
{
	public llvm(TargetInfo Target)
	{
		Type = ModuleType.External;

		if (Target.Platform != UnrealTargetPlatform.Win32)
		{
			// Currently we support only Win32 llvm builds.
			return;
		}

		var LLVMVersion = @"3.5.0";
		// VS2015 uses a newer version of the libs
		if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
		{
			LLVMVersion = @"3.6.2";
		}
		var VSVersion = @"vs" + WindowsPlatform.GetVisualStudioCompilerVersionName();
		var TargetArch = @"x86";
		var RootDirectory = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, @"llvm", LLVMVersion);
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(RootDirectory, "include"),
			});

		PublicLibraryPaths.AddRange(
			new string[] {
				Path.Combine(RootDirectory, @"lib", VSVersion, TargetArch, @"release"),
			});

		PublicAdditionalLibraries.AddRange(
			new string[] {
				"clangAnalysis.lib",
				"clangAST.lib",
				"clangBasic.lib",
				"clangDriver.lib",
				"clangEdit.lib",
				"clangFrontend.lib",
				"clangLex.lib",
				"clangParse.lib",
				"clangSema.lib",
				"clangSerialization.lib",
				"clangTooling.lib",
				"LLVMAnalysis.lib",
				"LLVMBitReader.lib",
				"LLVMCodegen.lib",
				"LLVMCore.lib",
				"LLVMMC.lib",
				"LLVMMCDisassembler.lib",
				"LLVMMCParser.lib",
				"LLVMObject.lib",
				"LLVMOption.lib",
				"LLVMSupport.lib",
				"LLVMTarget.lib",
				"LLVMX86AsmParser.lib",
				"LLVMX86AsmPrinter.lib",
				"LLVMX86CodeGen.lib",
				"LLVMX86Desc.lib",
				"LLVMX86Info.lib",
				"LLVMX86Utils.lib",
			});

		// The 3.6.2 version we use for VS2015 has moved some functionality around.
		if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
		{
			PublicAdditionalLibraries.AddRange(
				new string[] {
					"LLVMTransformUtils.lib",
				});
		}
	}
}
