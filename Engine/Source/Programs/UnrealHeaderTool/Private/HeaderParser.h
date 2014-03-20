// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HeaderParser.h: Parses annotated C++ headers to generate additional code and metadata.
=============================================================================*/

#pragma once

#include "ParserHelper.h"
#include "BaseParser.h"

/*-----------------------------------------------------------------------------
	Constants & types.
-----------------------------------------------------------------------------*/

enum {MAX_NEST_LEVELS = 16};

/* Code nesting types. */
enum ENestType
{
	NEST_GlobalScope,
	NEST_Class,
	NEST_FunctionDeclaration,
	NEST_Interface
};

/** Types of statements to allow within a particular nesting block. */
enum ENestAllowFlags
{
	ALLOW_Function			= 1,	// Allow Event declarations at this level.
	ALLOW_VarDecl			= 2,	// Allow variable declarations at this level.
	ALLOW_Class				= 4,	// Allow class definition heading.
	ALLOW_Return			= 8,	// Allow 'return' within a function.
	ALLOW_TypeDecl			= 16,	// Allow declarations which do not affect memory layout, such as structs, enums, and consts
};

/** Types access specifiers. */
enum EAccessSpecifier
{
	ACCESS_NotAnAccessSpecifier	= 0,	
	ACCESS_Public,
	ACCESS_Private,
	ACCESS_Protected,
	ACCESS_Num,
};

namespace EDelegateSpecifierAction
{
	enum Type
	{
		DontParse,
		Parse
	};
}

/** Information for a particular nesting level. */
struct FNestInfo
{
	/** Link to the stack node. */
	UStruct* Node;

	/** Statement that caused the nesting. */
	ENestType NestType;

	/** Types of statements to allow at this nesting level. */
	int32 Allow;
};

/////////////////////////////////////////////////////
// FHeaderParser

//
// Header parser class.  Extracts metadata from annotated C++ headers and gathers enough
// information to autogenerate additional headers and other boilerplate code.
//
class FHeaderParser : public FBaseParser, public FContextSupplier
{
public:
	// Parse all headers for classes that are inside LimitOuter.
	static bool ParseAllHeadersInside(FFeedbackContext* Warn, UPackage* LimitOuter, bool bAllowSaveExportedHeaders, bool bUseRelativePaths);

	// Performs a preliminary parse of the text in the specified buffer, pulling out:
	//   Class name and parent class name
	//   Is it an interface
	//   The list of other classes/interfaces it is dependent on
	//   
	//  It also splits the buffer up into:
	//   ScriptText (text outside of #if CPP and #if DEFAULTS blocks)
	static void SimplifiedClassParse(const TCHAR* Buffer, bool& bIsInterface, TArray<FName>& DependentOn, FString& out_ClassName, FString& out_ParentClassName, int32& out_ClassDeclLine, FStringOutputDevice& ScriptText);
	
	/** 
	 * Attempts to get class prefix. If the given class name does not start with a valid Unreal Prefix, it will return an empty string.
	 *
	 * @param InClassName - Name w/ potential prefix to check
	 */
	static FString GetClassPrefix( const FString InClassName );

	/** 
	 * Attempts to get class prefix. If the given class name does not start with a valid Unreal Prefix, it will return an empty string.
	 *
	 * @param InClassName - Name w/ potential prefix to check
	 * @param out bIsLabledDeprecated - Reference param set to True if the class name is marked as deprecated
	 */
	static FString GetClassPrefix( const FString InClassName, bool& bIsLabledDeprecated );

	/** 
	 * Returns True if the given class name includes a valid Unreal prefix and matches up with the given original class Name.
	 *
	 * @param InNameToCheck - Name w/ potential prefix to check
	 * @param OriginalClassName - Name of class w/ no prefix to check against
	 */
	static bool ClassNameHasValidPrefix(const FString InNameToCheck, const FString OriginalClassName);
	/** 
	 * Returns True if the given class name includes a valid Unreal prefix and matches based on the given class.
	 *
	 * @param InNameToCheck - Name w/ potential prefix to check
	 * @param OriginalClass - Class to check against
	 */
	static bool ClassNameHasValidPrefix(const FString InNameToCheck, const UClass* OriginalClass);

