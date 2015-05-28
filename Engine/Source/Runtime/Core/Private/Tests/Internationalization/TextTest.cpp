// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AutomationTest.h"
#include "ICUUtilities.h"


// Disable optimization for TextTest as it compiles very slowly in development builds
PRAGMA_DISABLE_OPTIMIZATION

#define LOCTEXT_NAMESPACE "Core.Tests.TextFormatTest"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTextTest, "System.Core.Misc.Text", EAutomationTestFlags::ATF_None)

namespace
{
	FText FormatWithoutArguments(const FText& Pattern)
	{
		FFormatOrderedArguments Arguments;
		return FText::Format(Pattern, Arguments);
	}

	void ArrayToString(const TArray<FString>& Array, FString& String)
	{
		const int32 Count = Array.Num();
		for(int32 i = 0; i < Count; ++i)
		{
			if(i > 0)
			{
				String += TEXT(", ");
			}
			String += Array[i];
		}
	}

	void TestPatternParameterEnumeration(FTextTest& Test, const FText& Pattern, TArray<FString>& ActualParameters, const TArray<FString>& ExpectedParameters)
	{
		ActualParameters.Empty(ExpectedParameters.Num());
		FText::GetFormatPatternParameters(Pattern, ActualParameters);
		if(ActualParameters != ExpectedParameters)
		{
			FString ActualParametersString;
			ArrayToString(ActualParameters, ActualParametersString);
			FString ExpectedParametersString;
			ArrayToString(ExpectedParameters, ExpectedParametersString);
			Test.AddError( FString::Printf( TEXT("\"%s\" contains parameters (%s) but expected parameters (%s)."), *(Pattern.ToString()), *(ActualParametersString), *(ExpectedParametersString) ) );
		}
	}
}


