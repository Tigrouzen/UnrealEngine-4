// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	KismetCompilerMisc.cpp
=============================================================================*/

#include "KismetCompilerPrivatePCH.h"
#include "KismetCompilerMisc.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetDebugUtilities.h"
#include "Editor/UnrealEd/Public/Kismet2/StructureEditorUtils.h"
#include "Editor/UnrealEd/Public/ObjectTools.h"
#include "DefaultValueHelper.h"

#define LOCTEXT_NAMESPACE "KismetCompiler"

//////////////////////////////////////////////////////////////////////////
// FKismetCompilerUtilities

/** Tests to see if a pin is schema compatible with a property */
bool FKismetCompilerUtilities::IsTypeCompatibleWithProperty(UEdGraphPin* SourcePin, UProperty* Property, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass)
{
	check(SourcePin != NULL);
	const FEdGraphPinType& Type = SourcePin->PinType;
	const EEdGraphPinDirection Direction = SourcePin->Direction; 

	const FString& PinCategory = Type.PinCategory;
	const FString& PinSubCategory = Type.PinSubCategory;
	const UObject* PinSubCategoryObject = Type.PinSubCategoryObject.Get();

	UProperty* TestProperty = NULL;
	const UFunction* OwningFunction = Cast<UFunction>(Property->GetOuter());
	if( Type.bIsArray )
	{
		// For arrays, the property we want to test against is the inner property
		if( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property) )
		{
			if(OwningFunction)
			{
				// Check for the magic ArrayParm property, which always matches array types
				FString ArrayPointerMetaData = OwningFunction->GetMetaData(TEXT("ArrayParm"));
				TArray<FString> ArrayPinComboNames;
				ArrayPointerMetaData.ParseIntoArray(&ArrayPinComboNames, TEXT(","), true);

				for(auto Iter = ArrayPinComboNames.CreateConstIterator(); Iter; ++Iter)
				{
					TArray<FString> ArrayPinNames;
					Iter->ParseIntoArray(&ArrayPinNames, TEXT("|"), true);

					if( ArrayPinNames[0] == SourcePin->PinName )
					{
						return true;
					}
				}
			}

			TestProperty = ArrayProp->Inner;
		}
		else
		{
			MessageLog.Error(*LOCTEXT("PinSpecifiedAsArray_Error", "Pin @@ is specified as an array, but does not have a valid array property.").ToString(), SourcePin);
			return false;
		}
	}
	else
	{
		// For scalars, we just take the passed in property
		TestProperty = Property;
	}

	// Check for the early out...if this is a type dependent parameter in an array function
	if ( (OwningFunction != NULL) && OwningFunction->HasMetaData(TEXT("ArrayParm")) )
	{
		// Check to see if this param is type dependent on an array parameter
		const FString DependentParams = OwningFunction->GetMetaData(TEXT("ArrayTypeDependentParams"));
		TArray<FString>	DependentParamNames;
		DependentParams.ParseIntoArray(&DependentParamNames, TEXT(","), true);
		if (DependentParamNames.Find(SourcePin->PinName) != INDEX_NONE)
		{
			//@todo:  This assumes that the wildcard coersion has done its job...I'd feel better if there was some easier way of accessing the target array type
			return true;
		}
	}

	int32 NumErrorsAtStart = MessageLog.NumErrors;

	// First check the type
	bool bTypeMismatch = false;
	bool bSubtypeMismatch = false;
	FString DesiredSubType(TEXT(""));

	if (PinCategory == Schema->PC_Boolean)
	{
		UBoolProperty* SpecificProperty = Cast<UBoolProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Byte)
	{
		UByteProperty* SpecificProperty = Cast<UByteProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Class)
	{
		const UClass* ClassType = (PinSubCategory == Schema->PSC_Self) ? SelfClass : Cast<const UClass>(PinSubCategoryObject);

		if (ClassType == NULL)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("FindClassForPin_Error", "Failed to find class for pin @@").ToString()), SourcePin);
		}
		else
		{
			const UClass* MetaClass = NULL;
			if (auto ClassProperty = Cast<UClassProperty>(TestProperty))
			{
				MetaClass = ClassProperty->MetaClass;
			}
			else if (auto AssetClassProperty = Cast<UAssetClassProperty>(TestProperty))
			{
				MetaClass = AssetClassProperty->MetaClass;
			}

			if (MetaClass != NULL)
			{
				DesiredSubType = MetaClass->GetName();

				const UClass* OutputClass = (Direction == EGPD_Output) ? ClassType :  MetaClass;
				const UClass* InputClass = (Direction == EGPD_Output) ? MetaClass : ClassType;

				// It matches if it's an exact match or if the output class is more derived than the input class
				bTypeMismatch = bSubtypeMismatch = !((OutputClass == InputClass) || (OutputClass->IsChildOf(InputClass)));
			}
			else
			{
				bTypeMismatch = true;
			}
		}
	}
	else if (PinCategory == Schema->PC_Float)
	{
		UFloatProperty* SpecificProperty = Cast<UFloatProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Int)
	{
		UIntProperty* SpecificProperty = Cast<UIntProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Name)
	{
		UNameProperty* SpecificProperty = Cast<UNameProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Delegate)
	{
		const UFunction* SignatureFunction = Cast<const UFunction>(PinSubCategoryObject);
		const UDelegateProperty* PropertyDelegate = Cast<const UDelegateProperty>(TestProperty);
		bTypeMismatch = !(SignatureFunction 
			&& PropertyDelegate 
			&& PropertyDelegate->SignatureFunction 
			&& PropertyDelegate->SignatureFunction->IsSignatureCompatibleWith(SignatureFunction));
	}
	else if (PinCategory == Schema->PC_Object)
	{
		const UClass* ObjectType = (PinSubCategory == Schema->PSC_Self) ? SelfClass : Cast<const UClass>(PinSubCategoryObject);

		if (ObjectType == NULL)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("FindClassForPin_Error", "Failed to find class for pin @@").ToString()), SourcePin);
		}
		else
		{
			UObjectPropertyBase* ObjProperty = Cast<UObjectPropertyBase>(TestProperty);
			if (ObjProperty != NULL && ObjProperty->PropertyClass)
			{
				DesiredSubType = ObjProperty->PropertyClass->GetName();

				const UClass* OutputClass = (Direction == EGPD_Output) ? ObjectType : ObjProperty->PropertyClass;
				const UClass* InputClass = (Direction == EGPD_Output) ? ObjProperty->PropertyClass : ObjectType;

				// It matches if it's an exact match or if the output class is more derived than the input class
				bTypeMismatch = bSubtypeMismatch = !((OutputClass == InputClass) || (OutputClass->IsChildOf(InputClass)));
			}
			else if (UInterfaceProperty* IntefaceProperty = Cast<UInterfaceProperty>(TestProperty))
			{
				UClass const* InterfaceClass = IntefaceProperty->InterfaceClass;
				if (InterfaceClass == NULL)
				{
					bTypeMismatch = true;
				}
				else 
				{
					DesiredSubType = InterfaceClass->GetName();
					bTypeMismatch = ObjectType->ImplementsInterface(InterfaceClass);
				}
			}
			else
			{
				bTypeMismatch = true;
			}
		}
	}
	else if (PinCategory == Schema->PC_String)
	{
		UStrProperty* SpecificProperty = Cast<UStrProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Text)
	{
		UTextProperty* SpecificProperty = Cast<UTextProperty>(TestProperty);
		bTypeMismatch = (SpecificProperty == NULL);
	}
	else if (PinCategory == Schema->PC_Struct)
	{
		const UScriptStruct* StructType = Cast<const UScriptStruct>(PinSubCategoryObject);
		if (StructType == NULL)
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("FindStructForPin_Error", "Failed to find struct for pin @@").ToString()), SourcePin);
		}
		else
		{
			UStructProperty* StructProperty = Cast<UStructProperty>(TestProperty);
			if (StructProperty != NULL)
			{
				DesiredSubType = StructProperty->Struct->GetName();
				bSubtypeMismatch = bTypeMismatch = (StructType != StructProperty->Struct);
			}
			else
			{
				bTypeMismatch = true;
			}
		}
	}
	else
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("UnsupportedTypeForPin", "Unsupported type (%s) on @@").ToString(), *UEdGraphSchema_K2::TypeToString(Type)), SourcePin);
	}

	if (bTypeMismatch)
	{
		MessageLog.Error(*FString::Printf(*LOCTEXT("TypeDoesNotMatchPropertyOfType_Error", "@@ of type %s doesn't match the property %s of type %s").ToString(),
			*UEdGraphSchema_K2::TypeToString(Type),
			*Property->GetName(),
			*UEdGraphSchema_K2::TypeToString(Property)),
			SourcePin);
	}

	// Now check the direction
	if (Property->HasAnyPropertyFlags(CPF_Parm))
	{
		// Parameters are directional
		const bool bOutParam = (Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm) && !(Property->HasAnyPropertyFlags(CPF_ReferenceParm)));

		if ( ((SourcePin->Direction == EGPD_Input) && bOutParam) || ((SourcePin->Direction == EGPD_Output) && !bOutParam))
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("DirectionMismatchParameter_Error", "The direction of @@ doesn't match the direction of parameter %s").ToString(), *Property->GetName()), SourcePin);
		}

 		if (Property->HasAnyPropertyFlags(CPF_ReferenceParm))
 		{
			TArray<FString> AutoEmittedTerms;
			Schema->GetAutoEmitTermParameters(OwningFunction, AutoEmittedTerms);
			const bool bIsAutoEmittedTerm = AutoEmittedTerms.Contains(SourcePin->PinName);

			// Make sure reference parameters are linked, except for FTransforms, which have a special node handler that adds an internal constant term
 			if (!bIsAutoEmittedTerm
				&& (SourcePin->LinkedTo.Num() == 0)
				&& (!SourcePin->PinType.PinSubCategoryObject.IsValid() || SourcePin->PinType.PinSubCategoryObject.Get()->GetName() != TEXT("Transform")) )
 			{
 				MessageLog.Error(*LOCTEXT("PassLiteral_Error", "Cannot pass a literal to @@.  Connect a variable to it instead.").ToString(), SourcePin);
 			}
 		}
	}

	return NumErrorsAtStart == MessageLog.NumErrors;
}

