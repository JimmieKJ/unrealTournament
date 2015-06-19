// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslUtils.h - Utilities for Hlsl.
=============================================================================*/

#pragma once

#define USE_UNREAL_ALLOCATOR	0
#define USE_PAGE_POOLING		0

namespace CrossCompiler
{
	namespace Memory
	{
		struct FPage
		{
			int8* Current;
			int8* Begin;
			int8* End;

			FPage(SIZE_T Size)
			{
				check(Size > 0);
				Begin = new int8[Size];
				End = Begin + Size;
				Current = Begin;
			}

			~FPage()
			{
				delete[] Begin;
			}

			static FPage* AllocatePage();
			static void FreePage(FPage* Page);
		};

		enum
		{
			PageSize = 64 * 1024
		};
	}

	struct FLinearAllocator
	{
		FLinearAllocator()
		{
			auto* Initial = Memory::FPage::AllocatePage();
			Pages.Add(Initial);
		}

		~FLinearAllocator()
		{
			for (auto* Page : Pages)
			{
				Memory::FPage::FreePage(Page);
			}
		}

		inline void* Alloc(SIZE_T NumBytes)
		{
			check(NumBytes <= Memory::PageSize);
			auto* Page = Pages.Last();
			if (Page->Current + NumBytes > Page->End)
			{
				Page = Memory::FPage::AllocatePage();
				Pages.Add(Page);
			}

			void* Ptr = Page->Current;
			Page->Current += NumBytes;
			return Ptr;
		}

		inline void* Alloc(SIZE_T NumBytes, SIZE_T Align)
		{
			void* Data = Alloc(NumBytes + Align - 1);
			UPTRINT Address = (UPTRINT)Data;
			Address += (Align - (Address % (UPTRINT)Align)) % Align;
			return (void*)Address;
		}

		/*inline*/ TCHAR* Strdup(const TCHAR* String)
		{
			if (!String)
			{
				return nullptr;
			}

			const auto Length = FCString::Strlen(String);
			const auto Size = (Length + 1) * sizeof(TCHAR);
			auto* Data = (TCHAR*)Alloc(Size);
			FCString::Strcpy(Data, Length, String);
			return Data;
		}

		inline TCHAR* Strdup(const FString& String)
		{
			return Strdup(*String);
		}

		TArray<Memory::FPage*, TInlineAllocator<8> > Pages;
	};

#if !USE_UNREAL_ALLOCATOR
	class FLinearAllocatorPolicy
	{
	public:
		// Unreal allocator magic
		enum { NeedsElementType = false };
		enum { RequireRangeCheck = true };

		template<typename ElementType>
		class ForElementType
		{
		public:

			/** Default constructor. */
			ForElementType() :
				LinearAllocator(nullptr),
				Data(nullptr)
			{}

			// FContainerAllocatorInterface
			/*FORCEINLINE*/ ElementType* GetAllocation() const
			{
				return Data;
			}
			void ResizeAllocation(int32 PreviousNumElements, int32 NumElements, int32 NumBytesPerElement)
			{
				void* OldData = Data;
				if (NumElements)
				{
					// Allocate memory from the stack.
					Data = (ElementType*)LinearAllocator->Alloc(NumElements * NumBytesPerElement,
						FMath::Max((uint32)sizeof(void*), (uint32)ALIGNOF(ElementType))
						);

					// If the container previously held elements, copy them into the new allocation.
					if (OldData && PreviousNumElements)
					{
						const int32 NumCopiedElements = FMath::Min(NumElements, PreviousNumElements);
						FMemory::Memcpy(Data, OldData, NumCopiedElements * NumBytesPerElement);
					}
				}
			}
			int32 CalculateSlack(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
			{
				return DefaultCalculateSlack(NumElements, NumAllocatedElements, NumBytesPerElement);
			}

			int32 GetAllocatedSize(int32 NumAllocatedElements, int32 NumBytesPerElement) const
			{
				return NumAllocatedElements * NumBytesPerElement;
			}

			FLinearAllocator* LinearAllocator;

		private:

			/** A pointer to the container's elements. */
			ElementType* Data;
		};

		typedef ForElementType<FScriptContainerElement> ForAnyElementType;
	};

	class FLinearBitArrayAllocator
		: public TInlineAllocator<4, FLinearAllocatorPolicy>
	{
	};

	class FLinearSparseArrayAllocator
		: public TSparseArrayAllocator<FLinearAllocatorPolicy, FLinearBitArrayAllocator>
	{
	};

	class FLinearSetAllocator
		: public TSetAllocator<FLinearSparseArrayAllocator, TInlineAllocator<1, FLinearAllocatorPolicy> >
	{
	};

	template <typename TType>
	class TLinearArray : public TArray<TType, FLinearAllocatorPolicy>
	{
	public:
		TLinearArray(FLinearAllocator* Allocator)
		{
			TArray<TType, FLinearAllocatorPolicy>::AllocatorInstance.LinearAllocator = Allocator;
		}
	};

	/*
	template <typename TType>
	struct TLinearSet : public TSet<typename TType, DefaultKeyFuncs<typename TType>, FLinearSetAllocator>
	{
	TLinearSet(FLinearAllocator* InAllocator)
	{
	Elements.AllocatorInstance.LinearAllocator = InAllocator;
	}
	};*/
#endif

	struct FSourceInfo
	{
		FString* Filename;
		int32 Line;
		int32 Column;

		FSourceInfo() : Filename(nullptr), Line(0), Column(0) {}
	};

	inline void SourceError(const FSourceInfo& SourceInfo, const TCHAR* String)
	{
		//@todo-rco: LOG
		if (SourceInfo.Filename)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s(%d): (%d) %s\n"), **SourceInfo.Filename, SourceInfo.Line, SourceInfo.Column, String);
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("<unknown>(%d): (%d) %s\n"), SourceInfo.Line, SourceInfo.Column, String);
		}
	}

	inline void SourceError(const TCHAR* String)
	{
		//@todo-rco: LOG
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), String);
	}

	inline void SourceWarning(const FSourceInfo& SourceInfo, const TCHAR* String)
	{
		//@todo-rco: LOG
		if (SourceInfo.Filename)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s(%d): (%d) %s\n"), **SourceInfo.Filename, SourceInfo.Line, SourceInfo.Column, String);
		}
		else
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("<unknown>(%d): (%d) %s\n"), SourceInfo.Line, SourceInfo.Column, String);
		}
	}

	inline void SourceWarning(const TCHAR* String)
	{
		//@todo-rco: LOG
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), String);
	}
}