	/** 
	 * Attempts to strip the given class name of it's affixed prefix. If no prefix exists, will return a blank string.
	 *
	 * @param InClassName - Class Name with a prefix to be stripped
	 */
	static FString GetClassNameWithPrefixRemoved( const FString InClassName );

	/** 
	 * Attempts to strip the given class name of it's affixed prefix. If no prefix exists, will return unchanged string.
	 *
	 * @param InClassName - Class Name with a prefix to be stripped
	 */
	static FString GetClassNameWithoutPrefix( const FString& InClassNameOrFilename );

	/**
	 * Tries to convert the header file name to a class name (with 'U' prefix)
	 *
	 * @param HeaderFilename Filename.
	 * @param OutClass The resulting class name (if successfull)
	 * @return true if the filename was a header filename (.h), false otherwise (in which case OutClassName is unmodified).
	 */
	static bool DependentClassNameFromHeader(const TCHAR* HeaderFilename, FString& OutClassName);

	/**
	 * Generates temporary class name (without 'u' prefix) from header name (no path, no extension).
	 *
	 * @param HeaderName Header name without path nor extension.
	 * @return Temporary class name.
	 */
	static FString GenerateTemporaryClassName(const TCHAR* HeaderName)
	{
		return FString(TEXT("TemporaryUHTHeader_")) + HeaderName;
	}

	/**
	 * Transforms cpp-formated string containing default value, to inner formated string
	 * If it cannot be transormed empty string is returned.
	 *
	 * @param Property - property, that owns the default value
	 * @param CppForm -  cpp-formated string
	 * @param out InnerForm -  inner formated string
	 *
	 * @return if succedded
	 */
	static bool DefaultValueStringCppFormatToInnerFormat(const UProperty* Property, const FString& CppForm, FString &InnerForm);

protected:
	friend struct FScriptLocation;

	// For compiling messages and errors.
	FFeedbackContext* Warn;

	// Class that is currently being parsed.
	UClass* Class;

	// Filename currently being parsed
	FString Filename;

	// Was the first include in the file a validly formed autogenerated header include?
	bool bSpottedAutogeneratedHeaderInclude;

	// Current nest level, starts at 0.
	int32 NestLevel;

	// Top nesting level.
	FNestInfo* TopNest;

	// Top stack node.
	UStruct* TopNode;

	// Information about all nesting levels.
	FNestInfo Nest[MAX_NEST_LEVELS];

	// enum for complier directives used to build up the directive stack
	struct ECompilerDirective
	{
		enum Type 
		{
			// this directive is insignificant and does not change the codegeneration at all
			Insignificant			= 0,
			// this indicates we are in a WITH_EDITOR #if-Block
			WithEditor				= 1<<0,
			// this indicates we are in a WITH_EDITORONLY_DATA #if-Block
			WithEditorOnlyData		= 1<<1,
		};
	};

	/** 
	 * Compiler directive nest in which the parser currently is
	 * NOTE: compiler directives are combined when more are added onto the stack, so
	 * checking the only the top of stack is enough to determine in which #if-Block(s) the current code
	 * is.
	 *
	 * ex. Stack.Num() == 1 while entering #if WITH_EDITOR: 
	 *		CompilerDirectiveStack[1] == CompilerDirectiveStack[0] | ECompilerDirective::WithEditor == 
	 *		CompilerDirecitveStack[1] == CompilerDirectiveStack.Num()-1 | ECompilerDirective::WithEditor
	 *
	 * ex. Stack.Num() == 2 while entering #if WITH_EDITOR: 
	 *		CompilerDirectiveStack[3] == CompilerDirectiveStack[0] | CompilerDirectiveStack[1] | CompilerDirectiveStack[2] | ECompilerDirective::WithEditor ==
	 *		CompilerDirecitveStack[3] == CompilerDirectiveStack.Num()-1 | ECompilerDirective::WithEditor
	 */
	TArray<uint32> CompilerDirectiveStack;

