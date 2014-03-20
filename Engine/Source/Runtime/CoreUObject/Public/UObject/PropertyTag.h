// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*-----------------------------------------------------------------------------
	FPropertyTag.
-----------------------------------------------------------------------------*/

#ifndef __UNPROPERTYTAG_H__
#define __UNPROPERTYTAG_H__

/**
 *  A tag describing a class property, to aid in serialization.
 */
struct FPropertyTag
{
	// Variables.
	FName	Type;		// Type of property
	uint8	BoolVal;	// a boolean property's value (never need to serialize data for bool properties except here)
	FName	Name;		// Name of property.
	FName	StructName;	// Struct name if UStructProperty.
	FName	EnumName;	// Enum name if UByteProperty
	FName InnerType; // Inner type if UArrayProperty
	int32		Size;       // Property size.
	int32		ArrayIndex;	// Index if an array; else 0.
	int32		SizeOffset;	// location in stream of tag size member

	// Constructors.
	FPropertyTag()
	{}
	FPropertyTag( FArchive& InSaveAr, UProperty* Property, int32 InIndex, uint8* Value, uint8* Defaults )
	:	Type		(Property->GetID())
	, BoolVal	(0)
	,	Name		(Property->GetFName())
	,	StructName	(NAME_None)
	,	EnumName	(NAME_None)
	, InnerType	(NAME_None)
	,	Size		(0)
	,	ArrayIndex	(InIndex)
	,	SizeOffset	(INDEX_NONE)
	{
		if (Property)
		{
			// Handle structs.
			if (Property->IsA(UStructProperty::StaticClass()))
			{
				UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);
				StructName = StructProperty->Struct->GetFName();
			}
			else if (Property->GetClass() == UByteProperty::StaticClass())
			{
				UByteProperty* ByteProp = CastChecked<UByteProperty>(Property);
				if (ByteProp->Enum != NULL)
				{
					EnumName = ByteProp->Enum->GetFName();
				}
			}
			else if (Property->IsA(UArrayProperty::StaticClass()))
			{
				UArrayProperty* ArrayProp = CastChecked<UArrayProperty>(Property);
				InnerType = ArrayProp->Inner->GetID();
			}
			else if (Property->IsA(UBoolProperty::StaticClass()))
			{
				UBoolProperty* Bool = CastChecked<UBoolProperty>(Property);
				BoolVal = Bool->GetPropertyValue(Value);
			}
		}
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPropertyTag& Tag )
	{
		// Name.
		Ar << Tag.Name;
		if( Tag.Name == NAME_None )
		{
			return Ar;
		}

		Ar << Tag.Type;
		if ( Ar.IsSaving() )
		{
			// remember the offset of the Size variable - UStruct::SerializeTaggedProperties will update it after the
			// property has been serialized.
			Tag.SizeOffset = Ar.Tell();
		}
		Ar << Tag.Size << Tag.ArrayIndex;

		// only need to serialize this for structs
		if (Tag.Type == NAME_StructProperty)
		{
			Ar << Tag.StructName;
		}
		// only need to serialize this for bools
		else if (Tag.Type == NAME_BoolProperty)
		{
			Ar << Tag.BoolVal;
		}
		// only need to serialize this for bytes
		else if (Tag.Type == NAME_ByteProperty)
		{
			Ar << Tag.EnumName;
		}
		// only need to serialize this for arrays
		else if (Tag.Type == NAME_ArrayProperty)
		{
			if (Ar.UE4Ver() >= VAR_UE4_ARRAY_PROPERTY_INNER_TAGS)
			{
				Ar << Tag.InnerType;
			}
		}

		return Ar;
	}

	// Property serializer.
	void SerializeTaggedProperty( FArchive& Ar, UProperty* Property, uint8* Value, int32 MaxReadBytes, uint8* Defaults )
	{
		if (Property->GetClass() == UBoolProperty::StaticClass())
		{
			UBoolProperty* Bool = (UBoolProperty*)Property;
			if (Ar.IsLoading())
			{
				Bool->SetPropertyValue(Value, BoolVal != 0);
			}
		}
		else
		{
			UProperty* OldSerializedProperty = GSerializedProperty;
			GSerializedProperty = Property;

			Property->SerializeItem( Ar, Value, MaxReadBytes, Defaults );

			GSerializedProperty = OldSerializedProperty;
		}
	}
};

#endif	// __UNPROPERTYTAG_H__
