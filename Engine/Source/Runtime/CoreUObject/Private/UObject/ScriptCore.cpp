// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScriptCore.cpp: Kismet VM execution and support code.
=============================================================================*/

#include "CoreUObjectPrivate.h"
#include "MallocProfiler.h"

DEFINE_LOG_CATEGORY(LogScriptFrame);
DEFINE_LOG_CATEGORY_STATIC(LogScriptCore, Log, All);

DECLARE_CYCLE_STAT(TEXT("Blueprint Time"),STAT_BlueprintTime,STATGROUP_Game);


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

//
// Native function table.
//
COREUOBJECT_API Native GNatives[EX_Max];
COREUOBJECT_API int32 GNativeDuplicate=0;

COREUOBJECT_API void (UObject::*GCasts[CST_Max])( FFrame &Stack, RESULT_DECL );
COREUOBJECT_API int32 GCastDuplicate=0;

#define RUNAWAY_LIMIT 1000000
#if !PLATFORM_DESKTOP
	#define RECURSE_LIMIT 120
#else
	#define RECURSE_LIMIT 250
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	#define DO_GUARD 1
#endif

//@TODO: ScriptParallel: Contended static usage
#if DO_GUARD
	static int32 Runaway=0;
	static int32 Recurse=0;
	#define CHECK_RUNAWAY {++Runaway;}
	COREUOBJECT_API void GInitRunaway() {Recurse=Runaway=0;}
#else
	#define CHECK_RUNAWAY
	COREUOBJECT_API void GInitRunaway() {}
#endif

#define IMPLEMENT_FUNCTION(cls,func) \
	static FNativeFunctionRegistrar cls##func##Registar(cls::StaticClass(),#func,(Native)&cls::func);

#define IMPLEMENT_CAST_FUNCTION(cls, CastIndex, func) \
	IMPLEMENT_FUNCTION(cls, func); \
	static uint8 cls##func##CastTemp = GRegisterCast( CastIndex, (Native)&cls::func );

#define IMPLEMENT_VM_FUNCTION(BytecodeIndex, func) \
	IMPLEMENT_FUNCTION(UObject, func) \
	static uint8 UObject##func##BytecodeTemp = GRegisterNative( BytecodeIndex, (Native)&UObject::func );

//////////////////////////////////////////////////////////////////////////
// FBlueprintCoreDelegates

FBlueprintCoreDelegates::FOnScriptDebuggingEvent FBlueprintCoreDelegates::OnScriptException;

void FBlueprintCoreDelegates::ThrowScriptException(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info)
{
	switch (Info.GetType())
	{
	case EBlueprintExceptionType::Breakpoint:
	case EBlueprintExceptionType::Tracepoint:
	case EBlueprintExceptionType::WireTracepoint:
		break;

	default:
		UE_SUPPRESS(LogScript, Warning, const_cast<FFrame*>(&StackFrame)->Logf(TEXT("%s"), *(Info.GetDescription())));
		break;
	}

	OnScriptException.Broadcast(ActiveObject, StackFrame, Info);

	if (Info.GetType() == EBlueprintExceptionType::FatalError)
	{
		// Crash maybe?
	}
}

//////////////////////////////////////////////////////////////////////////
// FEditorScriptExecutionGuard
FEditorScriptExecutionGuard::FEditorScriptExecutionGuard()
	: bOldGAllowScriptExecutionInEditor(GAllowActorScriptExecutionInEditor)
{
	GAllowActorScriptExecutionInEditor = true;

	if( GIsEditor && !FApp::IsGame() )
	{
		GInitRunaway();
	}
}

FEditorScriptExecutionGuard::~FEditorScriptExecutionGuard()
{
	GAllowActorScriptExecutionInEditor = bOldGAllowScriptExecutionInEditor;
}

/*-----------------------------------------------------------------------------
	FFrame implementation.
-----------------------------------------------------------------------------*/

void FFrame::Step(UObject *Context, RESULT_DECL)
{
	int32 B = *Code++;
	(Context->*GNatives[B])(*this,Result);
}

void FFrame::StepExplicitProperty(void*const Result, UProperty* Property)
{
	checkSlow(Result != NULL);

	if (Property->PropertyFlags & CPF_OutParm)
	{
		// look through the out parameter infos and find the one that has the address of this property
		FOutParmRec* Out = OutParms;
		checkSlow(Out);
		while (Out->Property != Property)
		{
			Out = Out->NextOutParm;
			checkSlow(Out);
		}
		MostRecentPropertyAddress = Out->PropAddr;
		// no need to copy property value, since the caller is just looking for MostRecentPropertyAddress
	}
	else
	{
		MostRecentPropertyAddress = Property->ContainerPtrToValuePtr<uint8>(Locals);
		Property->CopyCompleteValueToScriptVM(Result, MostRecentPropertyAddress);
	}
}

//
// Error or warning handler.
//
//@TODO: This function should take more information in, or be able to gather it from the callstack!
void FFrame::KismetExecutionMessage(const TCHAR* Message, ELogVerbosity::Type Verbosity)
{
	// Treat errors/warnings as bad
	if (Verbosity == ELogVerbosity::Warning)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		static bool GTreatScriptWarningsFatal = FParse::Param(FCommandLine::Get(),TEXT("FATALSCRIPTWARNINGS"));
		if (GTreatScriptWarningsFatal)
		{
			Verbosity = ELogVerbosity::Error;
		}
#endif
	}

	if (Verbosity == ELogVerbosity::Error)
	{
		UE_LOG(LogScriptCore, Fatal,TEXT("%s"), Message);
	}
	else
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		static bool GScriptStackForScriptWarning = FParse::Param(FCommandLine::Get(),TEXT("SCRIPTSTACKONWARNINGS"));
		UE_LOG(LogScript, Warning, TEXT("%s"), Message);
#endif
	}
}

void FFrame::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	// Treat errors/warnings as bad
	if (Verbosity == ELogVerbosity::Warning)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		static bool GTreatScriptWarningsFatal = FParse::Param(FCommandLine::Get(),TEXT("FATALSCRIPTWARNINGS"));
		if (GTreatScriptWarningsFatal)
		{
			Verbosity = ELogVerbosity::Error;
		}
#endif
	}
	if( Verbosity==ELogVerbosity::Error )
	{
		UE_LOG(LogScriptCore, Fatal,
			TEXT("%s\r\n\t%s\r\n\t%s:%04X\r\n\t%s"),
			V,
			*Object->GetFullName(),
			*Node->GetFullName(),
			Code - Node->Script.GetTypedData(),
			*GetStackTrace()
		);
	}
	else
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		static bool GScriptStackForScriptWarning = FParse::Param(FCommandLine::Get(),TEXT("SCRIPTSTACKONWARNINGS"));
		UE_LOG(LogScript, Warning,
			TEXT("%s\r\n\t%s\r\n\t%s:%04X%s"),
			V,
			*Object->GetFullName(),
			*Node->GetFullName(),
			Code - Node->Script.GetTypedData(),
			GScriptStackForScriptWarning ? *(FString(TEXT("\r\n")) + GetStackTrace()) : TEXT("")
		);
#endif
	}
}

FString FFrame::GetStackTrace() const
{
	FString Result;

	// travel down the stack recording the frames
	TArray<const FFrame*> FrameStack;
	const FFrame* CurrFrame = this;
	while (CurrFrame != NULL)
	{
		FrameStack.Add(CurrFrame);
		CurrFrame = CurrFrame->PreviousFrame;
	}
	
	// and then dump them to a string
	Result += FString( TEXT("Script call stack:\n") );
	for (int32 Index = FrameStack.Num() - 1; Index >= 0; Index--)
	{
		Result += FString::Printf(TEXT("\t%s\n"), *FrameStack[Index]->Node->GetFullName());
	}

	return Result;
}


/*-----------------------------------------------------------------------------
	Global script execution functions.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	Native registry.
-----------------------------------------------------------------------------*/

//
// Register a native function.
// Warning: Called at startup time, before engine initialization.
//
COREUOBJECT_API uint8 GRegisterNative( int32 NativeBytecodeIndex, const Native& Func )
{
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		for (uint32 i = 0; i < ARRAY_COUNT(GNatives); i++)
		{
			GNatives[i] = &UObject::execUndefined;
		}
	}

	if( NativeBytecodeIndex != INDEX_NONE )
	{
		if( NativeBytecodeIndex<0 || (uint32)NativeBytecodeIndex>ARRAY_COUNT(GNatives) || GNatives[NativeBytecodeIndex]!=&UObject::execUndefined) 
		{
#if !IS_MONOLITHIC
			if (GIsHotReload)
			{
				AddHotReloadFunctionRemap(Func, GNatives[NativeBytecodeIndex]);
			}
			else
#endif
			{
			GNativeDuplicate = NativeBytecodeIndex;
			}
		}
		GNatives[NativeBytecodeIndex] = Func;
	}

	return 0;
}

