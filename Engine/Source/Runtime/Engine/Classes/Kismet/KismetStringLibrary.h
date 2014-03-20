// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "KismetStringLibrary.generated.h"

UCLASS(MinimalAPI)
class UKismetStringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Converts a float value to a string */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (float)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_FloatToString(float InFloat);

	/** Converts an integer value to a string */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (int)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_IntToString(int32 InInt);

	/** Converts a byte value to a string */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (byte)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_ByteToString(uint8 InByte);

	/** Converts a boolean value to a string, either 'true' or 'false' */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (boolean)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_BoolToString(bool InBool);

	/** Converts a vector value to a string, in the form 'X= Y= Z=' */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (Vector)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_VectorToString(FVector InVec);

	/** Converts a vector2d value to a string, in the form 'X= Y=' */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (vector2d)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_Vector2dToString(FVector2D InVec);

	/** Converts a rotator value to a string, in the form 'P= Y= R=' */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (rotator)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_RotatorToString(FRotator InRot);

	/** Converts a UObject value to a string by calling the object's GetName method */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (object)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_ObjectToString(class UObject* InObj);

	/** Converts a linear color value to a string, in the form '(R=,G=,B=,A=)' */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (linear color)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_ColorToString(FLinearColor InColor);

	/** Converts a name value to a string */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "ToString (name)", CompactNodeTitle = "->"), Category="Utilities|String")
	static FString Conv_NameToString(FName InName);

	/** Converts a string to a name value */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "String To Name", CompactNodeTitle = "->"), Category="Utilities|String")
	static FName Conv_StringToName(const FString& InString);

	/** Converts a string to a int value */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "String To Int", CompactNodeTitle = "->"), Category="Utilities|String")
	static int32 Conv_StringToInt(const FString& InString);

	/** Converts a string to a float value */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "String To Float", CompactNodeTitle = "->"), Category="Utilities|String")
	static float Conv_StringToFloat(const FString& InString);
	

	/** 
	 * Converts a float->string, create a new string in the form AppendTo+Prefix+InFloat+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InFloat - The float value to convert
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (float)"), Category="Utilities|String")
	static FString BuildString_Float(const FString& AppendTo, const FString& Prefix, float InFloat, const FString& Suffix);

	/** 
	 * Converts a int->string, creating a new string in the form AppendTo+Prefix+InInt+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InInt - The int value to convert
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (int)"), Category="Utilities|String")
	static FString BuildString_Int(const FString& AppendTo, const FString& Prefix, int32 InInt, const FString& Suffix);

	/** 
	 * Converts a boolean->string, creating a new string in the form AppendTo+Prefix+InBool+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InBool - The bool value to convert. Will add "true" or "false" to the conversion string
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (boolean)"), Category="Utilities|String")
	static FString BuildString_Bool(const FString& AppendTo, const FString& Prefix, bool InBool, const FString& Suffix);

	/** 
	 * Converts a vector->string, creating a new string in the form AppendTo+Prefix+InVector+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InVector - The vector value to convert. Uses the standard FVector::ToString conversion
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (vector)"), Category="Utilities|String")
	static FString BuildString_Vector(const FString& AppendTo, const FString& Prefix, FVector InVector, const FString& Suffix);

	/** 
	 * Converts a vector2d->string, creating a new string in the form AppendTo+Prefix+InVector2d+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InVector2d - The vector2d value to convert. Uses the standard FVector2D::ToString conversion
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (vector2d)"), Category="Utilities|String")
	static FString BuildString_Vector2d(const FString& AppendTo, const FString& Prefix, FVector2D InVector2d, const FString& Suffix);

	/** 
	 * Converts a rotator->string, creating a new string in the form AppendTo+Prefix+InRot+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InRot	- The rotator value to convert. Uses the standard ToString conversion
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (rotator)"), Category="Utilities|String")
	static FString BuildString_Rotator(const FString& AppendTo, const FString& Prefix, FRotator InRot, const FString& Suffix);

	/** 
	 * Converts a object->string, creating a new string in the form AppendTo+Prefix+object name+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InObj - The object to convert. Will insert the name of the object into the conversion string
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (object)"), Category="Utilities|String")
	static FString BuildString_Object(const FString& AppendTo, const FString& Prefix, class UObject* InObj, const FString& Suffix);

	/** 
	 * Converts a color->string, creating a new string in the form AppendTo+Prefix+InColor+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InColor - The linear color value to convert. Uses the standard ToString conversion
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (color)"), Category="Utilities|String")
	static FString BuildString_Color(const FString& AppendTo, const FString& Prefix, FLinearColor InColor, const FString& Suffix);

	/** 
	 * Converts a color->string, creating a new string in the form AppendTo+Prefix+InName+Suffix
	 * @param AppendTo - An existing string to use as the start of the conversion string
	 * @param Prefix - A string to use as a prefix, after the AppendTo string
	 * @param InName - The name value to convert
	 * @param Suffix - A suffix to append to the end of the conversion string
	 * @return A new string built from the passed parameters
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "BuildString (name)"), Category="Utilities|String")
	static FString BuildString_Name(const FString& AppendTo, const FString& Prefix, FName InName, const FString& Suffix);


	//
	// String functions.
	//
	
	/**
	 * Concatenates two strings together to make a new string
	 * @param A - The original string
	 * @param B - The string to append to A
	 * @returns A new string which is the concatenation of A+B
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Append"), Category="Utilities|String")
	static FString Concat_StrStr(const FString& A, const FString& B);

	/**
	 * Test if the input strings are equal (A == B)
	 * @param A - The string to compare against
	 * @param B - The string to compare
	 * @returns True if the strings are equal, false otherwise
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal (string)", CompactNodeTitle = "=="), Category="Utilities|String")
	static bool EqualEqual_StrStr(const FString& A, const FString& B);

	/**
	 * Test if the input strings are equal (A == B), ignoring case
	 * @param A - The string to compare against
	 * @param B - The string to compare
	 * @returns True if the strings are equal, false otherwise
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "Equal, Case Insensitive (string)", CompactNodeTitle = "=="), Category="Utilities|String")
	static bool EqualEqual_StriStri(const FString& A, const FString& B);

	/** 
	 * Test if the input string are not equal (A != B)
	 * @param A - The string to compare against
	 * @param B - The string to compare
	 * @return Returns true if the input strings are not equal, false if they are equal
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual (string)", CompactNodeTitle = "!="), Category="Utilities|String")
	static bool NotEqual_StrStr(const FString& A, const FString& B);

	/** Test if the input string are not equal (A != B), ignoring case differences
	 * @param A - The string to compare against
	 * @param B - The string to compare
	 * @return Returns true if the input strings are not equal, false if they are equal
	 */
	UFUNCTION(BlueprintPure, meta=(FriendlyName = "NotEqual, case insenstive (string)", CompactNodeTitle = "!="), Category="Utilities|String")
	static bool NotEqual_StriStri(const FString& A, const FString& B);

	/** 
	 * Returns the number of characters in the string
	 * @param SourceString - The string to measure
	 * @return The number of chars in the string
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|String", meta=(CompactNodeTitle = "LEN", Keywords = "length"))
	static int32 Len(const FString& S);

	/** 
	 * Returns a substring from the string starting at the specified position
	 * @param SourceString - The string to get the substring from
	 * @param StartIndex - The location in SourceString to use as the start of the substring
	 * @param Length The length of the requested substring
	 *
	 * @return The requested substring
	 */
	UFUNCTION(BlueprintPure, meta=(StartIndex="0", Length="1"), Category="Utilities|String")
	static FString GetSubstring(const FString& SourceString, int32 StartIndex, int32 Length);

	/** 
	 * Gets a single character from the string (as an integer)
	 * @param SourceString - The string to convert
	 * @param Index - Location of the character whose value is required
	 * @return The integer value of the character or 0 if index is out of range
	 */
	UFUNCTION(BlueprintPure, meta=(Index="0"), Category="Utilities|String")
	static int32 GetCharacterAsNumber(const FString& SourceString, int32 Index);
};