uint32 FKismetCompilerUtilities::ConsignToOblivionCounter = 0;

// Rename a class and it's CDO into the transient package, and clear RF_Public on both of them
void FKismetCompilerUtilities::ConsignToOblivion(UClass* OldClass, bool bForceNoResetLoaders)
{
	if (OldClass != NULL)
	{
		// Use the Kismet class reinstancer to ensure that the CDO and any existing instances of this class are cleaned up!
		FBlueprintCompileReinstancer CTOResinstancer(OldClass);

		UPackage* OwnerOutermost = OldClass->GetOutermost();
		if( OldClass->ClassDefaultObject )
		{
			// rename to a temp name, move into transient package
			OldClass->ClassDefaultObject->ClearFlags(RF_Public);
			OldClass->ClassDefaultObject->SetFlags(RF_Transient);
			OldClass->ClassDefaultObject->RemoveFromRoot(); // make sure no longer in root set
		}
		
		OldClass->SetMetaData(FBlueprintMetadata::MD_IsBlueprintBase, TEXT("false"));
		OldClass->ClearFlags(RF_Public);
		OldClass->SetFlags(RF_Transient);
		OldClass->ClassFlags |= CLASS_Deprecated|CLASS_NewerVersionExists;
		OldClass->RemoveFromRoot(); // make sure no longer in root set

		// Invalidate the export for all old properties, to make sure they don't get partially reloaded and corrupt the class
		for( TFieldIterator<UProperty> It(OldClass,EFieldIteratorFlags::ExcludeSuper); It; ++It )
		{
			UProperty* Current = *It;
			InvalidatePropertyExport(Current);
		}

		for( TFieldIterator<UFunction> ItFunc(OldClass,EFieldIteratorFlags::ExcludeSuper); ItFunc; ++ItFunc )
		{
			UFunction* CurrentFunc = *ItFunc;
			ULinkerLoad::InvalidateExport(CurrentFunc);

			for( TFieldIterator<UProperty> It(CurrentFunc,EFieldIteratorFlags::ExcludeSuper); It; ++It )
			{
				UProperty* Current = *It;
				InvalidatePropertyExport(Current);
			}
		}

		const FString BaseName = FString::Printf(TEXT("DEADCLASS_%s_C_%d"), *OldClass->ClassGeneratedBy->GetName(), ConsignToOblivionCounter++);
		OldClass->Rename(*BaseName, GetTransientPackage(), (REN_DontCreateRedirectors|REN_NonTransactional|(bForceNoResetLoaders ? REN_ForceNoResetLoaders : 0)));

		// Make sure MetaData doesn't have any entries to the class we just renamed out of package
		OwnerOutermost->GetMetaData()->RemoveMetaDataOutsidePackage();
	}
}