bool FTextTest::RunTest (const FString& Parameters)
{
	AddLogItem(TEXT("This test is destructive to existing culture invariant text! All culture invariant text will appear in LEET afterwards!"));

	FInternationalization& I18N = FInternationalization::Get();
	const bool OriginalEnableErrorCheckingValue = FText::GetEnableErrorCheckingResults();
	const bool OriginalSuppressWarningsValue = FText::GetSuppressWarnings();
	const FString OriginalCulture = I18N.GetCurrentCulture()->GetName();

	FText::SetEnableErrorCheckingResults(true);
	FText::SetSuppressWarnings(true);

	FText ArgText0 = FText::FromString(TEXT("Arg0"));
	FText ArgText1 = FText::FromString(TEXT("Arg1"));
	FText ArgText2 = FText::FromString(TEXT("Arg2"));
	FText ArgText3 = FText::FromString(TEXT("Arg3"));

#define INVTEXT(x) FText::FromString(TEXT(x))

#define TEST( Desc, A, B ) if( !A.EqualTo(B) ) AddError(FString::Printf(TEXT("%s - A=%s B=%s"),*Desc,*A.ToString(),*B.ToString()))
	
	FText TestText;

	TestText = INVTEXT("Format with single apostrophes quotes: '{0}'");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Format with single apostrophes quotes: 'Arg0'"));
	TestText = INVTEXT("Format with double apostrophes quotes: ''{0}''");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Format with double apostrophes quotes: ''Arg0''"));
	TestText = INVTEXT("Print with single graves: `{0}`");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Print with single graves: {0}"));
	TestText = INVTEXT("Format with double graves: ``{0}``");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Format with double graves: `Arg0`"));

	TestText = INVTEXT("Testing `escapes` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing `escapes` here."));
	TestText = INVTEXT("Testing ``escapes` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing `escapes` here."));
	TestText = INVTEXT("Testing ``escapes`` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing `escapes` here."));

	TestText = INVTEXT("Testing `}escapes{ here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing }escapes{ here."));
	TestText = INVTEXT("Testing `}escapes{ here.`");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing }escapes{ here."));
	TestText = INVTEXT("Testing `}escapes{` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing }escapes{ here."));
	TestText = INVTEXT("Testing }escapes`{ here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing }escapes{ here."));
	TestText = INVTEXT("`Testing }escapes`{ here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("`Testing }escapes{ here."));

	TestText = INVTEXT("Testing `{escapes} here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing {escapes} here."));
	TestText = INVTEXT("Testing `{escapes} here.`");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing {escapes} here."));
	TestText = INVTEXT("Testing `{escapes}` here.");
	TEST( TestText.ToString(), FormatWithoutArguments(TestText), INVTEXT("Testing {escapes} here."));

	TestText = INVTEXT("Starting text: {0} {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 Arg1"));
	TestText = INVTEXT("{0} {1} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 Arg1 - Ending Text."));
	TestText = INVTEXT("Starting text: {0} {1} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 Arg1 - Ending Text."));
	TestText = INVTEXT("{0} {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 Arg1"));
	TestText = INVTEXT("{1} {0}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg1 Arg0"));
	TestText = INVTEXT("{0}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Arg0"));
	TestText = INVTEXT("{0} - {1} - {2} - {3}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0,ArgText1,ArgText2,ArgText3), INVTEXT("Arg0 - Arg1 - Arg2 - Arg3"));
	TestText = INVTEXT("{0} - {0} - {0} - {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 - Arg0 - Arg0 - Arg1"));

	TestText = INVTEXT("Starting text: {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg1"));
	TestText = INVTEXT("{0} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 - Ending Text."));
	TestText = INVTEXT("Starting text: {0} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 - Ending Text."));

	TestText = INVTEXT("{0} {2}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1, ArgText2), INVTEXT("Arg0 Arg2"));
	TestText = INVTEXT("{1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1, ArgText2), INVTEXT("Arg1"));

	FText::SetEnableErrorCheckingResults(false);
	FText::SetSuppressWarnings(true);

	TestText = INVTEXT("Starting text: {0} {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 Arg1"));
	TestText = INVTEXT("{0} {1} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 Arg1 - Ending Text."));
	TestText = INVTEXT("Starting text: {0} {1} - Ending Text.");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Starting text: Arg0 Arg1 - Ending Text."));
	TestText = INVTEXT("{0} {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 Arg1"));
	TestText = INVTEXT("{1} {0}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg1 Arg0"));
	TestText = INVTEXT("{0}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0), INVTEXT("Arg0"));
	TestText = INVTEXT("{0} - {1} - {2} - {3}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0,ArgText1,ArgText2,ArgText3), INVTEXT("Arg0 - Arg1 - Arg2 - Arg3"));
	TestText = INVTEXT("{0} - {0} - {0} - {1}");
	TEST( TestText.ToString(), FText::Format(TestText, ArgText0, ArgText1), INVTEXT("Arg0 - Arg0 - Arg0 - Arg1"));

	{
		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("Age"), FText::FromString( TEXT("23") ) );
		Arguments.Add( TEXT("Height"), FText::FromString( TEXT("68") ) );
		Arguments.Add( TEXT("Gender"), FText::FromString( TEXT("male") ) );
		Arguments.Add( TEXT("Name"), FText::FromString( TEXT("Saul") ) );

		// Not using all the arguments is okay.
		TestText = INVTEXT("My name is {Name}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My name is Saul.") );
		TestText = INVTEXT("My age is {Age}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My age is 23.") );
		TestText = INVTEXT("My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My gender is male.") );
		TestText = INVTEXT("My height is {Height}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My height is 68.") );

		// Using arguments out of order is okay.
		TestText = INVTEXT("My name is {Name}. My age is {Age}. My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My name is Saul. My age is 23. My gender is male.") );
		TestText = INVTEXT("My age is {Age}. My gender is {Gender}. My name is {Name}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My age is 23. My gender is male. My name is Saul.") );
		TestText = INVTEXT("My gender is {Gender}. My name is {Name}. My age is {Age}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My gender is male. My name is Saul. My age is 23.") );
		TestText = INVTEXT("My gender is {Gender}. My age is {Age}. My name is {Name}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My gender is male. My age is 23. My name is Saul.") );
		TestText = INVTEXT("My age is {Age}. My name is {Name}. My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My age is 23. My name is Saul. My gender is male.") );
		TestText = INVTEXT("My name is {Name}. My gender is {Gender}. My age is {Age}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("My name is Saul. My gender is male. My age is 23.") );

		// Reusing arguments is okay.
		TestText = INVTEXT("If my age is {Age}, I have been alive for {Age} year(s).");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("If my age is 23, I have been alive for 23 year(s).") );

		// Not providing an argument leaves the parameter as text.
		TestText = INVTEXT("What... is the air-speed velocity of an unladen swallow? {AirSpeedOfAnUnladenSwallow}.");
		TEST( TestText.ToString(), FText::Format( TestText, Arguments), INVTEXT("What... is the air-speed velocity of an unladen swallow? {AirSpeedOfAnUnladenSwallow}.") );
	}

	{
		TArray<FFormatArgumentData> ArgumentList;
		{
			FFormatArgumentData Argument;
			Argument.ArgumentName = FText::FromString( TEXT("Age") );
			Argument.ArgumentValue = FText::FromString( TEXT("23") );
			ArgumentList.Add(Argument);
		}
		{
			FFormatArgumentData Argument;
			Argument.ArgumentName = FText::FromString( TEXT("Height") );
			Argument.ArgumentValue = FText::FromString( TEXT("68") );
			ArgumentList.Add(Argument);
		}
		{
			FFormatArgumentData Argument;
			Argument.ArgumentName = FText::FromString( TEXT("Gender") );
			Argument.ArgumentValue = FText::FromString( TEXT("male") );
			ArgumentList.Add(Argument);
		}
		{
			FFormatArgumentData Argument;
			Argument.ArgumentName = FText::FromString( TEXT("Name") );
			Argument.ArgumentValue = FText::FromString( TEXT("Saul") );
			ArgumentList.Add(Argument);
		}

		// Not using all the arguments is okay.
		TestText = INVTEXT("My name is {Name}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My name is Saul.") );
		TestText = INVTEXT("My age is {Age}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My age is 23.") );
		TestText = INVTEXT("My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My gender is male.") );
		TestText = INVTEXT("My height is {Height}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My height is 68.") );

		// Using arguments out of order is okay.
		TestText = INVTEXT("My name is {Name}. My age is {Age}. My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My name is Saul. My age is 23. My gender is male.") );
		TestText = INVTEXT("My age is {Age}. My gender is {Gender}. My name is {Name}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My age is 23. My gender is male. My name is Saul.") );
		TestText = INVTEXT("My gender is {Gender}. My name is {Name}. My age is {Age}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My gender is male. My name is Saul. My age is 23.") );
		TestText = INVTEXT("My gender is {Gender}. My age is {Age}. My name is {Name}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My gender is male. My age is 23. My name is Saul.") );
		TestText = INVTEXT("My age is {Age}. My name is {Name}. My gender is {Gender}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My age is 23. My name is Saul. My gender is male.") );
		TestText = INVTEXT("My name is {Name}. My gender is {Gender}. My age is {Age}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("My name is Saul. My gender is male. My age is 23.") );

		// Reusing arguments is okay.
		TestText = INVTEXT("If my age is {Age}, I have been alive for {Age} year(s).");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("If my age is 23, I have been alive for 23 year(s).") );

		// Not providing an argument leaves the parameter as text.
		TestText = INVTEXT("What... is the air-speed velocity of an unladen swallow? {AirSpeedOfAnUnladenSwallow}.");
		TEST( TestText.ToString(), FText::Format(TestText, ArgumentList), INVTEXT("What... is the air-speed velocity of an unladen swallow? {AirSpeedOfAnUnladenSwallow}.") );
	}

