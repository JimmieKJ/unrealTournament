// Copyright 2013 Allegorithmic. All Rights Reserved.
#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "framework/std_wrappers.h"

static_assert(WITH_SUBSTANCE, "no_point_in_compiling_substancecore_if_the_substance_library_is_not_present");

#define SBS_DEFAULT_MEMBUDGET_MB (32)
#define SBS_DFT_COOKED_MIPS_NB (5)

//! A few forward declaration
namespace Substance
{
	struct FPackage;

	struct FGraphDesc;
	struct FOutputDesc;
	struct FInputDescBase;
	struct FImageInputDesc;

	struct FGraphInstance;
	struct FOutputInstance;
	struct FInputInstanceBase;
	struct FNumericalInputInstanceBase;
	struct FImageInputInstance;

	struct FPreset;

	//! @brief The list of channels roles available in Designer
	enum ChannelUse
	{
		CHAN_Undef				=0,
		CHAN_Diffuse,
		CHAN_Opacity,
		CHAN_Emissive,
		CHAN_Ambient,
		CHAN_AmbientOcclusion,
		CHAN_Mask,
		CHAN_Normal,
		CHAN_Bump,
		CHAN_Height,
		CHAN_Displacement,
		CHAN_Specular,
		CHAN_SpecularLevel,
		CHAN_SpecularColor,
		CHAN_Glossiness,
		CHAN_Roughness,
		CHAN_AnisotropyLevel,
		CHAN_AnisotropyAngle,
		CHAN_Transmissive,
		CHAN_Reflection,
		CHAN_Refraction,
		CHAN_Environment,
		CHAN_IOR,
		CU_SCATTERING0,
		CU_SCATTERING1,
		CU_SCATTERING2,
		CU_SCATTERING3,
		CHAN_Metallic,
		CHAN_BaseColor,
		CHAN_MAX
	};

	typedef std::vector<uint32> Uids;
}


//! @brief Pair class
template<class T, class U>
struct pair_t
{
	typedef std::pair<T, U> Type;
};


//! @brief Aligned allocator
template <class T>
struct aligned_allocator : std::allocator<T>
{
	template<class U>
	struct rebind { typedef aligned_allocator<U> other; };
	
	aligned_allocator()
	{
	}
	
	template<class Other>
	aligned_allocator(const aligned_allocator<Other>&)
	{
	}
	
	aligned_allocator(const aligned_allocator<T>&)
	{
	}

	typedef std::allocator<T> base;

	typedef typename base::pointer pointer;
	typedef typename base::size_type size_type;

	pointer allocate(size_type n)
	{
#if SUBSTANCE_MEMORY_STAT
	//	INC_MEMORY_STAT_BY(STAT_SubstanceMemory, n*sizeof(T));
#endif
		return (pointer)FMemory::Malloc(n*sizeof(T), 16);
	}

	pointer allocate(size_type n, void const*)
	{
		return this->allocate(n);
	}

	void deallocate(pointer p, size_type)
	{
#if SUBSTANCE_MEMORY_STAT
	//	DEC_MEMORY_STAT_BY(STAT_SubstanceMemory, FMemory::GetAllocSize(p));
#endif 
		return FMemory::Free(p);
	}
};


struct FSubstanceIntVector2
{
	int32 X, Y;
	FSubstanceIntVector2()
		:	X( 0 )
		,	Y( 0 )
	{}
	FSubstanceIntVector2( int32 InX, int32 InY )
		:	X( InX )
		,	Y( InY )
	{}
	static FSubstanceIntVector2 ZeroValue()
	{
		return FSubstanceIntVector2(0,0);
	}
	const int32& operator()( int32 i ) const
	{
		return (&X)[i];
	}
	int32& operator()( int32 i )
	{
		return (&X)[i];
	}
	const int32& operator[]( int32 i ) const
	{
		return (&X)[i];
	}
	int32& operator[]( int32 i )
	{
		return (&X)[i];
	}
	static int32 Num()
	{
		return 2;
	}
	bool operator==( const FSubstanceIntVector2& Other ) const
	{
		return X==Other.X && Y==Other.Y;
	}
	bool operator!=( const FSubstanceIntVector2& Other ) const
	{
		return X!=Other.X || Y!=Other.Y;
	}
	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FSubstanceIntVector2& V )
	{
		return Ar << V.X << V.Y;
	}
};


struct FSubstanceIntVector3
{
	int32 X, Y, Z;
	FSubstanceIntVector3()
		:	X( 0 )
		,	Y( 0 )
		,	Z( 0 )
	{}
	FSubstanceIntVector3( int32 InX, int32 InY, int32 InZ )
		:	X( InX )
		,	Y( InY )
		,	Z( InZ )
	{}
	static FSubstanceIntVector3 ZeroValue()
	{
		return FSubstanceIntVector3(0,0,0);
	}
	const int32& operator()( int32 i ) const
	{
		return (&X)[i];
	}
	int32& operator()( int32 i )
	{
		return (&X)[i];
	}
	int32& operator[]( int32 i )
	{
		return (&X)[i];
	}
	const int32& operator[]( int32 i ) const
	{
		return (&X)[i];
	}
	static int32 Num()
	{
		return 3;
	}
	bool operator==( const FSubstanceIntVector3& Other ) const
	{
		return X==Other.X && Y==Other.Y && Z==Other.Z;
	}
	bool operator!=( const FSubstanceIntVector3& Other ) const
	{
		return X!=Other.X || Y!=Other.Y || Z!=Other.Z;
	}
	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FSubstanceIntVector3& V )
	{
		return Ar << V.X << V.Y << V.Z;
	}
};


