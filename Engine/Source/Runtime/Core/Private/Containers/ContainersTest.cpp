// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AutomationTest.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainersTest, "System.Core.Misc.Containers", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

#define MAX_TEST_OBJECTS      65
#define MAX_TEST_OBJECTS_STEP 1


namespace
{
	namespace EContainerTestType
	{
		enum Type
		{
			MovedFrom,
			Key,
			Value
		};
	}

	struct FContainerTestStats
	{
		int32  NextId;
		int32  ConstructedIDs[MAX_TEST_OBJECTS];
		int32* End;

		FContainerTestStats()
		{
			Reset();
		}

		void Reset()
		{
			NextId = 1;
			End    = ConstructedIDs;
		}

		int32 Num() const
		{
			return End - ConstructedIDs;
		}

		int32 Add()
		{
			// Ensure we're not constructing too many objects
			check(Num() < MAX_TEST_OBJECTS);

			// Store ID in array
			return *End++ = NextId++;
		}

		void Remove(int32 ObjId)
		{
			for (int32* It = ConstructedIDs; It != End; ++It)
			{
				if (*It != ObjId)
					continue;

				// Remove this from the list
				--End;
				for (; It != End; ++It)
				{
					*It = *(It + 1);
				}
				return;
			}

			// We didn't find an entry for this - an invalid destructor call?
			check(false);
		}
	} ContainerTestStats;

	struct FContainerTestType
	{
		FContainerTestType(const TCHAR* InStr, EContainerTestType::Type InType)
			: Str (InStr)
			, Type(InType)
			, Id  (ContainerTestStats.Add())
		{
		}

		FContainerTestType(const FContainerTestType& Other)
			: Str (Other.Str)
			, Type(Other.Type)
			, Id  (ContainerTestStats.Add())
		{
		}

		FContainerTestType(FContainerTestType&& Other)
			: Str (Other.Str)
			, Type(Other.Type)
			, Id  (ContainerTestStats.Add())
		{
			Other.Str  = NULL;
			Other.Type = EContainerTestType::MovedFrom;
		}

		FContainerTestType& operator=(FContainerTestType&& Other)
		{
			Str  = Other.Str;
			Type = Other.Type;

			Other.Str  = NULL;
			Other.Type = EContainerTestType::MovedFrom;
			
			return *( static_cast<FContainerTestType*>( this ) );
		}

		FContainerTestType& operator=(const FContainerTestType& Other)
		{
			Str  = Other.Str;
			Type = Other.Type;
			
			return *( static_cast<FContainerTestType*>( this ) );
		}

		~FContainerTestType()
		{
			ContainerTestStats.Remove(Id);
		}

		friend bool operator==(const FContainerTestType& Lhs, const FContainerTestType& Rhs)
		{
			return Lhs.Type == Rhs.Type &&
			       Lhs.Id   == Rhs.Id   &&
			       !FCString::Strcmp(Lhs.Str, Rhs.Str);
		}

		friend bool operator!=(const FContainerTestType& Lhs, const FContainerTestType& Rhs)
		{
			return !(Lhs == Rhs);
		}

		const TCHAR*             Str;
		EContainerTestType::Type Type;
		int32                    Id;
	};

	struct FContainerTestKeyType : FContainerTestType
	{
		FContainerTestKeyType()
			: FContainerTestType(TEXT("<default key>"), EContainerTestType::Key)
		{
		}

		explicit FContainerTestKeyType(const TCHAR* InStr)
			: FContainerTestType(InStr, EContainerTestType::Key)
		{
		}

		FContainerTestKeyType(FContainerTestKeyType&& Other)
			: FContainerTestType(MoveTemp(Other))
		{
		}

		FContainerTestKeyType(const FContainerTestKeyType& Other)
			: FContainerTestType(Other)
		{
		}

		FContainerTestKeyType& operator=(FContainerTestKeyType&& Other)
		{
			(FContainerTestType&)*this = MoveTemp(Other);
			
			return *( static_cast<FContainerTestKeyType*>( this ) );
		}

		FContainerTestKeyType& operator=(const FContainerTestKeyType& Other)
		{
			(FContainerTestType&)*this = Other;
			
			return *( static_cast<FContainerTestKeyType*>( this ) );
		}
	};

	struct FContainerTestValueType : FContainerTestType
	{
		FContainerTestValueType()
			: FContainerTestType(TEXT("<default value>"), EContainerTestType::Value)
		{
		}

		explicit FContainerTestValueType(const TCHAR* InStr)
			: FContainerTestType(InStr, EContainerTestType::Value)
		{
		}

		FContainerTestValueType(FContainerTestValueType&& Other)
			: FContainerTestType(MoveTemp(Other))
		{
		}