#undef TEST
#define TEST( Pattern, Actual, Expected ) TestPatternParameterEnumeration(*this, Pattern, Actual, Expected)

	TArray<FString> ActualArguments;
	TArray<FString> ExpectedArguments;

	TestText = INVTEXT("My name is {Name}.");
	ExpectedArguments.Empty(1);
	ExpectedArguments.Add(TEXT("Name"));
	TEST(TestText, ActualArguments, ExpectedArguments);

	TestText = INVTEXT("My age is {Age}.");
	ExpectedArguments.Empty(1);
	ExpectedArguments.Add(TEXT("Age"));
	TEST(TestText, ActualArguments, ExpectedArguments);

	TestText = INVTEXT("If my age is {Age}, I have been alive for {Age} year(s).");
	ExpectedArguments.Empty(1);
	ExpectedArguments.Add(TEXT("Age"));
	TEST(TestText, ActualArguments, ExpectedArguments);

	TestText = INVTEXT("{0} - {1} - {2} - {3}");
	ExpectedArguments.Empty(4);
	ExpectedArguments.Add(TEXT("0"));
	ExpectedArguments.Add(TEXT("1"));
	ExpectedArguments.Add(TEXT("2"));
	ExpectedArguments.Add(TEXT("3"));
	TEST(TestText, ActualArguments, ExpectedArguments);

	TestText = INVTEXT("My name is {Name}. My age is {Age}. My gender is {Gender}.");
	ExpectedArguments.Empty(3);
	ExpectedArguments.Add(TEXT("Name"));
	ExpectedArguments.Add(TEXT("Age"));
	ExpectedArguments.Add(TEXT("Gender"));
	TEST(TestText, ActualArguments, ExpectedArguments);