void FKismetCompilerUtilities::InvalidatePropertyExport(UProperty* PropertyToInvalidate)
{
	// Arrays need special handling to make sure the inner property is also cleared
	UArrayProperty* ArrayProp = Cast<UArrayProperty>(PropertyToInvalidate);
 	if( ArrayProp && ArrayProp->Inner )
 	{
		ULinkerLoad::InvalidateExport(ArrayProp->Inner);
 	}

	ULinkerLoad::InvalidateExport(PropertyToInvalidate);
}

void FKismetCompilerUtilities::RemoveObjectRedirectorIfPresent(UObject* Package, const FString& NewName, UObject* ObjectBeingMovedIn)
{
	// We can rename on top of an object redirection (basically destroy the redirection and put us in its place).
	if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(StaticFindObject(UObjectRedirector::StaticClass(), Package, *NewName)))
	{
		ObjectTools::DeleteRedirector(Redirector);
		Redirector = NULL;
	}
}

void FKismetCompilerUtilities::EnsureFreeNameForNewClass(UClass* ClassToConsign, FString& ClassName, UBlueprint* Blueprint)
{
	check(Blueprint);

	UObject* OwnerOutermost = Blueprint->GetOutermost();

	// Try to find a class with the name we want to use in the scope
	UClass* AnyClassWithGoodName = (UClass*)StaticFindObject(UClass::StaticClass(), OwnerOutermost, *ClassName, false);
	if (AnyClassWithGoodName == ClassToConsign)
	{
		// Ignore it if it's the class we're already consigning anyway
		AnyClassWithGoodName = NULL;
	}

	if( ClassToConsign )
	{
		ConsignToOblivion(ClassToConsign, Blueprint->bIsRegeneratingOnLoad);
	}

	// Consign the class with the name we want to use
	if( AnyClassWithGoodName )
	{
		ConsignToOblivion(AnyClassWithGoodName, Blueprint->bIsRegeneratingOnLoad);
	}
}