		FContainerTestValueType(const FContainerTestValueType& Other)
			: FContainerTestType(Other)
		{
		}

		FContainerTestValueType& operator=(FContainerTestValueType&& Other)
		{
			(FContainerTestType&)*this = MoveTemp(Other);
			
			return *( static_cast<FContainerTestValueType*>( this ) );
		}

		FContainerTestValueType& operator=(const FContainerTestValueType& Other)
		{
			(FContainerTestType&)*this = Other;
			
			return *( static_cast<FContainerTestValueType*>( this ) );
		}
	};

	template <typename Container>
	void CheckContainerElements(Container& Cont)
	{
		auto It  = Cont.CreateIterator();
		auto CIt = Cont.CreateConstIterator();
		for (auto& E : Cont)
		{
			check(*It  == E);
			check(*CIt == E);

			++It;
			++CIt;
		}
	}

	template <typename Container>
	void CheckContainerNum(Container& Cont)
	{
		int32 Count = 0;
		for (auto It = Cont.CreateIterator(); It; ++It)
		{
			++Count;
		}

		int32 CCount = 0;
		for (auto It = Cont.CreateConstIterator(); It; ++It)
		{
			++CCount;
		}

		int32 RCount = 0;
		for (auto& It : Cont)
		{
			++RCount;
		}

		check(Count  == Cont.Num());
		check(CCount == Cont.Num());
		check(RCount == Cont.Num());
	}

	template <typename Container>
	void CheckContainerEnds(Container& Cont)
	{
		auto Iter  = Cont.CreateIterator();
		auto CIter = Cont.CreateConstIterator();

		for (int32 Num = Cont.Num(); Num; --Num)
		{
			++Iter;
			++CIter;
		}

		check(end(                  Cont) == Iter);
		check(end((const Container&)Cont) == CIter);
	}
}

bool FContainersTest::RunTest( const FString& Parameters )
{
	typedef TMap<int32, FContainerTestValueType> TestMapType;

	ContainerTestStats.Reset();

	TestMapType TestMap;

	// Subtract one to account for temporaries that will be created during an Add
	for (int32 Count = 0; Count < MAX_TEST_OBJECTS - 1; Count += MAX_TEST_OBJECTS_STEP)
	{
		for (int32 N = 0; N != Count; ++N)
		{
			TestMap.Add(N, FContainerTestValueType(TEXT("New Value")));
			CheckContainerNum     (TestMap);
			CheckContainerEnds    (TestMap);
			CheckContainerElements(TestMap);
		}

		for (int32 N = 0; N != Count; ++N)
		{
			TestMap.Remove(N);
			CheckContainerNum     (TestMap);
			CheckContainerEnds    (TestMap);
			CheckContainerElements(TestMap);
		}

		for (int32 N = 0; N != Count; ++N)
		{
			TestMap.Add((Count - 1) - N, FContainerTestValueType(TEXT("New Value")));
			CheckContainerNum     (TestMap);
			CheckContainerEnds    (TestMap);
			CheckContainerElements(TestMap);
		}

		for (int32 N = 0; N != Count; ++N)
		{
			TestMap.Remove(N);
			CheckContainerNum     (TestMap);
			CheckContainerEnds    (TestMap);
			CheckContainerElements(TestMap);
		}
	}

	return true;
}

//Array view tests
#include "Containers/ArrayView.h"

namespace ArrayViewTests
{
	// commented out lines shouldn't compile

	struct Base
	{
		int32 b;
	};

	struct Derived : public Base
	{
		int32 d;
	};

	template<typename T>
	void TestFunction(TArrayView<T>)
	{
	}

	bool RunTest()
	{
		// C array + derived-to-base conversions
		Derived test1[13];
		TestFunction<Derived>(test1);
		//TestFunction<Base>(test1);
		//TestFunction<const Base>(test1);

		// C array of pointers + derived-to-base conversions
		Derived* test2[13];
		TestFunction<const Derived* const>(test2);
		//TestFunction<const Derived*>(test2);
		TestFunction<Derived* const>(test2);
		//TestFunction<const Base* const>(test2);

		// TArray + derived-to-base conversions
		TArray<Derived> test3;
		TestFunction<Derived>(test3);
		//TestFunction<Base>(test3);
		//TestFunction<const Base>(test3);

		// const TArray
		const TArray<Base> test4;
		TestFunction<const Base>(test4);
		//TestFunction<Base>(test4);

		// TArray of const
		TArray<const Base> test5;
		TestFunction<const Base>(test5);
		//TestFunction<Base>(test5);

		// temporary C array
		struct Test6
		{
			Base test[13];
		};
		TestFunction<const Base>(Test6().test);
		//TestFunction<Base>(Test6().test); // shouldn't compile but VS allows it :(

		return true;
	}
};