COREUOBJECT_API uint8 GRegisterCast( int32 CastCode, const Native& Func )
{
	static int32 bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		for (uint32 i = 0; i < ARRAY_COUNT(GCasts); i++)
		{
			GCasts[i] = &UObject::execUndefined;
		}
	}

	//@TODO: UCREMOVAL: Remove rest of cast machinery
	check((CastCode == CST_ObjectToBool) || (CastCode == CST_ObjectToInterface) || (CastCode == CST_InterfaceToBool));

	if (CastCode != INDEX_NONE)
	{
		if(  
#if !IS_MONOLITHIC
			!GIsHotReload && 
#endif
			(CastCode<0 || (uint32)CastCode>ARRAY_COUNT(GCasts) || GCasts[CastCode]!=&UObject::execUndefined) ) 
		{
			GCastDuplicate = CastCode;
		}
		GCasts[CastCode] = Func;
	}
	return 0;
}

void UObject::SkipFunction(FFrame& Stack, RESULT_DECL, UFunction* Function)
{
	// allocate temporary memory on the stack for evaluating parameters
	uint8* Frame = (uint8*)FMemory_Alloca(Function->PropertiesSize);
	FMemory::Memzero(Frame, Function->PropertiesSize);
	for (UProperty* Property = (UProperty*)Function->Children; *Stack.Code != EX_EndFunctionParms; Property = (UProperty*)Property->Next)
	{
		Stack.MostRecentPropertyAddress = NULL;
		// evaluate the expression into our temporary memory space
		// it'd be nice to be able to skip the copy, but most native functions assume a non-NULL Result pointer
		// so we can only do that if we know the expression is an l-value (out parameter)
		Stack.Step(Stack.Object, (Property->PropertyFlags & CPF_OutParm) ? NULL : Property->ContainerPtrToValuePtr<uint8>(Frame));
	}

	// advance the code past EX_EndFunctionParms
	Stack.Code++;

	// destruct properties requiring it for which we had to use our temporary memory 
	// @warning: conditions for skipping DestroyValue() here must match conditions for passing NULL to Stack.Step() above
	for (UProperty* Destruct = Function->DestructorLink; Destruct; Destruct = Destruct->DestructorLinkNext)
	{
		if (!Destruct->HasAnyPropertyFlags(CPF_OutParm))
		{
			Destruct->DestroyValue_InContainer(Frame);
		}
	}

	UProperty* ReturnProp = Function->GetReturnProperty();
	if (ReturnProp != NULL)
	{
		// destroy old value if necessary
		ReturnProp->DestroyValue(Result);
		// copy zero value for return property into Result
		FMemory::Memzero(Result, ReturnProp->ArrayDim * ReturnProp->ElementSize);
	}
}

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4750) // warning C4750: function with _alloca() inlined into a loop
#endif

void UObject::CallFunction( FFrame& Stack, RESULT_DECL, UFunction* Function )
{
	checkSlow(Function);

	if (Function->FunctionFlags & FUNC_Native)
	{
		// Call native networkable function.
		uint8 Buffer[1024];
		int32 FunctionCallspace = GetFunctionCallspace( Function, Buffer, &Stack );
		uint8* SavedCode = NULL;
		if (FunctionCallspace & FunctionCallspace::Remote)
		{
			SavedCode = Stack.Code; // Since this is native, we need to rollback the stack if we are calling both remotely and locally
			CallRemoteFunction(Function, Buffer, &Stack);
		}

		if (FunctionCallspace & FunctionCallspace::Local)
		{
			if (SavedCode)
			{
				Stack.Code = SavedCode;
			}

			// Call regular native function.
			FScopeCycleCounterUObject ContextScope(Stack.Object);
			FScopeCycleCounterUObject FunctionScope(Function);

			Function->Invoke(this, Stack, Result);
		}
		else
		{
			// Eat up the remaining parameters in the stream.
			SkipFunction(Stack, Result, Function);
		}
	}
	else
	{
		// Make new stack frame in the current context.
		uint8* Frame = (uint8*)FMemory_Alloca(Function->PropertiesSize);
		FMemory::Memzero( Frame, Function->PropertiesSize );
		FFrame NewStack( this, Function, Frame, &Stack, Function->Children );
		FOutParmRec** LastOut = &NewStack.OutParms;
		UProperty* Property;

  		// Check to see if we need to handle a return value for this function.  We need to handle this first, because order of return parameters isn't always first.
 		if( Function->HasAnyFunctionFlags(FUNC_HasOutParms) )
 		{
 			// Iterate over the function parameters, searching for the ReturnValue
 			for( TFieldIterator<UProperty> ParmIt(Function); ParmIt; ++ParmIt )
 			{
 				Property = *ParmIt;
 				if( Property->HasAnyPropertyFlags(CPF_ReturnParm) )
 				{
 					FOutParmRec* RetVal = (FOutParmRec*)FMemory_Alloca(sizeof(FOutParmRec));
 
 					// Our context should be that we're in a variable assignment to the return value, so ensure that we have a valid property to return to
 					check(Result != NULL);
 					RetVal->PropAddr = (uint8*)Result;
 					RetVal->Property = Property;
					NewStack.OutParms = RetVal;
 
 					// A function can only have one return value, so we can stop searching
 					break;
 				}
 			}
 		}
		
		for (Property = (UProperty*)Function->Children; *Stack.Code != EX_EndFunctionParms; Property = (UProperty*)Property->Next)
		{
			checkfSlow(Property, TEXT("NULL Property in Function %s"), *Function->GetPathName()); 

			Stack.MostRecentPropertyAddress = NULL;
			
			// Skip the return parameter case, as we've already handled it above
			const bool bIsReturnParam = ((Property->PropertyFlags & CPF_ReturnParm) != 0);
			if( bIsReturnParam )
			{
				continue;
			}

			if (Property->PropertyFlags & CPF_OutParm)
			{
				// evaluate the expression for this parameter, which sets Stack.MostRecentPropertyAddress to the address of the property accessed
				Stack.Step(Stack.Object, NULL);

				FOutParmRec* Out = (FOutParmRec*)FMemory_Alloca(sizeof(FOutParmRec));
				// set the address and property in the out param info
				// warning: Stack.MostRecentPropertyAddress could be NULL for optional out parameters
				// if that's the case, we use the extra memory allocated for the out param in the function's locals
				// so there's always a valid address
				Out->PropAddr = (Stack.MostRecentPropertyAddress != NULL) ? Stack.MostRecentPropertyAddress : Property->ContainerPtrToValuePtr<uint8>(Frame);
				Out->Property = Property;

				// add the new out param info to the stack frame's linked list
				if (*LastOut)
				{
					(*LastOut)->NextOutParm = Out;
					LastOut = &(*LastOut)->NextOutParm;
				}
				else
				{
					*LastOut = Out;
				}
			}
			else
			{
				// copy the result of the expression for this parameter into the appropriate part of the local variable space
				uint8* Param = Property->ContainerPtrToValuePtr<uint8>(NewStack.Locals);
				checkSlow(Param);

				Stack.Step(Stack.Object, Param);
			}
		}
		Stack.Code++;
#if UE_BUILD_DEBUG
		// set the next pointer of the last item to NULL so we'll properly assert if something goes wrong
		if (*LastOut)
		{
			(*LastOut)->NextOutParm = NULL;
		}
#endif

		// Initialize any local struct properties with defaults
		for ( UProperty* LocalProp = Function->FirstPropertyToInit; LocalProp != NULL; LocalProp = (UProperty*)LocalProp->Next )
		{
			LocalProp->InitializeValue_InContainer(NewStack.Locals);
		}

		const bool bIsValidFunction = (Function->FunctionFlags & FUNC_Native) || (Function->Script.Num() > 0);

		// Execute the code.
		if( bIsValidFunction )
		{
			ProcessInternal( NewStack, Result );
		}

		// destruct properties on the stack, except for out params since we know we didn't use that memory
		for (UProperty* Destruct = Function->DestructorLink; Destruct; Destruct = Destruct->DestructorLinkNext)
		{
			if (!Destruct->HasAnyPropertyFlags(CPF_OutParm))
			{
				Destruct->DestroyValue_InContainer(NewStack.Locals);
			}
		}
	}
}

/** Helper function to zero the return value in case of a fatal (runaway / infinite recursion) error */
void ClearReturnValue(UProperty* ReturnProp, RESULT_DECL)
{
	if (ReturnProp != NULL)
	{
		// destroy old value if necessary
		if (!ReturnProp->HasAllPropertyFlags(CPF_NoDestructor))
		{
			ReturnProp->DestroyValue(Result);
		}
		// copy zero value for return property into Result
		FMemory::Memzero(Result, ReturnProp->ArrayDim * ReturnProp->ElementSize);
	}
}