/** Finds a property by name, starting in the specified scope; Validates property type and returns NULL along with emitting an error if there is a mismatch. */
UProperty* FKismetCompilerUtilities::FindPropertyInScope(UStruct* Scope, UEdGraphPin* Pin, FCompilerResultsLog& MessageLog, const UEdGraphSchema_K2* Schema, UClass* SelfClass)
{
	while (Scope != NULL)
	{
		for (TFieldIterator<UProperty> It(Scope, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			UProperty* Property = *It;

			if (Property->GetName() == Pin->PinName)
			{
				if (FKismetCompilerUtilities::IsTypeCompatibleWithProperty(Pin, Property, MessageLog, Schema, SelfClass))
				{
					return Property;
				}
				else
				{
					// Exit now, we found one with the right name but the type mismatched (and there was a type mismatch error)
					return NULL;
				}
			}
		}

		// Functions don't automatically check their class when using a field iterator
		UFunction* Function = Cast<UFunction>(Scope);
		Scope = (Function != NULL) ? Cast<UStruct>(Function->GetOuter()) : NULL;
	}

	// Couldn't find the name
	MessageLog.Error(*LOCTEXT("PropertyNotFound_Error", "The property associated with @@ could not be found").ToString(), Pin);
	return NULL;
}

// Finds a property by name, starting in the specified scope, returning NULL if it's not found
UProperty* FKismetCompilerUtilities::FindNamedPropertyInScope(UStruct* Scope, FName PropertyName)
{
	while (Scope != NULL)
	{
		for (TFieldIterator<UProperty> It(Scope, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			UProperty* Property = *It;

			// If we match by name, and var is not deprecated...
			if (Property->GetFName() == PropertyName && !Property->HasAllPropertyFlags(CPF_Deprecated))
			{
				return Property;
			}
		}

		// Functions don't automatically check their class when using a field iterator
		UFunction* Function = Cast<UFunction>(Scope);
		Scope = (Function != NULL) ? Cast<UStruct>(Function->GetOuter()) : NULL;
	}

	return NULL;
}

void FKismetCompilerUtilities::CompileDefaultProperties(UClass* Class)
{
	UObject* DefaultObject = Class->GetDefaultObject(); // Force the default object to be constructed if it isn't already
	check(DefaultObject);
}

void FKismetCompilerUtilities::LinkAddedProperty(UStruct* Structure, UProperty* NewProperty)
{
	check(NewProperty->Next == NULL);
	check(Structure->Children != NewProperty);

	NewProperty->Next = Structure->Children;
	Structure->Children = NewProperty;
}

const UFunction* FKismetCompilerUtilities::FindOverriddenImplementableEvent(const FName& EventName, const UClass * Class)
{
	const uint32 RequiredFlagMask = FUNC_Event | FUNC_BlueprintEvent | FUNC_Native;
	const uint32 RequiredFlagResult = FUNC_Event | FUNC_BlueprintEvent;

	const UFunction* FoundEvent = Class ? Class->FindFunctionByName(EventName, EIncludeSuperFlag::ExcludeSuper) : NULL;

	const bool bFlagsMatch = (NULL != FoundEvent) && (RequiredFlagResult == ( FoundEvent->FunctionFlags & RequiredFlagMask ));

	return bFlagsMatch ? FoundEvent : NULL;
}

void FKismetCompilerUtilities::ValidateEnumProperties(UObject* DefaultObject, FCompilerResultsLog& MessageLog)
{
	check(DefaultObject);
	for (TFieldIterator<UProperty> It(DefaultObject->GetClass()); It; ++It)
	{
		const UByteProperty* ByteProperty = Cast<UByteProperty>(*It);
		if(ByteProperty && !ByteProperty->HasAnyPropertyFlags(CPF_Transient))
		{
			const UEnum* Enum = ByteProperty->GetIntPropertyEnum();
			if(Enum)
			{		
				const uint8 EnumIndex = ByteProperty->GetPropertyValue_InContainer(DefaultObject);
				const int32 EnumAcceptableMax = Enum->NumEnums() - 1;
				if(EnumIndex >= EnumAcceptableMax)
				{
					MessageLog.Warning(
						*FString::Printf(
							*LOCTEXT("InvalidEnumDefaultValue_Error", "Default Enum value '%s' for class '%s' is invalid in object '%s' ").ToString(),
							*ByteProperty->GetName(),
							*DefaultObject->GetClass()->GetName(),
							*DefaultObject->GetName()
						)
					);
				}
			}
		}
	}
}

/** Creates a property named PropertyName of type PropertyType in the Scope or returns NULL if the type is unknown, but does *not* link that property in */
UProperty* FKismetCompilerUtilities::CreatePropertyOnScope(UStruct* Scope, const FName& PropertyName, const FEdGraphPinType& Type, UClass* SelfClass, uint64 PropertyFlags, const UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog)
{
	//@TODO: Check for name conflicts!

	// Properties are non-transactional as they're regenerated on every compile
	const EObjectFlags ObjectFlags = RF_Public;

	UProperty* NewProperty = NULL;
	UObject* PropertyScope = NULL;

	FName ValidatedPropertyName = PropertyName;

	// Check to see if there's already a property on this scope, and throw an internal compiler error if so
	// If this happens, it breaks the property link, which causes stack corruption and hard-to-track errors, so better to fail at this point
	{
		UProperty* ExistingProperty = FindObject<UProperty>(Scope, *PropertyName.ToString(), false);
		if( ExistingProperty )
		{
			MessageLog.Error(*FString::Printf(TEXT("Internal Compiler Error:  Duplicate property %s on scope %s"), *PropertyName.ToString(), (Scope ? *Scope->GetName() : TEXT("None"))));

			// Find a free name, so we can still create the property to make it easier to spot the duplicates, and avoid crashing
			uint32 Counter = 0;
			FString TestNameString;
			do 
			{
				TestNameString = PropertyName.ToString() + FString::Printf(TEXT("_ERROR_DUPLICATE_%d"), Counter++);
			} while (FindObject<UProperty>(Scope, *TestNameString, false) != NULL);

			ValidatedPropertyName = FName(*TestNameString);
		}
	}

	// Handle creating an array property, if necessary
	const bool bIsArrayProperty = Type.bIsArray;
	UArrayProperty* NewArrayProperty = NULL;
	if( bIsArrayProperty )
	{
		NewArrayProperty = NewNamedObject<UArrayProperty>(Scope, ValidatedPropertyName, ObjectFlags);
		PropertyScope = NewArrayProperty;
	}
	else
	{
		PropertyScope = Scope;
	}

	//@TODO: Nasty string if-else tree
	if (Type.PinCategory == Schema->PC_Object)
	{
		UClass* SubType = (Type.PinSubCategory == Schema->PSC_Self) ? SelfClass : Cast<UClass>(Type.PinSubCategoryObject.Get());

		if( SubType == NULL )
		{
			// If this is from a degenerate pin, because the object type has been removed, default this to a UObject subtype so we can make a dummy term for it to allow the compiler to continue
			SubType = UObject::StaticClass();
		}

		if (SubType != NULL)
		{
			if (SubType->HasAnyClassFlags(CLASS_Interface))
			{
				UInterfaceProperty* NewPropertyObj = NewNamedObject<UInterfaceProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				NewPropertyObj->InterfaceClass = SubType;
				NewProperty = NewPropertyObj;
			}
			else
			{
				UObjectPropertyBase* NewPropertyObj = NULL;

				if( Type.bIsWeakPointer )
				{
					NewPropertyObj = NewNamedObject<UWeakObjectProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				}
				else
				{
					NewPropertyObj = NewNamedObject<UObjectProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				}
				NewPropertyObj->PropertyClass = SubType;
				NewProperty = NewPropertyObj;
			}
		}
	}
	else if (Type.PinCategory == Schema->PC_Struct)
	{
		UScriptStruct* SubType = Cast<UScriptStruct>(Type.PinSubCategoryObject.Get());
		if (SubType != NULL)
		{
			FString StructureError;
			if (FStructureEditorUtils::EStructureError::Ok == FStructureEditorUtils::IsStructureValid(SubType, NULL, &StructureError))
			{
				UStructProperty* NewPropertyStruct = NewNamedObject<UStructProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
				NewPropertyStruct->Struct = SubType;
				NewProperty = NewPropertyStruct;
			}
			else
			{
				MessageLog.Error(
					*FString::Printf(
						*LOCTEXT("InvalidStructForField_Error", "Invalid property '%s' structure '%s' error: %s").ToString(),
						*PropertyName.ToString(),
						*SubType->GetName(),
						*StructureError
					));
			}
		}
	}
	else if (Type.PinCategory == Schema->PC_Class)
	{
		UClass* SubType = Cast<UClass>(Type.PinSubCategoryObject.Get());
		if (SubType != NULL)
		{
			UClassProperty* NewPropertyClass = NewNamedObject<UClassProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewPropertyClass->MetaClass = SubType;
			NewPropertyClass->PropertyClass = UClass::StaticClass();
			NewProperty = NewPropertyClass;
		}
	}
	else if (Type.PinCategory == Schema->PC_Delegate)
	{
		if (UFunction* SignatureFunction = Cast<UFunction>(Type.PinSubCategoryObject.Get()))
		{
			UDelegateProperty* NewPropertyDelegate = NewNamedObject<UDelegateProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
			NewPropertyDelegate->SignatureFunction = SignatureFunction;
			NewProperty = NewPropertyDelegate;
		}
	}
	else if (Type.PinCategory == Schema->PC_MCDelegate)
	{
		UFunction* const SignatureFunction = Cast<UFunction>(Type.PinSubCategoryObject.Get());
		UMulticastDelegateProperty* NewPropertyDelegate = NewNamedObject<UMulticastDelegateProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		NewPropertyDelegate->SignatureFunction = SignatureFunction;
		NewProperty = NewPropertyDelegate;
	}
	else if (Type.PinCategory == Schema->PC_Int)
	{
		NewProperty = NewNamedObject<UIntProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
	}
	else if (Type.PinCategory == Schema->PC_Float)
	{
		NewProperty = NewNamedObject<UFloatProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
	}
	else if (Type.PinCategory == Schema->PC_Boolean)
	{
		UBoolProperty* BoolProperty = NewNamedObject<UBoolProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		BoolProperty->SetBoolSize(sizeof(bool), true);
		NewProperty = BoolProperty;
	}
	else if (Type.PinCategory == Schema->PC_String)
	{
		NewProperty = NewNamedObject<UStrProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
	}
	else if (Type.PinCategory == Schema->PC_Text)
	{
		NewProperty = NewNamedObject<UTextProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
	}
	else if (Type.PinCategory == Schema->PC_Byte)
	{
		UByteProperty* ByteProp = NewNamedObject<UByteProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
		ByteProp->Enum = Cast<UEnum>(Type.PinSubCategoryObject.Get());

		NewProperty = ByteProp;
	}
	else if (Type.PinCategory == Schema->PC_Name)
	{
		NewProperty = NewNamedObject<UNameProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
	}
	else
	{
		// Failed to resolve the type-subtype, create a generic property to survive VM bytecode emission
		NewProperty = NewNamedObject<UIntProperty>(PropertyScope, ValidatedPropertyName, ObjectFlags);
	}

	if( bIsArrayProperty )
	{
		// Fix up the array property to have the new type-specific property as its inner, and return the new UArrayProperty
		NewArrayProperty->Inner = NewProperty;
		NewProperty = NewArrayProperty;
	}

	return NewProperty;
}


//////////////////////////////////////////////////////////////////////////
// FNodeHandlingFunctor

void FNodeHandlingFunctor::ResolveAndRegisterScopedTerm(FKismetFunctionContext& Context, UEdGraphPin* Net, TIndirectArray<FBPTerminal>& NetArray)
{
	// Determine the scope this takes place in
	UStruct* SearchScope = Context.Function;

	UEdGraphPin* SelfPin = CompilerContext.GetSchema()->FindSelfPin(*(Net->GetOwningNode()), EGPD_Input);
	if (SelfPin != NULL)
	{
		SearchScope = Context.GetScopeFromPinType(SelfPin->PinType, Context.NewClass);
	}

	// Find the variable in the search scope
	UProperty* BoundProperty = FKismetCompilerUtilities::FindPropertyInScope(SearchScope, Net, CompilerContext.MessageLog, CompilerContext.GetSchema(), Context.NewClass);
	if (BoundProperty != NULL)
	{
		// Create the term in the list
		FBPTerminal* Term = new (NetArray) FBPTerminal();
		Term->CopyFromPin(Net, Net->PinName);
		Term->AssociatedVarProperty = BoundProperty;
		Context.NetMap.Add(Net, Term);

		// Read-only variables and variables in const classes are both const
		if (BoundProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly) || Context.IsConstFunction())
		{
			Term->bIsConst = true;
		}

		// Resolve the context term
		if (SelfPin != NULL)
		{
			FBPTerminal** pContextTerm = Context.NetMap.Find(FEdGraphUtilities::GetNetFromPin(SelfPin));
			Term->Context = (pContextTerm != NULL) ? *pContextTerm : NULL;
		}
	}
}

FBlueprintCompiledStatement& FNodeHandlingFunctor::GenerateSimpleThenGoto(FKismetFunctionContext& Context, UEdGraphNode& Node, UEdGraphPin* ThenExecPin)
{
	UEdGraphNode* TargetNode = NULL;
	if ((ThenExecPin != NULL) && (ThenExecPin->LinkedTo.Num() > 0))
	{
		TargetNode = ThenExecPin->LinkedTo[0]->GetOwningNode();
	}

	if (Context.bCreateDebugData)
	{
		FBlueprintCompiledStatement& TraceStatement = Context.AppendStatementForNode(&Node);
		TraceStatement.Type = KCST_WireTraceSite;
		TraceStatement.Comment = Node.NodeComment.IsEmpty() ? Node.GetName() : Node.NodeComment;
	}

	FBlueprintCompiledStatement& GotoStatement = Context.AppendStatementForNode(&Node);
	GotoStatement.Type = KCST_UnconditionalGoto;
	Context.GotoFixupRequestMap.Add(&GotoStatement, TargetNode);

	return GotoStatement;
}

FBlueprintCompiledStatement& FNodeHandlingFunctor::GenerateSimpleThenGoto(FKismetFunctionContext& Context, UEdGraphNode& Node)
{
	UEdGraphPin* ThenExecPin = CompilerContext.GetSchema()->FindExecutionPin(Node, EGPD_Output);
	return GenerateSimpleThenGoto(Context, Node, ThenExecPin);
}

bool FNodeHandlingFunctor::ValidateAndRegisterNetIfLiteral(FKismetFunctionContext& Context, UEdGraphPin* Net)
{
	if (Net->LinkedTo.Num() == 0)
	{
		// Make sure the default value is valid
		FString DefaultAllowedResult = CompilerContext.GetSchema()->IsCurrentPinDefaultValid(Net);
		if (DefaultAllowedResult != TEXT(""))
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("InvalidDefaultValue_Error", "Default value '%s' for @@ is invalid: '%s'").ToString(), *(Net->GetDefaultAsString()), *DefaultAllowedResult), Net);
			return false;
		}

		FBPTerminal* LiteralTerm = Context.RegisterLiteral(Net);
		Context.LiteralHackMap.Add(Net, LiteralTerm);
	}

	return true;
}