	// Pushes the Directive specified to the CompilerDirectiveStack according to the rules described above
	void FORCEINLINE PushCompilerDirective(ECompilerDirective::Type Directive)
	{
		CompilerDirectiveStack.Push(CompilerDirectiveStack.Num()>0 ? (CompilerDirectiveStack[CompilerDirectiveStack.Num()-1] | Directive) : Directive);
	}

	/**
	 * The starting class flags (i.e. the class flags that were set before the
	 * CLASS_RecompilerClear mask was applied) for the class currently being compiled
	 */
	uint32 PreviousClassFlags;

	//
	FClassMetaData* ClassData;

	// Have seen the first of the two expected classes in the new style .h files?
	bool bHaveSeenFirstInterfaceClass;

	// Have seen the second of the two expected classes in the new style .h files?
	bool bHaveSeenSecondInterfaceClass;

	// Indicates that both interface classes have been parsed.
	bool bFinishedParsingInterfaceClasses;

	// For new-style classes, used to keep track of an unmatched {} pair
	bool bEncounteredNewStyleClass_UnmatchedBrackets;

	// Indicates that UCLASS/USTRUCT/UINTERFACE has already been parsed in this .h file..
	bool bHaveSeenUClass;

	// Indicates that a GENERATED_UCLASS_BODY has been found in the UClass.
	bool bClassHasGeneratedBody;

	// public, private, etc at the current parse spot
	EAccessSpecifier CurrentAccessSpecifier;

	////////////////////////////////////////////////////

	// Special parsed struct names that do not require a prefix
	TArray<FString> StructsWithNoPrefix;
	
	// Special parsed struct names that have a 'T' prefix
	TArray<FString> StructsWithTPrefix;

	// List of all legal variable specifier tokens
	TSet<FString> LegalVariableSpecifiers;

	// Mapping from 'human-readable' macro substring to # of parameters for delegate declarations
	// Index 0 is 1 parameter, Index 1 is 2, etc...
	TArray<FString> DelegateParameterCountStrings;

	// List of all used identifiers for net service function declarations (every function must be unique)
	TMap<int32, FString> UsedRPCIds;
	// List of all net service functions with undeclared response functions 
	TMap<int32, FString> RPCsNeedingHookup;

protected:
	// Constructor.
	FHeaderParser(FFeedbackContext* InWarn);

	~FHeaderParser()
	{
		if ( FScriptLocation::Compiler == this )
		{
			FScriptLocation::Compiler = NULL;
		}
	}

	// Returns true if the token is a variable specifier
	bool IsValidVariableSpecifier(const FToken& Token) const;

	// Returns true if the token is a dynamic delegate declaration
	bool IsValidDelegateDeclaration(const FToken& Token) const;

	// Returns true if the current token is a bitfield type
	bool IsBitfieldProperty();

	// Parse the parameter list of a function or delegate declaration
	void ParseParameterList(UFunction* Function, bool bExpectCommaBeforeName = false, TMap<FName, FString>* MetaData = NULL);

	// Throws if a specifier value wasn't provided
	void RequireSpecifierValue(const FPropertySpecifier& Specifier, bool bRequireExactlyOne = false);
	FString RequireExactlyOneSpecifierValue(const FPropertySpecifier& Specifier);

	/**
	 * Parse Class's annotated headers and optionally its child classes.  Marks the class as CLASS_Parsed.
	 *
	 * @param	AllClasses			the class tree containing all classes in the current package
	 * @param	HeaderParser		the header parser
	 * @param	Class				the class to parse
	 * @param	bParseSubclasses	true if we should parse all child classes of Class
	 *
	 * @return	true if the class was successfully compiled, false otherwise
	 */
	static bool ParseHeaders(FClassTree& AllClasses, FHeaderParser& HeaderParser, UClass* Class, bool bParseSubclasses);