#undef TEST

#undef INVTEXT

	FText::SetEnableErrorCheckingResults(true);
	FText::SetSuppressWarnings(true);

#if UE_ENABLE_ICU
	if (I18N.SetCurrentCulture("en-US"))
	{
#define TEST( A, B, ComparisonLevel ) if( !(FText::FromString(A)).EqualTo(FText::FromString(B), (ComparisonLevel)) ) AddError(FString::Printf(TEXT("Testing comparison of equivalent characters with comparison level (%s). - A=%s B=%s"),TEXT(#ComparisonLevel),(A),(B)))

		// Basic sanity checks
		TEST( TEXT("a"), TEXT("A"), ETextComparisonLevel::Primary ); // Basic sanity check
		TEST( TEXT("a"), TEXT("a"), ETextComparisonLevel::Tertiary ); // Basic sanity check
		TEST( TEXT("A"), TEXT("A"), ETextComparisonLevel::Tertiary ); // Basic sanity check

		// Test equivalence
		TEST( TEXT("ss"), TEXT("\x00DF"), ETextComparisonLevel::Primary ); // Lowercase Sharp s
		TEST( TEXT("SS"), TEXT("\x1E9E"), ETextComparisonLevel::Primary ); // Uppercase Sharp S
		TEST( TEXT("ae"), TEXT("\x00E6"), ETextComparisonLevel::Primary ); // Lowercase ae
		TEST( TEXT("AE"), TEXT("\x00C6"), ETextComparisonLevel::Primary ); // Uppercase AE

		// Test accentuation
		TEST( TEXT("u") , TEXT("\x00FC"), ETextComparisonLevel::Primary ); // Lowercase u with dieresis
		TEST( TEXT("U") , TEXT("\x00DC"), ETextComparisonLevel::Primary ); // Uppercase U with dieresis

#undef TEST
	}
	else
	{
		AddWarning(FString::Printf(TEXT("Internationalization data for %s missing - test is partially disabled."), TEXT("en-US")));
	}
#else
	AddWarning("ICU is disabled thus locale-aware string comparison is disabled.");
#endif