void UObject::ProcessInternal( FFrame& Stack, RESULT_DECL )
{
	// remove later when stable
	if (GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		static int32 num=0;
		num++;
		if (num < 5)
		{
			ensureMsgf(!GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists), TEXT("Object '%s' is being used for execution, but its class is out of date and has been replaced with a recompiled class!"), *GetFullName());
		}
		return;
	}

	int32 FunctionCallspace = GetFunctionCallspace( (UFunction*)Stack.Node, Stack.Locals, NULL );
	if (FunctionCallspace & FunctionCallspace::Remote)
	{
		CallRemoteFunction((UFunction*)Stack.Node, Stack.Locals, NULL);
	}

	if (FunctionCallspace & FunctionCallspace::Local)
	{
		MS_ALIGN(16) uint8 Buffer[MAX_SIMPLE_RETURN_VALUE_SIZE] GCC_ALIGN(16);

#if DO_GUARD
		if (++Recurse > RECURSE_LIMIT)
		{
			// We've hit the recursion limit, so print out the stack, warn, and then continue with a zeroed return value.
			UE_LOG(LogScriptCore, Log, TEXT("%s"), *Stack.GetStackTrace());

			// If we have a return property, return a zeroed value in it, to try and save execution as much as possible
			UProperty* ReturnProp = ((UFunction*)Stack.Node)->GetReturnProperty();
			ClearReturnValue(ReturnProp, Result);

			// Notify anyone who cares that we've had a fatal error, so we can shut down PIE, etc
			const FString Desc = FString::Printf(TEXT("Infinite script recursion (%i calls) detected"), RECURSE_LIMIT);
			FBlueprintExceptionInfo InfiniteRecursionExceptionInfo(EBlueprintExceptionType::InfiniteLoop, Desc);
			FBlueprintCoreDelegates::ThrowScriptException(this, Stack, InfiniteRecursionExceptionInfo);

			return;
		}
#endif
		FScopeCycleCounterUObject ContextScope(Stack.Object);
		FScopeCycleCounterUObject FunctionScope((UFunction*)Stack.Node);

		// Execute the bytecode
		while (*Stack.Code != EX_Return)
		{
#if DO_GUARD
			if( Runaway > RUNAWAY_LIMIT )
			{
				// We've hit the recursion limit, so print out the stack, warn, and then continue with a zeroed return value.
				UE_LOG(LogScriptCore, Log, TEXT("%s"), *Stack.GetStackTrace());

				// If we have a return property, return a zeroed value in it, to try and save execution as much as possible
				UProperty* ReturnProp = ((UFunction*)Stack.Node)->GetReturnProperty();
				ClearReturnValue(ReturnProp, Result);

				// Notify anyone who cares that we've had a fatal error, so we can shut down PIE, etc
				const FString Desc = FString::Printf(TEXT("Runaway loop detected (over %i iterations)"), RUNAWAY_LIMIT);
				FBlueprintExceptionInfo RunawayLoopExceptionInfo(EBlueprintExceptionType::InfiniteLoop, Desc);
				FBlueprintCoreDelegates::ThrowScriptException(this, Stack, RunawayLoopExceptionInfo);

				Runaway = 0;
				return;
			}
#endif

			Stack.Step(Stack.Object, Buffer);
		}

		// Step over the return statement and evaluate the result expression
		Stack.Code++;
		Stack.Step(Stack.Object, Result);

#if DO_GUARD
		--Recurse;
#endif
	}
	else
	{
		UProperty* ReturnProp = ((UFunction*)Stack.Node)->GetReturnProperty();
		if (ReturnProp != NULL)
		{
			// destroy old value if necessary
			ReturnProp->DestroyValue(Result);
			// copy zero value for return property into Result
			FMemory::Memzero(Result, ReturnProp->ArrayDim * ReturnProp->ElementSize);
		}
	}
}

bool UObject::CallFunctionByNameWithArguments( const TCHAR* Str, FOutputDevice& Ar, UObject* Executor )
{
	// Find an exec function.
	FString MsgStr;
	if(!FParse::Token(Str,MsgStr,true))
	{
		UE_LOG(LogScriptCore, Verbose, TEXT("CallFunctionByNameWithArguments: Not Parsed '%s'"), Str);
		return false;
	}
	const FName Message = FName(*MsgStr,FNAME_Find);
	if(Message == NAME_None)
	{
		UE_LOG(LogScriptCore, Verbose, TEXT("CallFunctionByNameWithArguments: Name not found '%s'"), Str);
		return false;
	}
	UFunction* Function = FindFunction(Message);
	if(NULL == Function)
	{
		UE_LOG(LogScriptCore, Verbose, TEXT("CallFunctionByNameWithArguments: Function not found '%s'"), Str);
		return false;
	}
	if(0 == (Function->FunctionFlags & FUNC_Exec))
	{
		UE_LOG(LogScriptCore, Verbose, TEXT("CallFunctionByNameWithArguments: Function not executable '%s'"), Str);
		return false;
	}

	UProperty* LastParameter=NULL;

	// find the last parameter
	for ( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm)) == CPF_Parm; ++It )
	{
		LastParameter = *It;
	}

	UStrProperty* LastStringParameter = Cast<UStrProperty>(LastParameter);


	// Parse all function parameters.
	uint8* Parms = (uint8*)FMemory_Alloca(Function->ParmsSize);
	FMemory::Memzero( Parms, Function->ParmsSize );

	bool Failed = 0;
	int32 NumParamsEvaluated = 0;
	for( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It, NumParamsEvaluated++ )
	{
		UProperty* propertyParam = *It;
		if (NumParamsEvaluated == 0 && Executor)
		{
			UObjectPropertyBase* Op = Cast<UObjectPropertyBase>(*It);
			if( Op && Executor->IsA(Op->PropertyClass) )
			{
				// First parameter is implicit reference to object executing the command.
				Op->SetObjectPropertyValue(Op->ContainerPtrToValuePtr<uint8>(Parms), Executor);
				continue;
			}
		}

		FParse::Next( &Str );

		// if Str is empty but we have more params to read parse the function to see if these have defaults, if so set them
		bool bFoundDefault = false;
		bool bFailedImport = true;
		if (!FCString::Strcmp(Str, TEXT("")))
		{
			const FName DefaultPropertyKey(*(FString(TEXT("CPP_Default_")) + propertyParam->GetName()));
#if WITH_EDITOR
			const FString PropertyDefaultValue = Function->GetMetaData(DefaultPropertyKey);
#else
			const FString PropertyDefaultValue = TEXT("");
#endif
			if (!PropertyDefaultValue.IsEmpty()) 
			{
				bFoundDefault = true;
				uint32 ExportFlags = PPF_Localized;

				// if this is the last parameter of the exec function and it's a string, make sure that it accepts the remainder of the passed in value
				if ( LastStringParameter != *It )
				{
					ExportFlags |= PPF_Delimited;
				}
				const TCHAR* Result = It->ImportText( *PropertyDefaultValue, It->ContainerPtrToValuePtr<uint8>(Parms), ExportFlags, NULL );
				bFailedImport = Result == NULL;
			}
		}

		if (!bFoundDefault)
		{
			uint32 ExportFlags = PPF_Localized;

			// if this is the last parameter of the exec function and it's a string, make sure that it accepts the remainder of the passed in value
			if ( LastStringParameter != *It )
			{
				ExportFlags |= PPF_Delimited;
			}
			const TCHAR* PreviousStr = Str;
			const TCHAR* Result = It->ImportText( Str, It->ContainerPtrToValuePtr<uint8>(Parms), ExportFlags, NULL );
			bFailedImport = (Result == NULL || Result == PreviousStr);
			
			// move to the next parameter
			Str = Result;
		}
		
		if( bFailedImport )
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Message"), FText::FromName( Message ));
			Arguments.Add(TEXT("PropertyName"), FText::FromString( It->GetName() ));
			Ar.Logf( *FText::Format( NSLOCTEXT( "Core", "BadProperty", "'{Message}': Bad or missing property '{PropertyName}'" ), Arguments ).ToString() );
			Failed = true;

			break;
		}

	}

	if( !Failed )
	{
		ProcessEvent( Function, Parms );
	}

	//!!destructframe see also UObject::ProcessEvent
	for( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
	{
		It->DestroyValue_InContainer(Parms);
	}

	// Success.
	return true;
}

UFunction* UObject::FindFunction( FName InName ) const
{
	return GetClass()->FindFunctionByName(InName);
}

UFunction* UObject::FindFunctionChecked( FName InName ) const
{
	UFunction* Result = FindFunction(InName);
	if (Result == NULL)
	{
		UE_LOG(LogScriptCore, Fatal, TEXT("Failed to find function %s in %s"), *InName.ToString(), *GetFullName());
	}
	return Result;
}

