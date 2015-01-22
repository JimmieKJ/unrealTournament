/************************************************************************************
 
PublicHeader:   OVR.h
Filename    :   OVR_TypesafeNumber.cpp
Content     :   Unit test for typesafe number template.
Created     :   March 2, 2014
Authors     :   Jonathan E. wright (nelno@oculusvr.com)
 
Copyright   :   Copyright 2014 Oculus, LLC. All Rights reserved.
 
************************************************************************************/

#include <assert.h>
#include <math.h>
#include "OVR_TypeSafeNumber.h"

namespace OVR {

// uncomment for unit testing
// #define ENABLE_UNIT_TEST

#if defined( ENABLE_UNIT_TEST )
class UnitTest_TypesafeNumberT 
{
public:
    enum UniqueType1Enum
    {
        UNIQUETYPE1_INITIAL_VALUE = 0
    };

    typedef TypesafeNumberT< int, UniqueType1Enum, UNIQUETYPE1_INITIAL_VALUE > UniqueType1;

    enum UniqueType2Enum
    {
        UNIQUETYPE2_INITIAL_VALUE = 1
    };

    typedef TypesafeNumberT< float, UniqueType2Enum, UNIQUETYPE2_INITIAL_VALUE > UniqueType2;

    UnitTest_TypesafeNumberT() 
    {
        // construction
        UniqueType1 t1;
        assert( t1.Get() == UNIQUETYPE1_INITIAL_VALUE );

        // explicit construction
        UniqueType1 t2( 100 );
        assert( t2.Get() == 100 );
        // assignmnet
        t1 = t2;
        // prefix unary addition
        ++t1;
        assert( t1.Get() == 101 );
        // postfix unary addition
        UniqueType1 temp = t1++;
        assert( t1.Get() == 102 );
        assert( temp.Get() == 101 );
        // prefix unary subtraction
        --t1;
        assert( t1.Get() == 101 );
        // postfix unary subtraction
        temp = t1--;
        assert( t1.Get() == 100 );
        assert( temp.Get() == 101 );
        // compound addition
        t1 += t1;
        assert( t1.Get() == 200 );
        // compound subtraction
        t1 -= t1;
        assert( t1.Get() == 0 );
        // compound multiplication
        t1 = UniqueType1( 10 );
        t1 *= t1;
        assert( t1.Get() == 100 );
        // compound division
        t1 /= t1;
        assert( t1.Get() == 1 );
        // compound modulus
        t1 %= t1;
        assert( t1.Get() == 0 );
        // not equal
        if ( t1 != UniqueType1( 0 ) ) 
        {
            assert( false );
        }
        // equal
        t1 = t2;
        if ( !( t1 == t2 ) )
        {
            assert( false );
        }
        UniqueType1 t3;
        t1 = UniqueType1( 5 );
        t2 = UniqueType1( 100 );
        // binary addition
        t3 = t1 + t2;
        assert( t3 == UniqueType1( 105 ) );
        // binary subtraction
        t3 = t2 - t1;
        assert( t3 == UniqueType1( 95 ) );
        // binary multiplication
        t3 = t2 * t1;
        assert( t3 == UniqueType1( 500 ) );
        // binary division
        t3 = t2 / t1;
        assert( t3 == UniqueType1( 20 ) );
        // binary modulus
        t3 = t1 % t2;
        assert( t3 == UniqueType1( 5 ) );

        // greater than
        t1 = UniqueType1( -100 );
        t2 = UniqueType1( 100 );
        t3 = UniqueType1( 100 );

        assert( t1 < t2 );
        assert( t2 > t1 );
        assert( t3 <= t2 );
        assert( t3 >= t2 );
        assert( t1 <= t2 );
        assert( t2 >= t1 );

        UniqueType2 otherType1( 1.0f );
        UniqueType2 otherType2( 10.0f );

        otherType1 += otherType2;
        assert( fabs( ( otherType1 - UniqueType2( 11.0f ) ).Get() ) < 0.001f );
#if 0        
        t1 = otherType; // this should fail on compile
#endif
    }
};

static UnitTest_TypesafeNumberT UnitTestTypesafeNumber;
#endif  // ENABLE_UNIT_TEST

}