#if UE_ENABLE_ICU
	// Sort Testing
	// French
	if (I18N.SetCurrentCulture("fr"))
	{
		TArray<FText> CorrectlySortedValues;
		CorrectlySortedValues.Add( FText::FromString( TEXT("cote") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("coté") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("côte") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("côté") ) );

		{
			// Make unsorted.
			TArray<FText> Values;
			Values.Reserve(CorrectlySortedValues.Num());

			Values.Add(CorrectlySortedValues[1]);
			Values.Add(CorrectlySortedValues[3]);
			Values.Add(CorrectlySortedValues[2]);
			Values.Add(CorrectlySortedValues[0]);

			// Execute sort.
			Values.Sort(FText::FSortPredicate());

			// Test if sorted.
			bool Identical = true;
			for(int32 j = 0; j < Values.Num(); ++j)
			{
				Identical = Values[j].EqualTo(CorrectlySortedValues[j]);
				if(!Identical)
				{
					break;
				}
			}
			if( !Identical )
			{
				//currently failing AddError(FString::Printf(TEXT("Sort order is wrong for culture (%s)."), *FInternationalization::Get().GetCurrentCulture()->GetEnglishName()));
			}
		}
	}
	else
	{
		AddWarning(FString::Printf(TEXT("Internationalization data for %s missing - test is partially disabled."), TEXT("fr")));
	}

	// French Canadian
	if (I18N.SetCurrentCulture("fr-CA"))
	{
		TArray<FText> CorrectlySortedValues;
		CorrectlySortedValues.Add( FText::FromString( TEXT("cote") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("côte") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("coté") ) );
		CorrectlySortedValues.Add( FText::FromString( TEXT("côté") ) );

		{
			// Make unsorted.
			TArray<FText> Values;
			Values.Reserve(CorrectlySortedValues.Num());

			Values.Add(CorrectlySortedValues[1]);
			Values.Add(CorrectlySortedValues[3]);
			Values.Add(CorrectlySortedValues[2]);
			Values.Add(CorrectlySortedValues[0]);

			// Execute sort.
			Values.Sort(FText::FSortPredicate());

			// Test if sorted.
			bool Identical = true;
			for(int32 j = 0; j < Values.Num(); ++j)
			{
				Identical = Values[j].EqualTo(CorrectlySortedValues[j]);
				if(!Identical) break;
			}
			if( !Identical )
			{
				//currently failing AddError(FString::Printf(TEXT("Sort order is wrong for culture (%s)."), *FInternationalization::Get().GetCurrentCulture()->GetEnglishName()));
			}
		}
	}
	else
	{
		AddWarning(FString::Printf(TEXT("Internationalization data for %s missing - test is partially disabled."), TEXT("fr-CA")));
	}
#else
	AddWarning("ICU is disabled thus locale-aware string collation is disabled.");
#endif