void UObject::ProcessEvent( UFunction* Function, void* Parms )
{
	static int32 ScriptEntryTag = 0;

	checkf(!HasAnyFlags(RF_Unreachable),TEXT("%s  Function: '%s'"), *GetFullName(), *Function->GetPathName());
	checkf(!GIsRoutingPostLoad, TEXT("Cannot call UnrealScript (%s - %s) while PostLoading objects"), *GetFullName(), *Function->GetFullName());

	// Reject.
	if (IsPendingKill())
	{
		return;
	}
	
#if WITH_EDITORONLY_DATA
	// Cannot invoke script events when the game thread is paused for debugging.
	if(GIntraFrameDebuggingGameThread)
	{
		if(GFirstFrameIntraFrameDebugging)
		{
			UE_LOG(LogScriptCore, Warning, TEXT("Cannot call UnrealScript (%s - %s) while stopped at a breakpoint."), *GetFullName(), *Function->GetFullName());
		}

		return;
	}
#endif	// WITH_EDITORONLY_DATA

	if ((Function->FunctionFlags & FUNC_Native) != 0)
	{
		int32 FunctionCallspace = GetFunctionCallspace(Function, Parms, NULL);
		if (FunctionCallspace & FunctionCallspace::Remote)
		{
			CallRemoteFunction(Function, Parms, NULL);
		}

		if ((FunctionCallspace & FunctionCallspace::Local) == 0)
		{
			return;
		}
	}
	else if (Function->Script.Num() == 0)
	{
		return;
	}
	checkSlow((Function->ParmsSize == 0) || (Parms != NULL));

	ScriptEntryTag++;

	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_BlueprintTime, ScriptEntryTag == 1);

	// Scope required for scoped script stats.
	{
		// Create a new local execution stack.
		FFrame NewStack( this, Function, FMemory_Alloca(Function->PropertiesSize), NULL, Function->Children );
		checkSlow(NewStack.Locals || Function->ParmsSize == 0);

		// initialize the parameter properties
		FMemory::Memcpy( NewStack.Locals, Parms, Function->ParmsSize );

		// zero the local property memory
		FMemory::Memzero( NewStack.Locals+Function->ParmsSize, Function->PropertiesSize-Function->ParmsSize );

		// if the function has out parameters, fill the stack frame's out parameter info with the info for those params 
		if ( Function->HasAnyFunctionFlags(FUNC_HasOutParms) )
		{
			FOutParmRec** LastOut = &NewStack.OutParms;
			for ( UProperty* Property = (UProperty*)Function->Children; Property && (Property->PropertyFlags&(CPF_Parm)) == CPF_Parm; Property = (UProperty*)Property->Next )
			{
				// this is used for optional parameters - the destination address for out parameter values is the address of the calling function
				// so we'll need to know which address to use if we need to evaluate the default parm value expression located in the new function's
				// bytecode
				if ( Property->HasAnyPropertyFlags(CPF_OutParm) )
				{
					FOutParmRec* Out = (FOutParmRec*)FMemory_Alloca(sizeof(FOutParmRec));
					// set the address and property in the out param info
					// note that since C++ doesn't support "optional out" we can ignore that here
					Out->PropAddr = Property->ContainerPtrToValuePtr<uint8>(Parms);
					Out->Property = Property;

					// add the new out param info to the stack frame's linked list
					if (*LastOut)
					{
						(*LastOut)->NextOutParm = Out;
						LastOut = &(*LastOut)->NextOutParm;
					}
					else
					{
						*LastOut = Out;
					}
				}
			}

#if UE_BUILD_DEBUG
			// set the next pointer of the last item to NULL so we'll properly assert if something goes wrong
			if (*LastOut)
			{
				(*LastOut)->NextOutParm = NULL;
			}
#endif
		}

		for ( UProperty* LocalProp = Function->FirstPropertyToInit; LocalProp != NULL; LocalProp = (UProperty*)LocalProp->Next )
		{
			LocalProp->InitializeValue_InContainer(NewStack.Locals);
		}
		// Call native function or UObject::ProcessInternal.
		if (Function->FunctionFlags & FUNC_Native)
		{
			FScopeCycleCounterUObject ContextScope(this);
			FScopeCycleCounterUObject FunctionScope(Function);
			Function->Invoke(this, NewStack, (uint8*)Parms + Function->ReturnValueOffset);
		}
		else
		{
			Function->Invoke(this, NewStack, (uint8*)Parms + Function->ReturnValueOffset);
		}

		// Destroy local variables except function parameters.!! see also UObject::CallFunctionByNameWithArguments
		// also copy back constructed value parms here so the correct copy is destroyed when the event function returns
		for (UProperty* P = Function->DestructorLink; P; P = P->DestructorLinkNext)
		{
			if (!P->IsInContainer(Function->ParmsSize))
			{
				P->DestroyValue_InContainer(NewStack.Locals);
			}
			else if (!(P->PropertyFlags & CPF_OutParm))
			{
				FMemory::Memcpy(P->ContainerPtrToValuePtr<uint8>(Parms), P->ContainerPtrToValuePtr<uint8>(NewStack.Locals), P->ArrayDim * P->ElementSize);
			}
		}
	}

	--ScriptEntryTag;
}

#ifdef _MSC_VER
#pragma warning (pop)
#endif

void UObject::execUndefined(FFrame& Stack, RESULT_DECL)
{
	Stack.Logf(ELogVerbosity::Error, TEXT("Unknown code token %02X"), Stack.Code[-1] );
}

void UObject::execLocalVariable(FFrame& Stack, RESULT_DECL)
{
	checkSlow(Stack.Object == this);
	checkSlow(Stack.Locals != NULL);

	UProperty* VarProperty = Stack.ReadProperty();
	Stack.MostRecentPropertyAddress = VarProperty->ContainerPtrToValuePtr<uint8>(Stack.Locals);

	if (Result)
	{
		VarProperty->CopyCompleteValueToScriptVM( Result, Stack.MostRecentPropertyAddress );
	}
}
IMPLEMENT_VM_FUNCTION( EX_LocalVariable, execLocalVariable );

void UObject::execInstanceVariable(FFrame& Stack, RESULT_DECL)
{
	UProperty* VarProperty = Stack.ReadProperty();
	Stack.MostRecentPropertyAddress = VarProperty->ContainerPtrToValuePtr<uint8>(this);

	if (Result)
	{
		VarProperty->CopyCompleteValueToScriptVM(Result, Stack.MostRecentPropertyAddress);
	}
}
IMPLEMENT_VM_FUNCTION( EX_InstanceVariable, execInstanceVariable );

void UObject::execLocalOutVariable(FFrame& Stack, RESULT_DECL)
{
	checkSlow(Stack.Object == this);

	// get the property we need to find
	UProperty* VarProperty = Stack.ReadProperty();
	
	// look through the out parameter infos and find the one that has the address of this property
	FOutParmRec* Out = Stack.OutParms;
	checkSlow(Out);
	while (Out->Property != VarProperty)
	{
		Out = Out->NextOutParm;
		checkSlow(Out);
	}
	Stack.MostRecentPropertyAddress = Out->PropAddr;

	// if desired, copy the value in that address to Result
	if (Result && Result != Stack.MostRecentPropertyAddress)
	{
		VarProperty->CopyCompleteValueToScriptVM(Result, Stack.MostRecentPropertyAddress);
	}
}
IMPLEMENT_VM_FUNCTION(EX_LocalOutVariable, execLocalOutVariable);

void UObject::execInterfaceContext(FFrame& Stack, RESULT_DECL)
{
	// get the value of the interface variable
	FScriptInterface InterfaceValue;
	Stack.Step(this, &InterfaceValue);

	if (Result != NULL)
	{
		// copy the UObject pointer to Result
		*(UObject**)Result = InterfaceValue.GetObject();
	}
}
IMPLEMENT_VM_FUNCTION( EX_InterfaceContext, execInterfaceContext );

void UObject::execEndOfScript( FFrame& Stack, RESULT_DECL )
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		UE_LOG(LogScriptCore, Warning, TEXT("--- Dumping bytecode for %s on %s ---"), *Stack.Node->GetFullName(), *Stack.Object->GetFullName());
		const UFunction* Func = Stack.Node;
		for(int32 i = 0; i < Func->Script.Num(); ++i)
		{
			UE_LOG(LogScriptCore, Log, TEXT("0x%x"), Func->Script[i]);
		}
	}
#endif //WITH_EDITOR

	UE_LOG(LogScriptCore, Fatal,TEXT("Execution beyond end of script in %s on %s"), *Stack.Node->GetFullName(), *Stack.Object->GetFullName());
}
IMPLEMENT_VM_FUNCTION( EX_EndOfScript, execEndOfScript );