void FNodeHandlingFunctor::SanitizeName(FString& Name)
{
	// Sanitize the name
	for (int32 i = 0; i < Name.Len(); ++i)
	{
		TCHAR& C = Name[i];

		const bool bGoodChar =
			((C >= 'A') && (C <= 'Z')) || ((C >= 'a') && (C <= 'z')) ||		// A-Z (upper and lowercase) anytime
			(C == '_') ||													// _ anytime
			((i > 0) && (C >= '0') && (C <= '9'));							// 0-9 after the first character

		if (!bGoodChar)
		{
			C = '_';
		}
	}
}

void FNodeHandlingFunctor::RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node)
{
	for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Node->Pins[PinIndex];
		if (!CompilerContext.GetSchema()->IsMetaPin(*Pin)
			|| (CompilerContext.GetSchema()->IsSelfPin(*Pin) && Pin->LinkedTo.Num() == 0 && Pin->DefaultObject) )
		{
			UEdGraphPin* Net = FEdGraphUtilities::GetNetFromPin(Pin);

			if (Context.NetMap.Find(Net) == NULL)
			{
				// New net, resolve the term that will be used to construct it
				FBPTerminal* Term = NULL;

				if ((Net->Direction == EGPD_Input) && (Net->LinkedTo.Num() == 0))
				{
					// Make sure the default value is valid
					FString DefaultAllowedResult = CompilerContext.GetSchema()->IsCurrentPinDefaultValid(Net);
					if (DefaultAllowedResult != TEXT(""))
					{
						CompilerContext.MessageLog.Error(*FString::Printf(*LOCTEXT("InvalidDefaultValue_Error", "Default value '%s' for @@ is invalid: '%s'").ToString(), *(Net->GetDefaultAsString()), *DefaultAllowedResult), Net);

						// Skip over these properties if they are array or ref properties, because the backend can't emit valid code for them
						if( Pin->PinType.bIsArray || Pin->PinType.bIsReference )
						{
							continue;
						}
					}

					Term = Context.RegisterLiteral(Net);
					Context.NetMap.Add(Net, Term);
				}
				else
				{
					RegisterNet(Context, Pin);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FNetNameMapping

template<>
KISMETCOMPILER_API FString FNetNameMapping::MakeBaseName<UEdGraphPin>(const UEdGraphPin* Net)
{
	UEdGraphNode* Owner = Net->GetOwningNode();
	FString Part1 = Owner->GetDescriptiveCompiledName();

	return FString::Printf(TEXT("%s_%s"), *Part1, *Net->PinName);
}

template<>
KISMETCOMPILER_API FString FNetNameMapping::MakeBaseName<UEdGraphNode>(const UEdGraphNode* Net)
{
	return FString::Printf(TEXT("%s"), *Net->GetDescriptiveCompiledName());
}

//////////////////////////////////////////////////////////////////////////
// FKismetFunctionContext

FKismetFunctionContext::FKismetFunctionContext(FCompilerResultsLog& InMessageLog, UEdGraphSchema_K2* InSchema, UBlueprintGeneratedClass* InNewClass, UBlueprint* InBlueprint)
	: Blueprint(InBlueprint)
	, SourceGraph(NULL)
	, EntryPoint(NULL)
	, Function(NULL)
	, NewClass(InNewClass)
	, MessageLog(InMessageLog)
	, Schema(InSchema)
	, UUIDCounter(1024)
	, bIsUbergraph(false)
	, bCannotBeCalledFromOtherKismet(false)
	, NetFlags(0)
	, bIsInterfaceStub(false)
	, bIsConstFunction(false)
	, bCreateDebugData(true)
	, bIsSimpleStubGraphWithNoParams(false)
	, SourceEventFromStubGraph(NULL)
{
	NetNameMap = new FNetNameMapping();
	bAllocatedNetNameMap = true;
}

FKismetFunctionContext::~FKismetFunctionContext()
{
	if (bAllocatedNetNameMap)
	{
		delete NetNameMap;
		NetNameMap = NULL;
	}

	for (int32 i = 0; i < AllGeneratedStatements.Num(); ++i)
	{
		delete AllGeneratedStatements[i];
	}
}

void FKismetFunctionContext::SetExternalNetNameMap(FNetNameMapping* NewMap)
{
	if (bAllocatedNetNameMap)
	{
		delete NetNameMap;
		NetNameMap = NULL;
	}

	bAllocatedNetNameMap = false;

	NetNameMap = NewMap;
}

#undef LOCTEXT_NAMESPACE

//////////////////////////////////////////////////////////////////////////
// FBPTerminal

void FBPTerminal::CopyFromPin(UEdGraphPin* Net, const FString& NewName)
{
	Type = Net->PinType;
	Source = Net;
	Name = NewName;

	bPassedByReference = Net->PinType.bIsReference;

	const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(Net->GetSchema());
	const bool bStructCategory = Schema && (Schema->PC_Struct == Net->PinType.PinCategory);
	const bool bStructSubCategoryObj = (NULL != Cast<UScriptStruct>(Net->PinType.PinSubCategoryObject.Get()));
	bIsStructContext = bStructCategory && bStructSubCategoryObj;
}