	//@TODO: Remove this method
	static void ParseClassName(const TCHAR* Temp, FString& ClassName);

	/**
	 * @param		Input		An input string, expected to be a script comment.
	 * @return					The input string, reformatted in such a way as to be appropriate for use as a tooltip.
	 */
	static FString FormatCommentForToolTip(const FString& Input);
	
	/**
	 * Begins the process of exporting C++ class declarations for native classes in the specified package
	 * 
	 * @param	CurrentPackage	the package being compiled
	 * @param	AllClasses		the class tree for CurrentPackage
	 */
	static void ExportNativeHeaders( UPackage* CurrentPackage, FClassTree& AllClasses, bool bAllowSaveExportedHeaders, bool bUseRelativePaths );

	// FContextSupplier interface.
	virtual FString GetContext() OVERRIDE;
	// End of FContextSupplier interface.

	// High-level compiling functions.
	bool ParseHeaderForOneClass(FClassTree& AllClasses, UClass* InClass);
	void CompileDirective(UClass* Class);
	void FinalizeScriptExposedFunctions(UClass* Class);
	UEnum* CompileEnum(UClass* Owner);
	UScriptStruct* CompileStructDeclaration(UClass* Owner);
	bool CompileDeclaration(FToken& Token);
	/** Skip C++ (noexport) declaration. */
	bool SkipDeclaration(FToken& Token);
	/** Similar to MatchSymbol() but will return to the exact location as on entry if the symbol was not found. */
	bool SafeMatchSymbol(const TCHAR* Match);
	void HandleOneInheritedClass(FString InterfaceName);
	void ParseClassNameDeclaration(FString& DeclaredClassName, FString& RequiredAPIMacroIfPresent);

	/** The property style of a variable declaration being parsed */
	struct EPropertyDeclarationStyle
	{
		enum Type
		{
			None,
			UPROPERTY
		};
	};

	void CompileClassDeclaration    ();
	void CompileDelegateDeclaration (const TCHAR* DelegateIdentifier, EDelegateSpecifierAction::Type SpecifierAction = EDelegateSpecifierAction::DontParse);
	void CompileFunctionDeclaration ();
	void CompileVariableDeclaration (UStruct* Struct, EPropertyDeclarationStyle::Type PropertyDeclarationStyle);
	void CompileInterfaceDeclaration();

	void ParseInterfaceNameDeclaration(FString& DeclaredInterfaceName, FString& RequiredAPIMacroIfPresent);
	void ParseSecondInterfaceClass();

	bool CompileStatement();

	// Compute the function parameter size and save the return offset
	static void ComputeFunctionParametersSize( UClass* InClass );

	// Checks to see if a particular kind of command is allowed on this nesting level.
	bool IsAllowedInThisNesting(uint32 AllowFlags);
	
	// Make sure that a particular kind of command is allowed on this nesting level.
	// If it's not, issues a compiler error referring to the token and the current
	// nesting level.
	void CheckAllow(const TCHAR* Thing, uint32 AllowFlags);

	void CheckInScope( UObject* Obj );

	UStruct* GetSuperScope( UStruct* CurrentScope, const FName& SearchName );

	/**
	 * Find a field in the specified context.  Starts with the specified scope, then iterates
	 * through the Outer chain until the field is found.
	 * 
	 * @param	InScope				scope to start searching for the field in 
	 * @param	InIdentifier		name of the field we're searching for
	 * @param	bIncludeParents		whether to allow searching in the scope of a parent struct
	 * @param	FieldClass			class of the field to search for.  used to e.g. search for functions only
	 * @param	Thing				hint text that will be used in the error message if an error is encountered
	 *
	 * @return	a pointer to a UField with a name matching InIdentifier, or NULL if it wasn't found
	 */
	UField* FindField( UStruct* InScope, const TCHAR* InIdentifier, bool bIncludeParents=true, UClass* FieldClass=UField::StaticClass(), const TCHAR* Thing=NULL );
	void SkipStatements( int32 SubCount, const TCHAR* ErrorTag );