void UObject::execNothing( FFrame& Stack, RESULT_DECL )
{
	// Do nothing.
}
IMPLEMENT_VM_FUNCTION( EX_Nothing, execNothing );

void UObject::execNothingOp4a( FFrame& Stack, RESULT_DECL )
{
	// Do nothing.
}
IMPLEMENT_VM_FUNCTION( EX_DeprecatedOp4A, execNothingOp4a );

void UObject::execBreakpoint( FFrame& Stack, RESULT_DECL )
{
#if WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		static FBlueprintExceptionInfo BreakpointExceptionInfo(EBlueprintExceptionType::Breakpoint);
		FBlueprintCoreDelegates::ThrowScriptException(this, Stack, BreakpointExceptionInfo);
	}
#endif
}
IMPLEMENT_VM_FUNCTION( EX_Breakpoint, execBreakpoint );

void UObject::execTracepoint( FFrame& Stack, RESULT_DECL )
{
#if WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		static FBlueprintExceptionInfo TracepointExceptionInfo(EBlueprintExceptionType::Tracepoint);
		FBlueprintCoreDelegates::ThrowScriptException(this, Stack, TracepointExceptionInfo);
	}
#endif
}
IMPLEMENT_VM_FUNCTION( EX_Tracepoint, execTracepoint );

void UObject::execWireTracepoint( FFrame& Stack, RESULT_DECL )
{
#if WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		static FBlueprintExceptionInfo TracepointExceptionInfo(EBlueprintExceptionType::WireTracepoint);
		FBlueprintCoreDelegates::ThrowScriptException(this, Stack, TracepointExceptionInfo);
	}
#endif
}
IMPLEMENT_VM_FUNCTION( EX_WireTracepoint, execWireTracepoint );

void UObject::execEndFunctionParms( FFrame& Stack, RESULT_DECL )
{
	// For skipping over optional function parms without values specified.
	Stack.Code--;
}
IMPLEMENT_VM_FUNCTION( EX_EndFunctionParms, execEndFunctionParms );


void UObject::execJump( FFrame& Stack, RESULT_DECL )
{
	CHECK_RUNAWAY;

	// Jump immediate.
	CodeSkipSizeType Offset = Stack.ReadCodeSkipCount();
	Stack.Code = &Stack.Node->Script[Offset];
}
IMPLEMENT_VM_FUNCTION( EX_Jump, execJump );

void UObject::execComputedJump( FFrame& Stack, RESULT_DECL )
{
	CHECK_RUNAWAY;

	// Get the jump offset expression
	int32 ComputedOffset = 0;
	Stack.Step( Stack.Object, &ComputedOffset );
	check((ComputedOffset < Stack.Node->Script.Num()) && (ComputedOffset >= 0));

	// Jump to the new offset
	Stack.Code = &Stack.Node->Script[ComputedOffset];
}
IMPLEMENT_VM_FUNCTION( EX_ComputedJump, execComputedJump );
	
void UObject::execJumpIfNot( FFrame& Stack, RESULT_DECL )
{
	CHECK_RUNAWAY;

	// Get code offset.
	CodeSkipSizeType Offset = Stack.ReadCodeSkipCount();

	// Get boolean test value.
	bool Value=0;
	Stack.Step( Stack.Object, &Value );

	// Jump if false.
	if( !Value )
	{
		Stack.Code = &Stack.Node->Script[ Offset ];
	}
}
IMPLEMENT_VM_FUNCTION( EX_JumpIfNot, execJumpIfNot );

void UObject::execAssert( FFrame& Stack, RESULT_DECL )
{
	// Get line number.
	int32 wLine = Stack.ReadWord();

	// find out whether we are in debug mode and therefore should crash on failure
	uint8 bDebug = *(uint8*)Stack.Code++;

	// Get boolean assert value.
	uint32 Value=0;
	Stack.Step( Stack.Object, &Value );

	// Check it.
	if( !Value )
	{
		Stack.Logf(TEXT("%s"), *Stack.GetStackTrace());
		if (bDebug)
		{
			Stack.Logf(ELogVerbosity::Error, TEXT("Assertion failed, line %i"), wLine);
		}
		else
		{
			UE_SUPPRESS(LogScript, Warning, Stack.Logf(TEXT("Assertion failed, line %i"), wLine));
		}
	}
}
IMPLEMENT_VM_FUNCTION( EX_Assert, execAssert );

void UObject::execPushExecutionFlow( FFrame& Stack, RESULT_DECL )
{
	// Read a code offset and push it onto the flow stack
	CodeSkipSizeType Offset = Stack.ReadCodeSkipCount();
	Stack.FlowStack.Push(Offset);
}
IMPLEMENT_VM_FUNCTION( EX_PushExecutionFlow, execPushExecutionFlow );

void UObject::execPopExecutionFlow( FFrame& Stack, RESULT_DECL )
{
	// Since this is a branch function, check for runaway script execution
	CHECK_RUNAWAY;

	// Try to pop an entry off the stack and go there
	if (Stack.FlowStack.Num())
	{
		CodeSkipSizeType Offset = Stack.FlowStack.Pop();
		Stack.Code = &Stack.Node->Script[ Offset ];
	}
	else
	{
		UE_LOG(LogScriptCore, Log, TEXT("%s"), *Stack.GetStackTrace());
		Stack.Logf(ELogVerbosity::Error, TEXT("Tried to pop from an empty flow stack"));
	}
}
IMPLEMENT_VM_FUNCTION( EX_PopExecutionFlow, execPopExecutionFlow );

void UObject::execPopExecutionFlowIfNot( FFrame& Stack, RESULT_DECL )
{
	// Since this is a branch function, check for runaway script execution
	CHECK_RUNAWAY;

	// Get boolean test value.
	bool Value=0;
	Stack.Step( Stack.Object, &Value );

	if (!Value)
	{
		// Try to pop an entry off the stack and go there
		if (Stack.FlowStack.Num())
		{
			CodeSkipSizeType Offset = Stack.FlowStack.Pop();
			Stack.Code = &Stack.Node->Script[ Offset ];
		}
		else
		{
			UE_LOG(LogScriptCore, Log, TEXT("%s"), *Stack.GetStackTrace());
			Stack.Logf(ELogVerbosity::Error, TEXT("Tried to pop from an empty flow stack"));
		}
	}
}
IMPLEMENT_VM_FUNCTION( EX_PopExecutionFlowIfNot, execPopExecutionFlowIfNot );


void UObject::execLet( FFrame& Stack, RESULT_DECL )
{
	checkSlow(!IsA(UBoolProperty::StaticClass()));

	// Get variable address.
	Stack.MostRecentPropertyAddress = NULL;
	Stack.Step( Stack.Object, NULL ); // Evaluate variable.

	if (Stack.MostRecentPropertyAddress == NULL)
	{
		static FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AccessViolation, TEXT("Attempt to assign variable through None"));
		FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);

		//@TODO: ScriptParallel: Contended static usage
		static uint8 Crud[1024];//@temp
		Stack.MostRecentPropertyAddress = Crud;
		FMemory::Memzero( Stack.MostRecentPropertyAddress, sizeof(FString) );
	}

	// Evaluate expression into variable.
	Stack.Step( Stack.Object, Stack.MostRecentPropertyAddress );
}
IMPLEMENT_VM_FUNCTION( EX_Let, execLet );

void UObject::execLetObj( FFrame& Stack, RESULT_DECL )
{
	// Get variable address.
	Stack.MostRecentPropertyAddress = NULL;
	Stack.Step( Stack.Object, NULL ); // Evaluate variable.

	if (Stack.MostRecentPropertyAddress == NULL)
	{
		static FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AccessViolation, TEXT("Attempt to assign variable through None"));
		FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);
	}

	void* ObjAddr = Stack.MostRecentPropertyAddress;
	UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Stack.MostRecentProperty);
	if (ObjectProperty == NULL)
	{
		UArrayProperty* ArrayProp = ExactCast<UArrayProperty>(Stack.MostRecentProperty);
		if (ArrayProp != NULL)
		{
			ObjectProperty = Cast<UObjectPropertyBase>(ArrayProp->Inner);
		}
	}

	UObject* NewValue = NULL;
	// evaluate the r-value for this expression into Value
	Stack.Step( Stack.Object, &NewValue );

	if (ObjAddr)
	{
		checkSlow(ObjectProperty);
		ObjectProperty->SetObjectPropertyValue(ObjAddr, NewValue);
	}
}
IMPLEMENT_VM_FUNCTION( EX_LetObj, execLetObj );

