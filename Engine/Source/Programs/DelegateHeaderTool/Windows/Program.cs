// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;


/**
 * The purpose of this application is simply to automatically generate the contents of DelegateCombinations.h,
 * which is part of our C++ delegate system in Unreal.
 */

namespace DelegateHeaderTool
{
	class Generator
	{
		/// <summary>
		/// Static design constraints for this app
		/// </summary>
		static class Config
		{
			/// Maximum number of parameters that can be passed when executing a delegate function, not including payload
			/// values.  Increasing this will cause more delegate type combinations to be generated.
			public static int MaxCallParameters = 8;

			/// Maximum number of stored payload members we'll support.  Increasing this will cause more delegate instance
			/// classes to be generated.  If this number is changed, various wrapper methods and macros in DelegateSignatureImpl.inl
			/// will need to be added or removed!
			public static int MaxPayloadMembers = 4;

			/// Whether to generate delegates that support a return value.  These delegates won't have
			/// any multi-cast support.
			public static bool bGenerateRetValDelegates = true;

			/// Whether go generate delegates that require 'const' methods
			public static bool bGenerateConstDelegates = true;

			/// Whether multi-cast delegates should be generated at all
			public static bool bGenerateMulticastDelegates = true;

			/// Whether dynamic delegates should be generated at all
			public static bool bGenerateDynamicDelegates = true;
		}


		/// <summary>
		/// Generates all delegates
		/// </summary>
		/// <param name="Output">The stream we're writing text to</param>
		public void GenerateAllDelegates( TextWriter Output )
		{
			// Call parameters
			for( var CallParameterCount = 0; CallParameterCount <= Config.MaxCallParameters; ++CallParameterCount )
			{
				// RetVal
				for( var RetValIter = 0; RetValIter <= 1; ++RetValIter )
				{
					bool bSupportsRetVal = RetValIter == 0 ? false : true;
					if( bSupportsRetVal && !Config.bGenerateRetValDelegates )
					{
						// Retval delegates not enabled
						continue;
					}

					GenerateDelegate(
						Output: Output,
						CallParameterCount: CallParameterCount,
						bSupportsRetVal: bSupportsRetVal );
				}
			}
		}