struct FSubstanceIntVector4
{
	int32 X, Y, Z, W;
	FSubstanceIntVector4()
		:	X( 0 )
		,	Y( 0 )
		,	Z( 0 )
		,	W( 0 )
	{}
	FSubstanceIntVector4( int32 InX, int32 InY, int32 InZ, int32 InW )
		:	X( InX )
		,	Y( InY )
		,	Z( InZ )
		,	W( InW )
	{}
	static FSubstanceIntVector4 ZeroValue()
	{
		return FSubstanceIntVector4(0,0,0,0);
	}
	const int32& operator()( int32 i ) const
	{
		return (&X)[i];
	}
	int32& operator()( int32 i )
	{
		return (&X)[i];
	}
	int32& operator[]( int32 i )
	{
		return (&X)[i];
	}
	const int32& operator[]( int32 i ) const 
	{
		return (&X)[i];
	}
	static int32 Num()
	{
		return 4;
	}
	bool operator==( const FSubstanceIntVector4& Other ) const
	{
		return X==Other.X && Y==Other.Y && Z==Other.Z && W==Other.W;
	}
	bool operator!=( const FSubstanceIntVector4& Other ) const
	{
		return X!=Other.X || Y!=Other.Y || Z!=Other.Z || W!=Other.W;
	}
	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FSubstanceIntVector4& V )
	{
		return Ar << V.X << V.Y << V.Z << V.W;
	}
};

typedef FGuid substanceGuid_t;

struct guid_t_comp {
	bool operator() (const substanceGuid_t& lhs, const substanceGuid_t& rhs) const
	{
		return GetTypeHash(lhs)<GetTypeHash(rhs);
	}
};

typedef FString input_hash_t;
typedef TArray<uint8> binary_t;

//! @brief Only used during runtime to store sbsbin
typedef std::vector< unsigned char, aligned_allocator < unsigned char > > aligned_binary_t;

typedef FVector2D vec2float_t;
typedef FVector vec3float_t;
typedef FVector4 vec4float_t;

//! @brief Using float vector for the moment
typedef FSubstanceIntVector2 vec2int_t;
typedef FSubstanceIntVector3 vec3int_t;
typedef FSubstanceIntVector4 vec4int_t;

typedef struct Substance::FGraphDesc		graph_desc_t;
typedef struct Substance::FGraphInstance	graph_inst_t;

typedef struct Substance::FOutputDesc		output_desc_t;
typedef struct Substance::FInputDescBase	input_desc_t;

typedef TSharedPtr<input_desc_t> input_desc_ptr;

typedef struct Substance::FOutputInstance		output_inst_t;
typedef struct Substance::FInputInstanceBase	input_inst_t;
typedef struct Substance::FNumericalInputInstanceBase num_input_inst_t;
typedef struct Substance::FImageInputInstance	img_input_inst_t;

typedef struct Substance::FPackage package_t;

typedef struct Substance::FPreset preset_t;
//! @brief Container of presets
typedef TArray<preset_t> presets_t;

typedef TSharedPtr< input_inst_t > input_inst_t_ptr;

//! @brief Serialization functions for the UE4
FArchive& operator<<(FArchive& Ar, TSharedPtr< input_inst_t >& I);
FArchive& operator<<(FArchive& Ar, output_inst_t& O);
FArchive& operator<<(FArchive& Ar, graph_inst_t& G);
FArchive& operator<<(FArchive& Ar, output_desc_t& O);
FArchive& operator<<(FArchive& Ar, TSharedPtr< input_desc_t >& I);
FArchive& operator<<(FArchive& Ar, package_t*& P);
FArchive& operator<<(FArchive& Ar, graph_desc_t*& G);
FArchive& operator<<(FArchive& Ar, graph_desc_t& G);

 #define _LINENAME_CONCAT( _name_, _line_ ) _name_##_line_
 #define _LINENAME(_name_, _line_) _LINENAME_CONCAT(_name_,_line_)
 #define _UNIQUE_VAR(_name_) _LINENAME(_name_,__LINE__)

 /* This is a lightweight foreach loop */
 #define SBS_VECTOR_FOREACH(var, vec) \
	 for (size_t _UNIQUE_VAR(__i)=0, _UNIQUE_VAR(__j)=0; _UNIQUE_VAR(__i)<vec.size(); _UNIQUE_VAR(__i)++) for (var = vec[_UNIQUE_VAR(__i)]; _UNIQUE_VAR(__j)==_UNIQUE_VAR(__i); _UNIQUE_VAR(__j)++)

 #define SBS_VECTOR_REVERSE_FOREACH(var, vec) \
	 for (size_t _UNIQUE_VAR(__i)=vec.size(), _UNIQUE_VAR(__j)=vec.size(); _UNIQUE_VAR(__i)!=0; _UNIQUE_VAR(__i)--) for (var = vec[_UNIQUE_VAR(__i)-1]; _UNIQUE_VAR(__j)==_UNIQUE_VAR(__i); _UNIQUE_VAR(__j)--)