#if UE_ENABLE_ICU
	{
		I18N.SetCurrentCulture(OriginalCulture);

		TArray<uint8> FormattedHistoryAsEnglish;
		TArray<uint8> FormattedHistoryAsFrenchCanadian;
		TArray<uint8> InvariantFTextData;

		FString InvariantString = TEXT("This is a culture invariant string.");
		FString FormattedTestLayer2_OriginalLanguageSourceString;
		FText FormattedTestLayer2;

		// Scoping to allow all locals to leave scope after we serialize at the end
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("String1"), LOCTEXT("RebuildFTextTest1_Lorem", "Lorem"));
			Args.Add(TEXT("String2"), LOCTEXT("RebuildFTextTest1_Ipsum", "Ipsum"));
			FText FormattedTest1 = FText::Format(LOCTEXT("RebuildNamedText1", "{String1} \"Lorem Ipsum\" {String2}"), Args);

			FFormatOrderedArguments ArgsOrdered;
			ArgsOrdered.Add(LOCTEXT("RebuildFTextTest1_Lorem", "Lorem"));
			ArgsOrdered.Add(LOCTEXT("RebuildFTextTest1_Ipsum", "Ipsum"));
			FText FormattedTestOrdered1 = FText::Format(LOCTEXT("RebuildOrderedText1", "{0} \"Lorem Ipsum\" {1}"), ArgsOrdered);

			// Will change to 5.542 due to default settings for numbers
			FText AsNumberTest1 = FText::AsNumber(5.5421);

			FText AsPercentTest1 = FText::AsPercent(0.925);
			FText AsCurrencyTest1 = FText::AsCurrency(100.25);

			FDateTime DateTimeInfo(2080, 8, 20, 9, 33, 22);
			FText AsDateTimeTest1 = FText::AsDateTime(DateTimeInfo, EDateTimeStyle::Default, EDateTimeStyle::Default, TEXT("UTC"));

			// FormattedTestLayer2 must be updated when adding or removing from this block. Further, below, 
			// verifying the LEET translated string must be changed to reflect what the new string looks like.
			FFormatNamedArguments ArgsLayer2;
			ArgsLayer2.Add("NamedLayer1", FormattedTest1);
			ArgsLayer2.Add("OrderedLayer1", FormattedTestOrdered1);
			ArgsLayer2.Add("FTextNumber", AsNumberTest1);
			ArgsLayer2.Add("Number", 5010.89221);
			ArgsLayer2.Add("DateTime", AsDateTimeTest1);
			ArgsLayer2.Add("Percent", AsPercentTest1);
			ArgsLayer2.Add("Currency", AsCurrencyTest1);
			FormattedTestLayer2 = FText::Format(LOCTEXT("RebuildTextLayer2", "{NamedLayer1} | {OrderedLayer1} | {FTextNumber} | {Number} | {DateTime} | {Percent} | {Currency}"), ArgsLayer2);

			{
				// Serialize the full, bulky FText that is a composite of most of the other FTextHistories.
				FMemoryWriter Ar(FormattedHistoryAsEnglish);
				Ar << FormattedTestLayer2;
				Ar.Close();
			}

			// The original string in the native language.
			FormattedTestLayer2_OriginalLanguageSourceString = FormattedTestLayer2.BuildSourceString();

			{
				// Swap to "LEET" culture to check if rebuilding works (verify the whole)
				I18N.SetCurrentCulture("LEET");

				// When changes are made to FormattedTestLayer2, please pull out the newly translated LEET string and update the below if-statement to keep the test passing!
				FString LEETTranslatedString = FormattedTestLayer2.ToString();

				FString DesiredOutput = FString(TEXT("\x2021") TEXT("\xAB") TEXT("\xAB") TEXT("L0r3m") TEXT("\xBB") TEXT(" \"L0r3m 1p$um\" ") TEXT("\xAB") TEXT("Ip$um") TEXT("\xBB") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("\xAB") TEXT("L0r3m") TEXT("\xBB") TEXT(" \"L0r3m 1p$um\" ") TEXT("\xAB") TEXT("Ip$um") TEXT("\xBB") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("5.5421") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("5010.89221") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("Aug 20, 2080, 9:33:22 AM") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("92%") TEXT("\xBB") TEXT(" | ") TEXT("\xAB") TEXT("\xA4") TEXT("\xA0") TEXT("100.25") TEXT("\xBB") TEXT("\x2021"));
				// Convert the baked string into an FText, which will be leetified, then compare it to the rebuilt FText
				if(LEETTranslatedString != DesiredOutput)
				{
					AddError( TEXT("FormattedTestLayer2 did not rebuild to correctly in LEET!") );
					AddError( TEXT("Formatted Output=") + LEETTranslatedString );
					AddError( TEXT("Desired Output=") + DesiredOutput );
				}
			}

			// Swap to French-Canadian to check if rebuilding works (verify each numerical component)
			{
				I18N.SetCurrentCulture("fr-CA");

				// Need the FText to be rebuilt in fr-CA.
				FormattedTestLayer2.ToString();

				if(AsNumberTest1.CompareTo(FText::AsNumber(5.5421)) != 0)
				{
					AddError( TEXT("AsNumberTest1 did not rebuild correctly in French-Canadian") );
					AddError( TEXT("Number Output=") + AsNumberTest1.ToString() );
				}

				if(AsPercentTest1.CompareTo(FText::AsPercent(0.925)) != 0)
				{
					AddError( TEXT("AsPercentTest1 did not rebuild correctly in French-Canadian") );
					AddError( TEXT("Percent Output=") + AsPercentTest1.ToString() );
				}

				if(AsCurrencyTest1.CompareTo(FText::AsCurrency(100.25)) != 0)
				{
					AddError( TEXT("AsCurrencyTest1 did not rebuild correctly in French-Canadian") );
					AddError( TEXT("Currency Output=") + AsCurrencyTest1.ToString() );
				}

				if(AsDateTimeTest1.CompareTo(FText::AsDateTime(DateTimeInfo, EDateTimeStyle::Default, EDateTimeStyle::Default, TEXT("UTC"))) != 0)
				{
					AddError( TEXT("AsDateTimeTest1 did not rebuild correctly in French-Canadian") );
					AddError( TEXT("DateTime Output=") + AsDateTimeTest1.ToString() );
				}

				{
					// Serialize the full, bulky FText that is a composite of most of the other FTextHistories.
					// We don't care how this may be translated, we will be serializing this in as LEET.
					FMemoryWriter Ar(FormattedHistoryAsFrenchCanadian);
					Ar << FormattedTestLayer2;
					Ar.Close();
				}

				{
					FText InvariantFText = FText::FromString(InvariantString);

					// Serialize an invariant FText
					FMemoryWriter Ar(InvariantFTextData);
					Ar << InvariantFText;
					Ar.Close();
				}
			}
		}

		{
			I18N.SetCurrentCulture("LEET");

			FText FormattedEnglishTextHistoryAsLeet;
			FText FormattedFrenchCanadianTextHistoryAsLeet;

			{
				FMemoryReader Ar(FormattedHistoryAsEnglish);
				Ar << FormattedEnglishTextHistoryAsLeet;
				Ar.Close();
			}
			{
				FMemoryReader Ar(FormattedHistoryAsFrenchCanadian);
				Ar << FormattedFrenchCanadianTextHistoryAsLeet;
				Ar.Close();
			}

			// Confirm the two FText's serialize in and get translated into the current (LEET) translation. One originated in English, the other in French-Canadian locales.
			if(FormattedEnglishTextHistoryAsLeet.CompareTo(FormattedFrenchCanadianTextHistoryAsLeet) != 0)
			{
				AddError( TEXT("Serialization of text histories from source English and source French-Canadian to LEET did not produce the same results!") );
				AddError( TEXT("English Output=") + FormattedEnglishTextHistoryAsLeet.ToString() );
				AddError( TEXT("French-Canadian Output=") + FormattedFrenchCanadianTextHistoryAsLeet.ToString() );
			}

			// Confirm the two FText's source strings for the serialized FTexts are the same.
			if(FormattedEnglishTextHistoryAsLeet.BuildSourceString() != FormattedFrenchCanadianTextHistoryAsLeet.BuildSourceString())
			{
				AddError( TEXT("Serialization of text histories from source English and source French-Canadian to LEET did not produce the same source results!") );
				AddError( TEXT("English Output=") + FormattedEnglishTextHistoryAsLeet.BuildSourceString() );
				AddError( TEXT("French-Canadian Output=") + FormattedFrenchCanadianTextHistoryAsLeet.BuildSourceString() );
			}

			// Rebuild in LEET so that when we build the source string the DisplayString is still in LEET. 
			FormattedTestLayer2.ToString();

			{
				I18N.SetCurrentCulture(OriginalCulture);

				FText InvariantFText;

				FMemoryReader Ar(InvariantFTextData);
				Ar << InvariantFText;
				Ar.Close();

				if(InvariantFText.ToString() != InvariantString)
				{
					AddError( TEXT("Invariant FText did not match the original FString after serialization!") );
					AddError( TEXT("Invariant Output=") + InvariantFText.ToString() );
				}


				FString FormattedTestLayer2_SourceString = FormattedTestLayer2.BuildSourceString();

				// Compare the source string of the LEETified version of FormattedTestLayer2 to ensure it is correct.
				if(FormattedTestLayer2_OriginalLanguageSourceString != FormattedTestLayer2_SourceString)
				{
					AddError( TEXT("FormattedTestLayer2's source string was incorrect!") );
					AddError( TEXT("Output=") + FormattedTestLayer2_SourceString );
					AddError( TEXT("Desired Output=") + FormattedTestLayer2_OriginalLanguageSourceString );
				}
			}
		}
	}
