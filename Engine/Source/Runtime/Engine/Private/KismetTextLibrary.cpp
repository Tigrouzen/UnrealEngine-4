// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "EngineKismetLibraryClasses.h"

//////////////////////////////////////////////////////////////////////////
// UKismetTextLibrary

UKismetTextLibrary::UKismetTextLibrary(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FString UKismetTextLibrary::Conv_TextToString(const FText& InText)
{
	return InText.ToString();
}

FText UKismetTextLibrary::Conv_StringToText(const FString& InString)
{	
	return FText::FromString(InString);
}

FText UKismetTextLibrary::Conv_NameToText(FName InName)
{
	return FText::FromName(InName);
}

bool UKismetTextLibrary::TextIsEmpty(const FText& InText)
{
	return InText.IsEmpty();
}

bool UKismetTextLibrary::TextIsTransient(const FText& InText)
{
	return InText.IsTransient();
}

bool UKismetTextLibrary::TextIsCultureInvariant(const FText& InText)
{
	return InText.IsCultureInvariant();
}

FText UKismetTextLibrary::TextTrimPreceding(const FText& InText)
{
	return FText::TrimPreceding(InText);
}

FText UKismetTextLibrary::TextTrimTrailing(const FText& InText)
{
	return FText::TrimTrailing(InText);
}

FText UKismetTextLibrary::TextTrimPrecedingAndTrailing(const FText& InText)
{
	return FText::TrimPrecedingAndTrailing(InText);
}

FText UKismetTextLibrary::GetEmptyText()
{
	return FText::GetEmpty();
}

bool UKismetTextLibrary::FindTextInLocalizationTable(const FString& Namespace, const FString& Key, FText& OutText)
{
	return FText::FindText(Namespace, Key, OutText);
}

bool UKismetTextLibrary::EqualEqual_IgnoreCase_TextText(const FText& A, const FText& B)
{
	return A.EqualToCaseIgnored( B );
}

bool UKismetTextLibrary::EqualEqual_TextText(const FText& A, const FText& B)
{
	return A.EqualTo( B );
}

bool UKismetTextLibrary::NotEqual_IgnoreCase_TextText(const FText& A, const FText& B)
{
	return !A.EqualToCaseIgnored( B );
}

bool UKismetTextLibrary::NotEqual_TextText(const FText& A, const FText& B)
{
	return !A.EqualTo( B );
}

FText UKismetTextLibrary::Conv_IntToText(int32 Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;

	return FText::AsNumber(Value, &NumberFormatOptions);
}

FText UKismetTextLibrary::Conv_FloatToText(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;

	return FText::AsNumber(Value, &NumberFormatOptions);
}

FText UKismetTextLibrary::AsCurrency_Integer(int32 Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;

	return FText::AsCurrency(Value, &NumberFormatOptions);
}

FText UKismetTextLibrary::AsCurrency_Float(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;

	return FText::AsCurrency(Value, &NumberFormatOptions);
}

FText UKismetTextLibrary::AsPercent_Float(float Value, TEnumAsByte<ERoundingMode> RoundingMode, bool bUseGrouping/* = true*/, int32 MinimumIntegralDigits/* = 1*/, int32 MaximumIntegralDigits/* = 324*/, int32 MinimumFractionalDigits/* = 0*/, int32 MaximumFractionalDigits/* = 3*/)
{
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.UseGrouping = bUseGrouping;
	NumberFormatOptions.RoundingMode = RoundingMode;
	NumberFormatOptions.MinimumIntegralDigits = MinimumIntegralDigits;
	NumberFormatOptions.MaximumIntegralDigits = MaximumIntegralDigits;
	NumberFormatOptions.MinimumFractionalDigits = MinimumFractionalDigits;
	NumberFormatOptions.MaximumFractionalDigits = MaximumFractionalDigits;

	return FText::AsPercent(Value, &NumberFormatOptions);
}