void UObject::execLetWeakObjPtr( FFrame& Stack, RESULT_DECL )
{
	// Get variable address.
	Stack.MostRecentPropertyAddress = NULL;
	Stack.Step( Stack.Object, NULL ); // Evaluate variable.

	if (Stack.MostRecentPropertyAddress == NULL)
	{
		static FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AccessViolation, TEXT("Attempt to assign variable through None"));
		FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);
	}

	void* ObjAddr = Stack.MostRecentPropertyAddress;
	UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Stack.MostRecentProperty);
	if (ObjectProperty == NULL)
	{
		UArrayProperty* ArrayProp = ExactCast<UArrayProperty>(Stack.MostRecentProperty);
		if (ArrayProp != NULL)
		{
			ObjectProperty = Cast<UObjectPropertyBase>(ArrayProp->Inner);
		}
	}

	TWeakObjectPtr<UObject> NewValue = NULL;
	// evaluate the r-value for this expression into Value
	Stack.Step( Stack.Object, &NewValue );

	if (ObjAddr)
	{
		checkSlow(ObjectProperty);
		ObjectProperty->SetObjectPropertyValue(ObjAddr, NewValue.Get());
	}
}
IMPLEMENT_VM_FUNCTION( EX_LetWeakObjPtr, execLetWeakObjPtr );

void UObject::execLetBool( FFrame& Stack, RESULT_DECL )
{
	Stack.MostRecentPropertyAddress = NULL;
	Stack.MostRecentProperty = NULL;

	// Get the variable and address to place the data.
	Stack.Step( Stack.Object, NULL );

	/*
		Class bool properties are packed together as bitfields, so in order 
		to set the value on the correct bool, we need to mask it against
		the bool property's BitMask.

		Local bool properties (declared inside functions) are not packed, thus
		their bitmask is always 1.

		Bool properties inside dynamic arrays and tmaps are also not packed together.
		If the bool property we're accessing is an element in a dynamic array, Stack.MostRecentProperty
		will be pointing to the dynamic array that has a UBoolProperty as its inner, so
		we'll need to check for that.
	*/
	uint8* BoolAddr = (uint8*)Stack.MostRecentPropertyAddress;
	UBoolProperty* BoolProperty = ExactCast<UBoolProperty>(Stack.MostRecentProperty);
	if (BoolProperty == NULL)
	{
		UArrayProperty* ArrayProp = ExactCast<UArrayProperty>(Stack.MostRecentProperty);
		if (ArrayProp != NULL)
		{
			BoolProperty = ExactCast<UBoolProperty>(ArrayProp->Inner);
		}
	}

	bool NewValue = false;

	// evaluate the r-value for this expression into Value
	Stack.Step( Stack.Object, &NewValue );
	if( BoolAddr )
	{
		checkSlow(BoolProperty->IsA(UBoolProperty::StaticClass()));
		BoolProperty->SetPropertyValue( BoolAddr, NewValue );
	}
}
IMPLEMENT_VM_FUNCTION( EX_LetBool, execLetBool );


void UObject::execLetDelegate( FFrame& Stack, RESULT_DECL )
{
	// Get variable address.
	Stack.MostRecentPropertyAddress = NULL;
	Stack.MostRecentProperty = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.

	FScriptDelegate* DelegateAddr = (FScriptDelegate*)Stack.MostRecentPropertyAddress;
	FScriptDelegate Delegate;
	Stack.Step( Stack.Object, &Delegate );

	if (DelegateAddr != NULL)
	{
		DelegateAddr->SetFunctionName( Delegate.GetFunctionName() );
		DelegateAddr->SetObject( Delegate.GetObject() );
	}
}
IMPLEMENT_VM_FUNCTION( EX_LetDelegate, execLetDelegate );


void UObject::execLetMulticastDelegate( FFrame& Stack, RESULT_DECL )
{
	// Get variable address.
	Stack.MostRecentPropertyAddress = NULL;
	Stack.MostRecentProperty = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.

	FMulticastScriptDelegate* DelegateAddr = (FMulticastScriptDelegate*)Stack.MostRecentPropertyAddress;
	FMulticastScriptDelegate Delegate;
	Stack.Step( Stack.Object, &Delegate );

	if (DelegateAddr != NULL)
	{
		*DelegateAddr = Delegate;
	}
}
IMPLEMENT_VM_FUNCTION( EX_LetMulticastDelegate, execLetMulticastDelegate );


void UObject::execSelf( FFrame& Stack, RESULT_DECL )
{
	// Get Self actor for this context.
	*(UObject**)Result = this;
}
IMPLEMENT_VM_FUNCTION( EX_Self, execSelf );

void UObject::execContext( FFrame& Stack, RESULT_DECL )
{
	ProcessContextOpcode(Stack, Result, /*bCanFailSilently=*/ false);
}
IMPLEMENT_VM_FUNCTION( EX_Context, execContext );

void UObject::execContext_FailSilent( FFrame& Stack, RESULT_DECL )
{
	ProcessContextOpcode(Stack, Result, /*bCanFailSilently=*/ true);
}
IMPLEMENT_VM_FUNCTION( EX_Context_FailSilent, execContext_FailSilent );

void UObject::ProcessContextOpcode( FFrame& Stack, RESULT_DECL, bool bCanFailSilently )
{
	Stack.MostRecentProperty = NULL;
	
	// Get object variable.
	UObject* NewContext = NULL;
	Stack.Step( this, &NewContext );

	// Execute or skip the following expression in the object's context.
	if (NewContext != NULL)
	{
		Stack.Code += sizeof(CodeSkipSizeType) + sizeof(ScriptPointerType) + sizeof(uint8);
		Stack.Step( NewContext, Result );
	}
	else
	{
		if (!bCanFailSilently)
		{
			if (Stack.MostRecentProperty != NULL)
			{
				const FString Desc = FString::Printf(TEXT("Accessed None '%s'"), *Stack.MostRecentProperty->GetName());
				FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AccessViolation, Desc);
				FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);
			}
			else
			{
				// Stack.MostRecentProperty will be NULL under the following conditions:
				//   1. the context expression was a function call which returned an object
				//   2. the context expression was a literal object reference
				static FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AccessViolation, TEXT("Accessed None"));
				FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);
			}
		}

		CodeSkipSizeType wSkip = Stack.ReadCodeSkipCount();
		VariableSizeType bSize = Stack.ReadVariableSize();
		Stack.Code += wSkip;
		Stack.MostRecentPropertyAddress = NULL;
		Stack.MostRecentProperty = NULL;

		if (Result)
		{
			FMemory::Memzero( Result, bSize );
		}
	}
}

void UObject::execStructMemberContext(FFrame& Stack, RESULT_DECL)
{
	// Get the structure element we care about
	UProperty* StructProperty = Stack.ReadProperty();
	checkSlow(StructProperty);

	// Evaluate an expression leading to the struct.
	Stack.MostRecentProperty = NULL;
	Stack.MostRecentPropertyAddress = NULL;
	Stack.Step(Stack.Object, NULL);

	if (Stack.MostRecentProperty != NULL)
	{
		// Offset into the specific member
		Stack.MostRecentPropertyAddress = StructProperty->ContainerPtrToValuePtr<uint8>(Stack.MostRecentPropertyAddress);
		Stack.MostRecentProperty = StructProperty;

		// Handle variable reads
		if (Result)
		{
			StructProperty->CopyCompleteValueToScriptVM(Result, Stack.MostRecentPropertyAddress);
		}
	}
	else
	{
		// Access none
		static FBlueprintExceptionInfo ExceptionInfo(EBlueprintExceptionType::AccessViolation, TEXT("Struct expression was None"));
		FBlueprintCoreDelegates::ThrowScriptException(this, Stack, ExceptionInfo);

		Stack.MostRecentPropertyAddress = NULL;
		Stack.MostRecentProperty = NULL;
	}
}
IMPLEMENT_VM_FUNCTION( EX_StructMemberContext, execStructMemberContext );

void UObject::execVirtualFunction( FFrame& Stack, RESULT_DECL )
{
	// Call the virtual function.
	CallFunction( Stack, Result, FindFunctionChecked(Stack.ReadName()) );
}
IMPLEMENT_VM_FUNCTION( EX_VirtualFunction, execVirtualFunction );

void UObject::execFinalFunction( FFrame& Stack, RESULT_DECL )
{
	// Call the final function.
	CallFunction( Stack, Result, (UFunction*)Stack.ReadObject() );
}
IMPLEMENT_VM_FUNCTION( EX_FinalFunction, execFinalFunction );

class FCallDelegateHelper
{
public:
	static void CheckEqualParamSize(const FMulticastScriptDelegate& MulticastDelegate, const UFunction* SignatureFunction)
	{
		check(SignatureFunction);
		const uint32 ParamSize = SignatureFunction->ParmsSize;
		for (auto FunctionIt = MulticastDelegate.InvocationList.CreateConstIterator(); FunctionIt; ++FunctionIt)
		{
			if( FunctionIt->IsBound() )
			{
				const UObject* Obj = FunctionIt->GetObject();
				FName FunctionName = FunctionIt->GetFunctionName();
				if(Obj && (FunctionName != NAME_None))
				{
					UFunction* Function = Obj->FindFunctionChecked( FunctionName );
					check(Function->ParmsSize == ParamSize);
					check(SignatureFunction->IsSignatureCompatibleWith(Function));
				}
			}
		}
	}