		/// <summary>
		/// Given a positive integer number less than 20, creates a non-digit string representation of that number
		/// </summary>
		/// <param name="Number">The number to convert to a string</param>
		/// <returns></returns>
		private static string MakeFriendlyNumber( int Number )
		{
			string[] FriendlyOnes = new string[] { "", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine" };
			string[] FriendlyTeens = new string[] { "Ten", "Eleven", "Twelve", "Thirteen", "Fourteen", "Fifteen", "Sixteen", "Seventeen", "Eighteen", "Nineteen" };

			if( Number == 0 )
			{
				return "Zero";
			}
			else if( Number < 10 )
			{
				return FriendlyOnes[ Number ];
			}
			else if( Number < 20 )
			{
				return FriendlyTeens[ Number - 10 ];
			}
			else
			{
				throw new ArgumentOutOfRangeException( "Number" );
			}
		}


		/// <summary>
		/// Generates code to define a single delegate signature combination
		/// </summary>
		/// <param name="Output">Stream to write the code text to</param>
		/// <param name="CallParameterCount">Number of function parameters</param>
		/// <param name="bSupportsRetVal">True if the delegate function will return a value</param>
		void GenerateDelegate( TextWriter Output, int CallParameterCount, bool bSupportsRetVal )
		{
			// For each payload count, generate a new set of delegate instance classes
			int MaxPayloadMembers = Config.MaxPayloadMembers;

			Output.WriteLine( "" );

			var DefineFuncSuffix = new StringBuilder( "#define FUNC_SUFFIX " );
            var DefineRetvalTypedef = new StringBuilder( "#define FUNC_RETVAL_TYPEDEF " );
			var DefineTemplateDecl = new StringBuilder( "#define FUNC_TEMPLATE_DECL " );
			var DefineTemplateDeclTypename = new StringBuilder( "#define FUNC_TEMPLATE_DECL_TYPENAME " );
			var DefineTemplateDeclNoShadow = new StringBuilder( "#define FUNC_TEMPLATE_DECL_NO_SHADOW " );
			var DefineTemplateArgs = new StringBuilder( "#define FUNC_TEMPLATE_ARGS " );
			var DefineHasParams = String.Format( "#define FUNC_HAS_PARAMS {0}", CallParameterCount > 0 ? 1 : 0 );
			var DefineParamList = new StringBuilder( "#define FUNC_PARAM_LIST" );
			var DefineParamMembers = new StringBuilder( "#define FUNC_PARAM_MEMBERS" );
			var DefineParamPassThru = new StringBuilder( "#define FUNC_PARAM_PASSTHRU" );
			var DefineParamParmsPassIn = new StringBuilder( "#define FUNC_PARAM_PARMS_PASSIN" );
			var DefineParamInitializerList = new StringBuilder( "#define FUNC_PARAM_INITIALIZER_LIST" );
			var DefineIsVoid = String.Format( "#define FUNC_IS_VOID {0}", bSupportsRetVal ? 0 : 1 );
			var DefineDeclareDelegate = new StringBuilder( "#define DECLARE_DELEGATE" );
            var DefineDeclareMulticastDelegate = new StringBuilder("#define DECLARE_MULTICAST_DELEGATE");
            var DefineDeclareEvent = new StringBuilder("#define DECLARE_EVENT");
			var DefineDeclareDynamicDelegate = new StringBuilder( "#define DECLARE_DYNAMIC_DELEGATE" );
			var DefineDeclareDynamicMulticastDelegate = new StringBuilder( "#define DECLARE_DYNAMIC_MULTICAST_DELEGATE" );

			var DynamicDelegateDefaultPtr = "FWeakObjectPtr";


			// Return type, always specified here even for void functions
			var TemplateDecl = new StringBuilder( "RetValType" );
			var TemplateDeclTypename = new StringBuilder( "typename RetValType" );
			var TemplateDeclNoShadow = new StringBuilder( "typename RetValTypeNoShadow" );
			var TemplateArgs = new StringBuilder( "RetValType" );

			var FuncName = new StringBuilder();
			var FuncSuffix = new StringBuilder();
			{
				if( bSupportsRetVal )
				{
					FuncName.Append( "RetVal" );
					FuncSuffix.Append( "RetVal" );
				}

				if( CallParameterCount == 0 )
				{
					if( !String.IsNullOrEmpty( FuncName.ToString() ) )
					{
						FuncSuffix.Append( "_" );
					}
					FuncSuffix.Append( "NoParams" );
				}
				else
				{
					if( !String.IsNullOrEmpty( FuncName.ToString() ) )
					{
						FuncName.Append( "_" );
						FuncSuffix.Append( "_" );
					}
					var ParametersName = String.Format( "{0}Param{1}", MakeFriendlyNumber( CallParameterCount ), CallParameterCount > 1 ? "s" : "" );
					FuncName.Append( ParametersName );
					FuncSuffix.AppendFormat( ParametersName );
				}
			}

			// Add the function suffix to the delegate macro
			DefineFuncSuffix.Append( FuncSuffix );

			if( !String.IsNullOrEmpty( FuncName.ToString() ) )
			{
				DefineDeclareDelegate.Append( "_" + FuncName );
                DefineDeclareMulticastDelegate.Append("_" + FuncName);
                DefineDeclareEvent.Append("_" + FuncName);
				DefineDeclareDynamicDelegate.Append( "_" + FuncName );
				DefineDeclareDynamicMulticastDelegate.Append( "_" + FuncName );
			}
			DefineDeclareDelegate.Append( "( " );
            DefineDeclareMulticastDelegate.Append("( ");
            DefineDeclareEvent.Append("( ");
			DefineDeclareDynamicDelegate.Append( "( " );
			DefineDeclareDynamicMulticastDelegate.Append( "( " );

			if( bSupportsRetVal )
			{
				DefineDeclareDelegate.Append( "RetValType, " );
				DefineDeclareDynamicDelegate.Append( "RetValType, " );
			}
			DefineDeclareDelegate.Append( "DelegateName" );
            DefineDeclareMulticastDelegate.Append("DelegateName");
            DefineDeclareEvent.Append("OwningType, EventName");
			DefineDeclareDynamicDelegate.Append( "DelegateName" );
			DefineDeclareDynamicMulticastDelegate.Append( "DelegateName" );


			// Parameters
			for( var CallParameterIndex = 1; CallParameterIndex <= CallParameterCount; ++CallParameterIndex )
			{
				TemplateDecl.AppendFormat( ", Param{0}Type", CallParameterIndex );
				TemplateDeclTypename.AppendFormat( ", typename Param{0}Type", CallParameterIndex );
				TemplateDeclNoShadow.AppendFormat( ", typename Param{0}TypeNoShadow", CallParameterIndex );
				TemplateArgs.AppendFormat( ", Param{0}Type", CallParameterIndex );

				if( CallParameterIndex > 1 )
				{
					DefineParamList.Append( "," );
					DefineParamPassThru.Append( "," );
					DefineParamParmsPassIn.Append( "," );
					DefineParamInitializerList.Append(",");
				}
				DefineParamList.AppendFormat( " Param{0}Type InParam{0}", CallParameterIndex );
				DefineParamMembers.AppendFormat(" Param{0}Type Param{0};", CallParameterIndex);
				DefineParamPassThru.AppendFormat( " InParam{0}", CallParameterIndex );
				DefineParamParmsPassIn.AppendFormat(" Parms.Param{0}", CallParameterIndex);
				DefineParamInitializerList.AppendFormat(" Param{0}( InParam{0} )", CallParameterIndex);

				DefineDeclareDelegate.AppendFormat( ", Param{0}Type", CallParameterIndex );
                DefineDeclareMulticastDelegate.AppendFormat(", Param{0}Type", CallParameterIndex);
                DefineDeclareEvent.AppendFormat(", Param{0}Type", CallParameterIndex);
				DefineDeclareDynamicDelegate.AppendFormat( ", Param{0}Type", CallParameterIndex );
				DefineDeclareDynamicMulticastDelegate.AppendFormat( ", Param{0}Type", CallParameterIndex );

				DefineDeclareDynamicDelegate.AppendFormat( ", Param{0}Name", CallParameterIndex );
				DefineDeclareDynamicMulticastDelegate.AppendFormat( ", Param{0}Name", CallParameterIndex );
            }

			DefineDeclareDelegate.AppendFormat( " ) FUNC_DECLARE_DELEGATE( {0}, DelegateName", FuncSuffix );
            DefineDeclareMulticastDelegate.AppendFormat(" ) FUNC_DECLARE_MULTICAST_DELEGATE( {0}, DelegateName", FuncSuffix);
            DefineDeclareEvent.AppendFormat(" ) FUNC_DECLARE_EVENT( OwningType, EventName, {0}", FuncSuffix);
			if( bSupportsRetVal )
			{
				DefineDeclareDynamicDelegate.AppendFormat(" ) BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_DELEGATE) FUNC_DECLARE_DYNAMIC_DELEGATE_RETVAL( {0}, {1}, DelegateName, DelegateName##_DelegateWrapper", DynamicDelegateDefaultPtr, FuncSuffix);
			}
			else
			{
				DefineDeclareDynamicDelegate.AppendFormat(" ) BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_DELEGATE) FUNC_DECLARE_DYNAMIC_DELEGATE( {0}, {1}, DelegateName, DelegateName##_DelegateWrapper", DynamicDelegateDefaultPtr, FuncSuffix);
			}
			DefineDeclareDynamicMulticastDelegate.AppendFormat(" ) BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_DELEGATE) FUNC_DECLARE_DYNAMIC_MULTICAST_DELEGATE( {0}, {1}, DelegateName, DelegateName##_DelegateWrapper", DynamicDelegateDefaultPtr, FuncSuffix);

			if( bSupportsRetVal )
			{
				DefineDeclareDynamicDelegate.AppendFormat( ", RetValType" );
			}

			if( CallParameterCount == 0 )
			{
				DefineDeclareDynamicDelegate.Append( ", " );
				DefineDeclareDynamicMulticastDelegate.Append( ", " );
			}
			else
			{
				DefineDeclareDynamicDelegate.Append( ", FUNC_CONCAT( " );
				DefineDeclareDynamicMulticastDelegate.Append( ", FUNC_CONCAT( " );
				for( var CallParameterIndex = 1; CallParameterIndex <= CallParameterCount; ++CallParameterIndex )
				{
					if( CallParameterIndex > 1 )
					{
						DefineDeclareDynamicDelegate.Append( ", " );
						DefineDeclareDynamicMulticastDelegate.Append( ", " );
					}
					DefineDeclareDynamicDelegate.AppendFormat( "Param{0}Type InParam{0}", CallParameterIndex );
					DefineDeclareDynamicMulticastDelegate.AppendFormat( "Param{0}Type InParam{0}", CallParameterIndex );
				}
				DefineDeclareDynamicDelegate.Append( " )" );
				DefineDeclareDynamicMulticastDelegate.Append( " )" );
			}

			{
				DefineDeclareDynamicDelegate.Append( ", FUNC_CONCAT( *this" );
				DefineDeclareDynamicMulticastDelegate.Append( ", FUNC_CONCAT( *this" );
				for( var CallParameterIndex = 1; CallParameterIndex <= CallParameterCount; ++CallParameterIndex )
				{
					DefineDeclareDynamicDelegate.AppendFormat( ", InParam{0}", CallParameterIndex );
					DefineDeclareDynamicMulticastDelegate.AppendFormat( ", InParam{0}", CallParameterIndex );
				}
				DefineDeclareDynamicDelegate.Append( " )" );
				DefineDeclareDynamicMulticastDelegate.Append( " )" );
			}
				

			if( bSupportsRetVal )
			{
				DefineDeclareDelegate.Append( ", RetValType" );
				DefineDeclareDynamicDelegate.Append( ", RetValType" );
                DefineRetvalTypedef.Append( " typedef RetValType RetValType;" );
			}
			else
			{
				DefineDeclareDelegate.Append( ", void" );
                DefineDeclareMulticastDelegate.Append(", void");
                DefineDeclareEvent.Append(", void");
				DefineDeclareDynamicDelegate.Append( ", void" );
				DefineDeclareDynamicMulticastDelegate.Append( ", void" );
			}

			// Parameters
			for( var CallParameterIndex = 1; CallParameterIndex <= CallParameterCount; ++CallParameterIndex )
			{
				DefineDeclareDelegate.AppendFormat( ", Param{0}Type", CallParameterIndex );
                DefineDeclareMulticastDelegate.AppendFormat(", Param{0}Type", CallParameterIndex);
                DefineDeclareEvent.AppendFormat(", Param{0}Type", CallParameterIndex);
				DefineDeclareDynamicDelegate.AppendFormat( ", Param{0}Type", CallParameterIndex );
				DefineDeclareDynamicMulticastDelegate.AppendFormat( ", Param{0}Type", CallParameterIndex );
			}


			DefineDeclareDelegate.Append( " )" );
            DefineDeclareMulticastDelegate.Append(" )");
            DefineDeclareEvent.Append(" )");
			DefineDeclareDynamicDelegate.Append( " )" );
			DefineDeclareDynamicMulticastDelegate.Append( " )" );

			DefineTemplateDecl.Append( TemplateDecl );
			DefineTemplateDeclTypename.Append( TemplateDeclTypename );
			DefineTemplateDeclNoShadow.Append( TemplateDeclNoShadow );
			DefineTemplateArgs.Append( TemplateArgs );

			Output.WriteLine( DefineFuncSuffix );
            Output.WriteLine( DefineRetvalTypedef );
			Output.WriteLine( DefineTemplateDecl );
			Output.WriteLine( DefineTemplateDeclTypename );
            Output.WriteLine( DefineTemplateDeclNoShadow );
			Output.WriteLine( DefineTemplateArgs );
			Output.WriteLine( DefineHasParams );
			Output.WriteLine( DefineParamList );
			Output.WriteLine( DefineParamMembers );
			Output.WriteLine( DefineParamPassThru );
			Output.WriteLine( DefineParamParmsPassIn );
			Output.WriteLine( DefineParamInitializerList );
			Output.WriteLine( DefineIsVoid );
			Output.WriteLine( "#include \"DelegateInstanceInterfaceImpl.inl\"" );


			for( var PayloadMemberCount = 0; PayloadMemberCount <= MaxPayloadMembers; ++PayloadMemberCount )
			{
				Output.WriteLine();
				var DefineHasPayload = String.Format( "#define FUNC_HAS_PAYLOAD {0}", PayloadMemberCount > 0 ? 1 : 0 );
				var DefinePayloadFuncSuffix = new StringBuilder( "#define FUNC_PAYLOAD_SUFFIX " );
				var DefinePayloadTemplateDecl = new StringBuilder("#define FUNC_PAYLOAD_TEMPLATE_DECL ");
				var DefinePayloadTemplateDeclTypename = new StringBuilder("#define FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME ");
				var DefinePayloadTemplateDeclNoShadow = new StringBuilder("#define FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW ");
				var DefinePayloadTemplateArgs = new StringBuilder( "#define FUNC_PAYLOAD_TEMPLATE_ARGS " );
				var DefinePayloadMembers = new StringBuilder( "#define FUNC_PAYLOAD_MEMBERS " );
				var DefinePayloadList = new StringBuilder( "#define FUNC_PAYLOAD_LIST" );
				var DefinePayloadPassThru = new StringBuilder( "#define FUNC_PAYLOAD_PASSTHRU" );
				var DefinePayloadPassIn = new StringBuilder( "#define FUNC_PAYLOAD_PASSIN" );
				var DefinePayloadInitializerList = new StringBuilder( "#define FUNC_PAYLOAD_INITIALIZER_LIST" );
    
				var PayloadFuncSuffix = new StringBuilder( FuncSuffix.ToString() );
				var PayloadTemplateDecl = new StringBuilder( TemplateDecl.ToString() );
				var PayloadTemplateDeclTypename = new StringBuilder( TemplateDeclTypename.ToString() );
				var PayloadTemplateDeclNoShadow = new StringBuilder( TemplateDeclNoShadow.ToString() );
				var PayloadTemplateArgs = new StringBuilder( TemplateArgs.ToString() );
    
				if( PayloadMemberCount > 0 )
				{
					var PayloadsName = String.Format( "{0}Var{1}", MakeFriendlyNumber( PayloadMemberCount ), PayloadMemberCount > 1 ? "s" : "" );
					PayloadFuncSuffix.Append( "_" + PayloadsName );
    
					for( var PayloadMemberIndex = 1; PayloadMemberIndex <= PayloadMemberCount; ++PayloadMemberIndex )
					{
						PayloadTemplateDecl.AppendFormat( ", Var{0}Type", PayloadMemberIndex );
						PayloadTemplateDeclTypename.AppendFormat( ", typename Var{0}Type", PayloadMemberIndex );
						PayloadTemplateDeclNoShadow.AppendFormat( ", typename Var{0}TypeNoShadow", PayloadMemberIndex );
						PayloadTemplateArgs.AppendFormat( ", Var{0}Type", PayloadMemberIndex );
    
						DefinePayloadMembers.AppendFormat( "Var{0}Type Var{0}; ", PayloadMemberIndex );
    
						if( PayloadMemberIndex > 1 )
						{
							DefinePayloadList.Append( "," );
							DefinePayloadPassThru.Append( "," );
							DefinePayloadPassIn.Append( "," );
							DefinePayloadInitializerList.Append( "," );
						}
						DefinePayloadList.AppendFormat( " Var{0}Type InVar{0}", PayloadMemberIndex );
						DefinePayloadPassThru.AppendFormat( " InVar{0}", PayloadMemberIndex );
						DefinePayloadPassIn.AppendFormat( " Var{0}", PayloadMemberIndex );
						DefinePayloadInitializerList.AppendFormat( " Var{0}( InVar{0} )", PayloadMemberIndex );
					}
				}
    
				// Add the payload function suffix to the delegate macro
				DefinePayloadFuncSuffix.Append( PayloadFuncSuffix );
    
				DefinePayloadTemplateDecl.Append( PayloadTemplateDecl );
				DefinePayloadTemplateDeclTypename.Append( PayloadTemplateDeclTypename );
				DefinePayloadTemplateDeclNoShadow.Append( PayloadTemplateDeclNoShadow );
				DefinePayloadTemplateArgs.Append( PayloadTemplateArgs );

				Output.WriteLine( DefineHasPayload );
				Output.WriteLine( DefinePayloadFuncSuffix );
				Output.WriteLine( DefinePayloadTemplateDecl );
				Output.WriteLine( DefinePayloadTemplateDeclTypename );
				Output.WriteLine( DefinePayloadTemplateDeclNoShadow );
				Output.WriteLine( DefinePayloadTemplateArgs );
				Output.WriteLine( DefinePayloadMembers );
				Output.WriteLine( DefinePayloadList );
				Output.WriteLine( DefinePayloadPassThru );
				Output.WriteLine( DefinePayloadPassIn );
				Output.WriteLine( DefinePayloadInitializerList );


				// Const
				for( var ConstIter = 0; ConstIter <= 1; ++ConstIter )
				{
					bool bSupportsConst = ConstIter == 0 ? false : true;
					if( bSupportsConst && !Config.bGenerateConstDelegates )
					{
						// Const delegates not enabled
						continue;
					}

					var DefineIsConst = String.Format( "#define FUNC_IS_CONST {0}", bSupportsConst ? 1 : 0 );
					var DefineFuncConstSuffix = String.Format( "#define FUNC_CONST_SUFFIX {0}", bSupportsConst ? "_Const" : "" );
					
					Output.WriteLine( DefineIsConst );
					Output.WriteLine( DefineFuncConstSuffix );

					Output.WriteLine( "#include \"DelegateInstancesImpl.inl\"" );
					Output.WriteLine( "#undef FUNC_IS_CONST" );
					Output.WriteLine( "#undef FUNC_CONST_SUFFIX" );
				}
    
                Output.WriteLine( "" );
    
                Output.WriteLine( "#undef FUNC_HAS_PAYLOAD" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_SUFFIX" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_TEMPLATE_DECL" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_TEMPLATE_DECL_TYPENAME" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_TEMPLATE_DECL_NO_SHADOW" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_TEMPLATE_ARGS" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_MEMBERS" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_LIST" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_PASSTHRU" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_PASSIN" );
                Output.WriteLine( "#undef FUNC_PAYLOAD_INITIALIZER_LIST" );
			}

			Output.WriteLine();
			Output.WriteLine( "#include \"DelegateSignatureImpl.inl\"" );
			Output.WriteLine();

			Output.WriteLine( DefineDeclareDelegate );
			if( Config.bGenerateMulticastDelegates && !bSupportsRetVal )
			{
				Output.WriteLine( DefineDeclareMulticastDelegate );
			}

            if ( Config.bGenerateMulticastDelegates && !bSupportsRetVal )
            {
                Output.WriteLine( DefineDeclareEvent );
            }

			Output.WriteLine( DefineDeclareDynamicDelegate );
			if( Config.bGenerateMulticastDelegates && !bSupportsRetVal )
			{
				Output.WriteLine( DefineDeclareDynamicMulticastDelegate );
			}

            Output.WriteLine( "#undef FUNC_SUFFIX" );
            Output.WriteLine( "#undef FUNC_TEMPLATE_DECL" );
            Output.WriteLine( "#undef FUNC_TEMPLATE_DECL_TYPENAME" );
            Output.WriteLine( "#undef FUNC_TEMPLATE_DECL_NO_SHADOW" );
            Output.WriteLine( "#undef FUNC_TEMPLATE_ARGS" );
            Output.WriteLine( "#undef FUNC_HAS_PARAMS" );
            Output.WriteLine( "#undef FUNC_PARAM_LIST" );
			Output.WriteLine( "#undef FUNC_PARAM_MEMBERS" );
            Output.WriteLine( "#undef FUNC_PARAM_PASSTHRU" );
			Output.WriteLine( "#undef FUNC_PARAM_PARMS_PASSIN" );
			Output.WriteLine( "#undef FUNC_PARAM_INITIALIZER_LIST" );
            Output.WriteLine( "#undef FUNC_IS_VOID" );
        }

	};



	/// <summary>
	/// The program class
	/// </summary>
	static class Program
	{
		/// <summary>
		/// Main program entry point
		/// </summary>
		/// <param name="args">Command-line arguments.  Not used in this application.</param>
		static void Main( string[] args )
		{
			// NOTE: The source file path is hard-coded relative to the directory that we expect this project to live in
			using( var Output = new StreamWriter( "..\\..\\..\\..\\..\\..\\..\\Engine\\Source\\Runtime\\Core\\Public\\Delegates\\DelegateCombinations.h" ) )
			{
				Output.WriteLine( "// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved." );
				Output.WriteLine( "" );
				Output.WriteLine( "// NOTE: This source file was automatically generated using DelegateHeaderTool" );
				Output.WriteLine( "" );
				Output.WriteLine( "// Only designed to be included directly by Delegate.h" );
				Output.WriteLine( "#if !defined( __Delegate_h__ ) || !defined( FUNC_INCLUDING_INLINE_IMPL )" );
				Output.WriteLine( "    #error \"This inline header must only be included by Delegate.h\"" );
				Output.WriteLine( "#endif" );
				Output.WriteLine( "" );
				Output.WriteLine( "" );

				var DelegateHeaderGenerator = new Generator();
				DelegateHeaderGenerator.GenerateAllDelegates( Output );
			}
		}
	}
}
