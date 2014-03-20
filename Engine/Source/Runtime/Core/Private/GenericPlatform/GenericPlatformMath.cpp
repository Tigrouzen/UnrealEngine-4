// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericPlatformMath.cpp: Generic implementations of Math platform functions
=============================================================================*/

#include "CorePrivate.h"
#include "BigInt.h"

static int32 GSRandSeed;

void FGenericPlatformMath::SRandInit( int32 Seed ) 
{
	GSRandSeed = Seed; 
}

float FGenericPlatformMath::SRand() 
{ 
	GSRandSeed = (GSRandSeed * 196314165) + 907633515;
	union { float f; int32 i; } Result;
	union { float f; int32 i; } Temp;
	const float SRandTemp = 1.0f;
	Temp.f = SRandTemp;
	Result.i = (Temp.i & 0xff800000) | (GSRandSeed & 0x007fffff);
	return FPlatformMath::Fractional( Result.f );
} 

extern float TheCompilerDoesntKnowThisIsAlwaysZero;

void FGenericPlatformMath::AutoTest() 
{
	check(IsNaN(sqrtf(-1.0f)));
	check(!IsFinite(sqrtf(-1.0f)));
	check(!IsFinite(-1.0f/TheCompilerDoesntKnowThisIsAlwaysZero));
	check(!IsFinite(1.0f/TheCompilerDoesntKnowThisIsAlwaysZero));
	check(!IsNaN(-1.0f/TheCompilerDoesntKnowThisIsAlwaysZero));
	check(!IsNaN(1.0f/TheCompilerDoesntKnowThisIsAlwaysZero));
	check(!IsNaN(MAX_FLT));
	check(IsFinite(MAX_FLT));
	check(!IsNaN(0.0f));
	check(IsFinite(0.0f));
	check(!IsNaN(1.0f));
	check(IsFinite(1.0f));
	check(!IsNaN(-1.e37f));
	check(IsFinite(-1.e37f));
	check(FloorLog2(0) == 0);
	check(FloorLog2(1) == 0);
	check(FloorLog2(2) == 1);
	check(FloorLog2(12) == 3);
	check(FloorLog2(16) == 4);

	{
		// Shift test
		const uint32 ShiftValue[] = { 0xCACACAC2U, 0x1U, 0x0U, 0x0U, 0x0U, 0x0U, 0x0U, 0x0U };
		int256 TestValue(ShiftValue);
		int256 Shift(TestValue);
		Shift <<= 88;
		Shift >>= 88;
		check(Shift == TestValue);
	}

	{
		int256 Dividend(3806401LL);
		int256 Divisor(3233LL);
		int256 Remainder;
		Dividend.DivideWithRemainder(Divisor, Remainder);
		check(Dividend.ToInt() == 1177LL);
		check(Remainder.ToInt() == 1160LL);	
	}

	{
		// Division test: 4294967296LL / 897LL = 4788146LL, R = 334LL
		int256 Dividend(4294967296LL);
		int256 Divisor(897LL);
		int256 Remainder;
		Dividend.DivideWithRemainder(Divisor, Remainder);
		check(Dividend.ToInt() == 4788146LL);
		check(Remainder.ToInt() == 334LL);
	}
}