#else
	AddWarning("ICU is disabled thus locale-aware formatting needed in rebuilding source text from history is disabled.");
#endif

	FText::SetEnableErrorCheckingResults(OriginalEnableErrorCheckingValue);
	FText::SetSuppressWarnings(OriginalSuppressWarningsValue);

	//**********************************
	// FromString Test
	//**********************************
	TestText = FText::FromString( TEXT("Test String") );

	if ( GIsEditor && TestText.IsCultureInvariant() )
	{
		AddError( TEXT("FromString should not produce a Culture Invariant Text when called inside the editor") );
	}
	
	if ( !GIsEditor && !TestText.IsCultureInvariant() )
	{
		AddError( TEXT("FromString should produce a Culture Invariant Text when called outside the editor") );
	}

	if ( TestText.IsTransient() )
	{
		AddError( TEXT("FromString should never produce a Transient Text") );
	}

	I18N.SetCurrentCulture(OriginalCulture);

	return true;
}


#if UE_ENABLE_ICU

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FICUTextTest, "System.Core.Misc.ICUText", EAutomationTestFlags::ATF_SmokeTest)

bool FICUTextTest::RunTest (const FString& Parameters)
{
	// Test to make sure that ICUUtilities converts strings correctly

	const FString SourceString(TEXT("This is a test"));
	const FString SourceString2(TEXT("This is another test"));
	icu::UnicodeString ICUString;
	FString ConversionBackStr;

	// ---------------------------------------------------------------------

	ICUUtilities::ConvertString(SourceString, ICUString);
	if (SourceString.Len() != ICUString.countChar32())
	{
		AddError(FString::Printf(TEXT("icu::UnicodeString is the incorrect length (%d; expected %d)."), ICUString.countChar32(), SourceString.Len()));
	}

	ICUUtilities::ConvertString(ICUString, ConversionBackStr);
	if (ICUString.length() != ConversionBackStr.Len())
	{
		AddError(FString::Printf(TEXT("FString is the incorrect length (%d; expected %d)."), ConversionBackStr.Len(), ICUString.countChar32()));
	}
	if (SourceString != ConversionBackStr)
	{
		AddError(FString::Printf(TEXT("FString is has the incorrect converted value ('%s'; expected '%s')."), *ConversionBackStr, *SourceString));
	}

	// ---------------------------------------------------------------------

	ICUUtilities::ConvertString(SourceString2, ICUString);
	if (SourceString2.Len() != ICUString.countChar32())
	{
		AddError(FString::Printf(TEXT("icu::UnicodeString is the incorrect length (%d; expected %d)."), ICUString.countChar32(), SourceString2.Len()));
	}

	ICUUtilities::ConvertString(ICUString, ConversionBackStr);
	if (ICUString.length() != ConversionBackStr.Len())
	{
		AddError(FString::Printf(TEXT("FString is the incorrect length (%d; expected %d)."), ConversionBackStr.Len(), ICUString.countChar32()));
	}
	if (SourceString2 != ConversionBackStr)
	{
		AddError(FString::Printf(TEXT("FString is has the incorrect converted value ('%s'; expected '%s')."), *ConversionBackStr, *SourceString2));
	}

	// ---------------------------------------------------------------------

	ICUUtilities::ConvertString(SourceString, ICUString);
	if (SourceString.Len() != ICUString.countChar32())
	{
		AddError(FString::Printf(TEXT("icu::UnicodeString is the incorrect length (%d; expected %d)."), ICUString.countChar32(), SourceString.Len()));
	}

	ICUUtilities::ConvertString(ICUString, ConversionBackStr);
	if (ICUString.length() != ConversionBackStr.Len())
	{
		AddError(FString::Printf(TEXT("FString is the incorrect length (%d; expected %d)."), ConversionBackStr.Len(), ICUString.countChar32()));
	}
	if (SourceString != ConversionBackStr)
	{
		AddError(FString::Printf(TEXT("FString is has the incorrect converted value ('%s'; expected '%s')."), *ConversionBackStr, *SourceString));
	}

	return true;
}

#endif // #if UE_ENABLE_ICU

#undef LOCTEXT_NAMESPACE 


PRAGMA_ENABLE_OPTIMIZATION