	static void CallMulticastDelegate(FFrame& Stack)
	{
		//Get delegate
		UFunction* SignatureFunction = CastChecked<UFunction>(Stack.ReadObject());
		Stack.MostRecentPropertyAddress = NULL;
		Stack.MostRecentProperty = NULL;
		Stack.Step( Stack.Object, NULL );
		const FMulticastScriptDelegate* DelegateAddr = (FMulticastScriptDelegate*)Stack.MostRecentPropertyAddress;
		check(NULL != DelegateAddr);
		CheckEqualParamSize(*DelegateAddr, SignatureFunction);

		//Fill parameters
		uint8* Parameters = (uint8*)FMemory_Alloca(SignatureFunction->ParmsSize);
		FMemory::Memzero(Parameters, SignatureFunction->ParmsSize);
		for (UProperty* Property = (UProperty*)SignatureFunction->Children; *Stack.Code != EX_EndFunctionParms; Property = (UProperty*)Property->Next)
		{
			Stack.MostRecentPropertyAddress = NULL;
			if (Property->PropertyFlags & CPF_OutParm)
			{
				Stack.Step(Stack.Object, NULL);
				if(NULL != Stack.MostRecentPropertyAddress)
				{
					check(Property->IsInContainer(SignatureFunction->ParmsSize));
					uint8* ConstRefCopyParamAdress = Property->ContainerPtrToValuePtr<uint8>(Parameters);
					Property->CopyCompleteValueToScriptVM(ConstRefCopyParamAdress, Stack.MostRecentPropertyAddress);
				}
			}
			else
			{
				uint8* Param = Property->ContainerPtrToValuePtr<uint8>(Parameters);
				checkSlow(Param);
				Stack.Step(Stack.Object, Param);
			}
		}
		Stack.Code++;

		//Process delegate
		DelegateAddr->ProcessMulticastDelegate<UObject>(Parameters);
		
		//Clean parameters
		for (UProperty* Destruct = SignatureFunction->DestructorLink; Destruct; Destruct = Destruct->DestructorLinkNext)
		{
			Destruct->DestroyValue_InContainer(Parameters);
		}
	}
};

void UObject::execCallMulticastDelegate( FFrame& Stack, RESULT_DECL )
{
	FCallDelegateHelper::CallMulticastDelegate(Stack);
}
IMPLEMENT_VM_FUNCTION( EX_CallMulticastDelegate, execCallMulticastDelegate );

void UObject::execAddMulticastDelegate( FFrame& Stack, RESULT_DECL )
{
	// Get variable address.
	Stack.MostRecentPropertyAddress = NULL;
	Stack.MostRecentProperty = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.

	FMulticastScriptDelegate* DelegateAddr = (FMulticastScriptDelegate*)Stack.MostRecentPropertyAddress;
	FScriptDelegate Delegate;
	Stack.Step( Stack.Object, &Delegate );

	if (DelegateAddr != NULL)
	{
		DelegateAddr->AddUnique(Delegate);
	}
}
IMPLEMENT_VM_FUNCTION( EX_AddMulticastDelegate, execAddMulticastDelegate );

void UObject::execRemoveMulticastDelegate( FFrame& Stack, RESULT_DECL )
{
	// Get variable address.
	Stack.MostRecentPropertyAddress = NULL;
	Stack.MostRecentProperty = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.

	FMulticastScriptDelegate* DelegateAddr = (FMulticastScriptDelegate*)Stack.MostRecentPropertyAddress;
	FScriptDelegate Delegate;
	Stack.Step( Stack.Object, &Delegate );

	if (DelegateAddr != NULL)
	{
		DelegateAddr->Remove(Delegate);
	}
}
IMPLEMENT_VM_FUNCTION( EX_RemoveMulticastDelegate, execRemoveMulticastDelegate );

void UObject::execClearMulticastDelegate( FFrame& Stack, RESULT_DECL )
{
	// Get the delegate address
	Stack.MostRecentPropertyAddress = NULL;
	Stack.MostRecentProperty = NULL;
	Stack.Step( Stack.Object, NULL );

	FMulticastScriptDelegate* DelegateAddr = (FMulticastScriptDelegate*)Stack.MostRecentPropertyAddress;
	if (DelegateAddr != NULL)
	{
		DelegateAddr->Clear();
	}
}
IMPLEMENT_VM_FUNCTION( EX_ClearMulticastDelegate, execClearMulticastDelegate );

void UObject::execIntConst( FFrame& Stack, RESULT_DECL )
{
	*(int32*)Result = Stack.ReadInt();
}
IMPLEMENT_VM_FUNCTION( EX_IntConst, execIntConst );

void UObject::execSkipOffsetConst( FFrame& Stack, RESULT_DECL )
{
	CodeSkipSizeType Literal = Stack.ReadCodeSkipCount();
	*(int32*)Result = Literal;
}
IMPLEMENT_VM_FUNCTION( EX_SkipOffsetConst, execSkipOffsetConst );

void UObject::execFloatConst( FFrame& Stack, RESULT_DECL )
{
	*(float*)Result = Stack.ReadFloat();
}
IMPLEMENT_VM_FUNCTION( EX_FloatConst, execFloatConst );

void UObject::execStringConst( FFrame& Stack, RESULT_DECL )
{
	*(FString*)Result = (ANSICHAR*)Stack.Code;
	while( *Stack.Code )
		Stack.Code++;
	Stack.Code++;
}
IMPLEMENT_VM_FUNCTION( EX_StringConst, execStringConst );

void UObject::execUnicodeStringConst( FFrame& Stack, RESULT_DECL )
{
 	*(FString*)Result = FString((UCS2CHAR*)Stack.Code);

	while( *(uint16*)Stack.Code )
	{
		Stack.Code+=sizeof(uint16);
	}
	Stack.Code+=sizeof(uint16);
}
IMPLEMENT_VM_FUNCTION( EX_UnicodeStringConst, execUnicodeStringConst );

void UObject::execTextConst( FFrame& Stack, RESULT_DECL )
{
	FString SourceString;
	FString KeyString;
	FString Namespace;
	Stack.Step( Stack.Object, &SourceString);
	Stack.Step( Stack.Object, &KeyString);
	Stack.Step( Stack.Object, &Namespace);
	*(FText*)Result = FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(*SourceString, *Namespace, *KeyString);
}
IMPLEMENT_VM_FUNCTION( EX_TextConst, execTextConst );

void UObject::execObjectConst( FFrame& Stack, RESULT_DECL )
{
	*(UObject**)Result = (UObject*)Stack.ReadObject();
}
IMPLEMENT_VM_FUNCTION( EX_ObjectConst, execObjectConst );

void UObject::execInstanceDelegate( FFrame& Stack, RESULT_DECL )
{
	FName FunctionName = Stack.ReadName();

	((FScriptDelegate*)Result)->SetFunctionName( FunctionName );
	((FScriptDelegate*)Result)->SetObject( (FunctionName == NAME_None) ? NULL : this );
}
IMPLEMENT_VM_FUNCTION( EX_InstanceDelegate, execInstanceDelegate );

void UObject::execBindDelegate( FFrame& Stack, RESULT_DECL )
{
	FName FunctionName = Stack.ReadName();

	// Get delegate address.
	Stack.MostRecentPropertyAddress = NULL;
	Stack.MostRecentProperty = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.

	FScriptDelegate* DelegateAddr = (FScriptDelegate*)Stack.MostRecentPropertyAddress;

	UObject* ObjectForDelegate = NULL;
	Stack.Step(Stack.Object, &ObjectForDelegate);

	if (DelegateAddr)
	{
		DelegateAddr->SetFunctionName(FunctionName);
		DelegateAddr->SetObject(ObjectForDelegate);
	}
}
IMPLEMENT_VM_FUNCTION( EX_BindDelegate, execBindDelegate );

void UObject::execNameConst( FFrame& Stack, RESULT_DECL )
{
	*(FName*)Result = Stack.ReadName();
}
IMPLEMENT_VM_FUNCTION( EX_NameConst, execNameConst );

void UObject::execByteConst( FFrame& Stack, RESULT_DECL )
{
	*(uint8*)Result = *Stack.Code++;
}
IMPLEMENT_VM_FUNCTION( EX_ByteConst, execByteConst );