	/** The category of variable declaration being parsed */
	struct EVariableCategory
	{
		enum Type
		{
			RegularParameter,
			ReplicatedParameter,
			Return,
			Member
		};
	};

	/**
	 * Parses a variable or return value declaration and determines the variable type and property flags.
	 *
	 * @param   Scope                     struct to create the property in
	 * @param   VarProperty               will be filled in with type and property flag data for the property declaration that was parsed
	 * @param   ObjectFlags               will contain the object flags that should be assigned to all UProperties which are part of this declaration (will not be determined
	 *                                    until GetVarNameAndDim is called for this declaration).
	 * @param   Disallow                  contains a mask of variable modifiers that are disallowed in this context
	 * @param   Thing                     used for compiler errors to provide more information about the type of parsing that was occurring
	 * @param   OuterPropertyType         only specified when compiling the inner properties for arrays or maps.  corresponds to the FToken for the outer property declaration.
	 * @param   PropertyDeclarationStyle  if the variable is defined with a UPROPERTY
	 * @param   VariableCategory          what kind of variable is being parsed
	 *
	 * @return  true if the variable type was parsed
	 */
	bool GetVarType(UStruct* Scope, FPropertyBase& VarProperty, EObjectFlags& ObjectFlags, uint64 Disallow, const TCHAR* Thing, FToken* OuterPropertyType, EPropertyDeclarationStyle::Type PropertyDeclarationStyle, EVariableCategory::Type VariableCategory);

	/**
	 * Parses a variable name declaration and creates a new UProperty object.
	 *
	 * @param	Scope			struct to create the property in
	 * @param	VarProperty		type and propertyflag info for the new property (inout)
	 * @param	ObjectFlags		flags to pass on to the new property
	 * @param	NoArrays		true if static arrays are disallowed
	 * @param	IsFunction		true if the property is a function parameter or return value
	 * @param	HardcodedName	name to assign to the new UProperty, if specified. primarily used for function return values,
	 *							which are automatically called "ReturnValue"
	 * @param	HintText		text to use in error message if error is encountered
	 *
	 * @return	a pointer to the new UProperty if successful, or NULL if there was no property to parse
	 */
	UProperty* GetVarNameAndDim(
		UStruct* Struct,
		FToken& VarProperty,
		EObjectFlags ObjectFlags,
		bool NoArrays,
		bool IsFunction,
		const TCHAR* HardcodedName,
		const TCHAR* Thing);
	
	void CheckObscures(UStruct* Scope, const FString& ScriptName, const FString& FieldName);
	
	bool AllowReferenceToClass( UClass* CheckClass ) const;

	/**
	 * @return	true if Scope has UProperty objects in its list of fields
	 */
	static bool HasMemberProperties( const UStruct* Scope );

	/**
	 * Ensures at script compile time that the metadata formatting is correct
	 * @param	InKey			the metadata key being added
	 * @param	InValue			the value string that will be associated with the InKey
	 */
	void ValidateMetaDataFormat(UField* Field, const FString& InKey, const FString& InValue);

	/**
	 * Ensures at script compile time that the metadata formatting is correct
	 */
	void ValidateMetaDataFormat(UField* Field, const TMap<FName, FString>& MetaData);

	// Validates the metadata, then adds it to the class data
	void AddMetaDataToClassData(UField* Field, const TMap<FName, FString>& InMetaData);

	/**
	 * Parses optional metadata text.
	 *
	 * @param	MetaData	the metadata map to store parsed metadata in
	 * @param	FieldName	the field being parsed (used for logging)
	 *
	 * @return	true if metadata was specified
	 */
	void ParseFieldMetaData(TMap<FName, FString>& MetaData, const TCHAR* FieldName);