void UObject::execRotationConst( FFrame& Stack, RESULT_DECL )
{
	((FRotator*)Result)->Pitch = Stack.ReadFloat();
	((FRotator*)Result)->Yaw   = Stack.ReadFloat();
	((FRotator*)Result)->Roll  = Stack.ReadFloat();
}
IMPLEMENT_VM_FUNCTION( EX_RotationConst, execRotationConst );

void UObject::execVectorConst( FFrame& Stack, RESULT_DECL )
{
	((FVector*)Result)->X = Stack.ReadFloat();
	((FVector*)Result)->Y = Stack.ReadFloat();
	((FVector*)Result)->Z = Stack.ReadFloat();
}
IMPLEMENT_VM_FUNCTION( EX_VectorConst, execVectorConst );

void UObject::execTransformConst( FFrame& Stack, RESULT_DECL )
{
	// Rotation
	FQuat TmpRotation;
	TmpRotation.X = Stack.ReadFloat();
	TmpRotation.Y = Stack.ReadFloat();
	TmpRotation.Z = Stack.ReadFloat();
	TmpRotation.W = Stack.ReadFloat();

	// Translation
	FVector TmpTranslation;
	TmpTranslation.X = Stack.ReadFloat();
	TmpTranslation.Y = Stack.ReadFloat();
	TmpTranslation.Z = Stack.ReadFloat();

	// Scale
	FVector TmpScale;
	TmpScale.X = Stack.ReadFloat();
	TmpScale.Y = Stack.ReadFloat();
	TmpScale.Z = Stack.ReadFloat();

	((FTransform*)Result)->SetComponents(TmpRotation, TmpTranslation, TmpScale);
}
IMPLEMENT_VM_FUNCTION( EX_TransformConst, execTransformConst );

void UObject::execStructConst( FFrame& Stack, RESULT_DECL )
{
	UScriptStruct* ScriptStruct = CastChecked<UScriptStruct>(Stack.ReadObject());
	int32 SerializedSize = Stack.ReadInt();

	// Temporarily disabling this check because we can't assume the serialized size
	// will match the struct size on all platforms (like win64 vs win32 cooked)
	//check( ScriptStruct->GetStructureSize() == SerializedSize );
	
	for( UProperty* StructProp = ScriptStruct->PropertyLink; StructProp; StructProp = StructProp->PropertyLinkNext )
	{
		// Const struct arrays aren't supported yet
		if( StructProp->IsA(UArrayProperty::StaticClass()) )
		{
			continue;
		}

		Stack.Step(Stack.Object, StructProp->ContainerPtrToValuePtr<uint8>(Result));
	}

	P_FINISH;	// EX_EndStructConst
}
IMPLEMENT_VM_FUNCTION( EX_StructConst, execStructConst );

void UObject::execSetArray( FFrame& Stack, RESULT_DECL )
{
	// Get the array address
	Stack.MostRecentPropertyAddress = NULL;
	Stack.MostRecentProperty = NULL;
	Stack.Step( Stack.Object, NULL ); // Array to set
	
	UArrayProperty* ArrayProperty = CastChecked<UArrayProperty>(Stack.MostRecentProperty);
 	FScriptArrayHelper ArrayHelper(ArrayProperty, Stack.MostRecentPropertyAddress);
 	ArrayHelper.EmptyValues();
 
 	// Read in the parameters one at a time
 	int32 i = 0;
 	while(*Stack.Code != EX_EndArray)
 	{
 		ArrayHelper.AddValues(1);
 		Stack.Step(Stack.Object, ArrayHelper.GetRawPtr(i++));
 	}
 
 	P_FINISH;
}
IMPLEMENT_VM_FUNCTION( EX_SetArray, execSetArray );

void UObject::execIntZero( FFrame& Stack, RESULT_DECL )
{
	*(int32*)Result = 0;
}
IMPLEMENT_VM_FUNCTION( EX_IntZero, execIntZero );

void UObject::execIntOne( FFrame& Stack, RESULT_DECL )
{
	*(int32*)Result = 1;
}
IMPLEMENT_VM_FUNCTION( EX_IntOne, execIntOne );

void UObject::execTrue( FFrame& Stack, RESULT_DECL )
{
	*(bool*)Result = true;
}
IMPLEMENT_VM_FUNCTION( EX_True, execTrue );

void UObject::execFalse( FFrame& Stack, RESULT_DECL )
{
	*(bool*)Result = false;
}
IMPLEMENT_VM_FUNCTION( EX_False, execFalse );

void UObject::execNoObject( FFrame& Stack, RESULT_DECL )
{
	*(UObject**)Result = NULL;
}
IMPLEMENT_VM_FUNCTION( EX_NoObject, execNoObject );

void UObject::execIntConstByte( FFrame& Stack, RESULT_DECL )
{
	*(int32*)Result = *Stack.Code++;
}
IMPLEMENT_VM_FUNCTION( EX_IntConstByte, execIntConstByte );


void UObject::execDynamicCast( FFrame& Stack, RESULT_DECL )
{
	// Get "to cast to" class for the dynamic actor class
	UClass* Class = (UClass *)Stack.ReadObject();

	// Compile object expression.
	UObject* Castee = NULL;
	Stack.Step( Stack.Object, &Castee );
	//*(UObject**)Result = (Castee && Castee->IsA(Class)) ? Castee : NULL;
	*(UObject**)Result = NULL; // default value


	// if we were passed in a null value
	if( Castee == NULL )
	{
		if( Class->HasAnyClassFlags(CLASS_Interface) )
		{
			((FScriptInterface*)Result)->SetObject(NULL);
		}
		else
		{
			*(UObject**)Result = NULL;
		}
		return;
	}

	// check to see if the Castee is an implemented interface by looking up the
	// class hierarchy and seeing if any class in said hierarchy implements the interface
	if( Class->HasAnyClassFlags(CLASS_Interface) )
	{
		if ( Castee->GetClass()->ImplementsInterface(Class) )
		{
			// interface property type - convert to FScriptInterface
			((FScriptInterface*)Result)->SetObject(Castee);
			((FScriptInterface*)Result)->SetInterface(Castee->GetInterfaceAddress(Class));
		}
	}
	// check to see if the Castee is a castable class
	else if( Castee->IsA(Class) )
	{
		*(UObject**)Result = Castee;
	}
}
IMPLEMENT_VM_FUNCTION( EX_DynamicCast, execDynamicCast );

void UObject::execMetaCast( FFrame& Stack, RESULT_DECL )
{
	UClass* MetaClass = (UClass*)Stack.ReadObject();

	// Compile actor expression.
	UObject* Castee=NULL;
	Stack.Step( Stack.Object, &Castee );
	*(UObject**)Result = (Castee && Castee->IsA(UClass::StaticClass()) && ((UClass*)Castee)->IsChildOf(MetaClass)) ? Castee : NULL;
}
IMPLEMENT_VM_FUNCTION( EX_MetaCast, execMetaCast );

void UObject::execPrimitiveCast( FFrame& Stack, RESULT_DECL )
{
	int32 B = *(Stack.Code)++;
	(Stack.Object->*GCasts[B])( Stack, Result );
}
IMPLEMENT_VM_FUNCTION( EX_PrimitiveCast, execPrimitiveCast );

void UObject::execInterfaceCast( FFrame& Stack, RESULT_DECL )
{
	(Stack.Object->*GCasts[CST_ObjectToInterface])(Stack, Result);
}
IMPLEMENT_VM_FUNCTION( EX_InterfaceCast, execInterfaceCast );


void UObject::execObjectToBool( FFrame& Stack, RESULT_DECL )
{
	UObject* Obj=NULL;
	Stack.Step( Stack.Object, &Obj );
	*(bool*)Result = Obj != NULL;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ObjectToBool, execObjectToBool );

void UObject::execInterfaceToBool( FFrame& Stack, RESULT_DECL )
{
	FScriptInterface Interface;
	Stack.Step( Stack.Object, &Interface);
	*(bool*)Result = (Interface.GetObject() != NULL);
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_InterfaceToBool, execInterfaceToBool );

void UObject::execObjectToInterface( FFrame& Stack, RESULT_DECL )
{
	FScriptInterface& InterfaceValue = *(FScriptInterface*)Result;

	// read the interface class off the stack
	UClass* InterfaceClass = Cast<UClass>(Stack.ReadObject());
	checkSlow(InterfaceClass != NULL);

	// read the object off the stack
	UObject* ObjectValue = NULL;
	Stack.Step( Stack.Object, &ObjectValue );

	if ( ObjectValue && ObjectValue->GetClass()->ImplementsInterface(InterfaceClass) )
	{
		InterfaceValue.SetObject(ObjectValue);

		void* IAddress = ObjectValue->GetInterfaceAddress(InterfaceClass);
		InterfaceValue.SetInterface(IAddress);
	}
	else
	{
		InterfaceValue.SetObject(NULL);
	}
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ObjectToInterface, execObjectToInterface );