	/**
	 * Formats the current comment, if any, and adds it to the metadata as a tooltip.
	 *
	 * @param	MetaData	the metadata map to store the tooltip in
	 */
	void AddFormattedPrevCommentAsTooltipMetaData(TMap<FName, FString>& MetaData);

	/** 
	 * Tries to parse the token as an access protection specifier (public:, protected:, or private:)
	 *
	 * @return	EAccessSpecifier this is, or zero if it is none
	 */
	EAccessSpecifier ParseAccessProtectionSpecifier(FToken& Token);

	const TCHAR* NestTypeName( ENestType NestType );

	UClass* GetQualifiedClass( const TCHAR* Thing );

	// Nest management functions.
	void PushNest( ENestType NestType, FName ThisName, UStruct* InNode );
	void PopNest( ENestType NestType, const TCHAR* Descr );

	/**
	 * Binds all delegate properties declared in ValidationScope the delegate functions specified in the variable declaration, verifying that the function is a valid delegate
	 * within the current scope.  This must be done once the entire class has been parsed because instance delegate properties must be declared before the delegate declaration itself.
	 *
	 * @todo: this function will no longer be required once the post-parse fixup phase is added (TTPRO #13256)
	 *
	 * @param	ValidationScope		the scope to validate delegate properties for
	 * @param	OwnerClass			the class currently being compiled.
	 * @param	DelegateCache		cached map of delegates that have already been found; used for faster lookup.
	 */
	void FixupDelegateProperties( UStruct* ValidationScope, UClass* OwnerClass, TMap<FName, UFunction*>& DelegateCache );

	/**
	 * Verifies that all specified class's UProperties with CFG_RepNotify have valid callback targets with no parameters nor return values
	 *
	 * @param	TargetClass			class to verify rep notify properties for
	 */
	void VerifyRepNotifyCallbacks( UClass* TargetClass );

	// Retry functions.
	void InitScriptLocation( FScriptLocation& Retry );
	void ReturnToLocation( const FScriptLocation& Retry, bool Binary=1, bool Text=1 );

	/**
	 * Helper function to determine whether the class hierarchy rooted at Suspect is
	 * dependent on the hierarchy rooted at Source.
	 *
	 * @param	Suspect		Root of hierarchy for suspect class
	 * @param	Source		Root of hierarchy for source class
	 * @param	AllClasses	Array of parsed classes
	 * @return	true if the hierarchy rooted at Suspect is dependent on the one rooted at Source, false otherwise
	 */
	bool IsDependentOn( UClass* Suspect, UClass* Source, const FClassTree& AllClasses );

	/**
	 * If the property has already been seen during compilation, then return add. If not,
	 * then return replace so that INI files don't mess with header exporting
	 *
	 * @param PropertyName the string token for the property
	 *
	 * @return FNAME_Replace_Not_Safe_For_Threading or FNAME_Add
	 */
	EFindName GetFindFlagForPropertyName(const TCHAR* PropertyName);

	// Check to see if anything in the class hierarchy passed in has CLASS_DefaultToInstanced
	static bool DoesAnythingInHierarchyHaveDefaultToInstanced(UClass* TestClass)
	{
		bool bDefaultToInstanced = false;

		UClass* Search = TestClass;
		while (!bDefaultToInstanced && (Search != NULL))
		{
			bDefaultToInstanced = Search->HasAnyClassFlags(CLASS_DefaultToInstanced);
			Search = Search->GetSuperClass();
		}

		return bDefaultToInstanced;
	}

	static void ValidatePropertyIsDeprecatedIfNecessary(FPropertyBase& VarProperty, FToken* OuterPropertyType);
};

/////////////////////////////////////////////////////
// FHeaderPreParser

class FHeaderPreParser : public FBaseParser
{
public:
	FHeaderPreParser()
	{
	}

	void ParseClassDeclaration(const TCHAR* InputText, int32 InLineNumber, const TCHAR* StartingMatchID, FString& out_ClassName, FString& out_BaseClassName, TArray<FName>& inout_ClassNames);
};
