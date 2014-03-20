// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NameTypes.h: Unreal global name types.
=============================================================================*/

#pragma once

/*----------------------------------------------------------------------------
	Definitions.
----------------------------------------------------------------------------*/

/** Maximum size of name. */
enum {NAME_SIZE	= 1024};

/** Name index. */
typedef int32 NAME_INDEX;

#define checkName checkSlow

/** Externally, the instance number to represent no instance number is NAME_NO_NUMBER, 
    but internally, we add 1 to indices, so we use this #define internally for 
	zero'd memory initialization will still make NAME_None as expected */
#define NAME_NO_NUMBER_INTERNAL	0

/** Conversion routines between external representations and internal */
#define NAME_INTERNAL_TO_EXTERNAL(x) (x - 1)
#define NAME_EXTERNAL_TO_INTERNAL(x) (x + 1)

/** Special value for an FName with no number */
#define NAME_NO_NUMBER NAME_INTERNAL_TO_EXTERNAL(NAME_NO_NUMBER_INTERNAL)


/** this is the character used to separate a subobject root from its subobjects in a path name. */
#define SUBOBJECT_DELIMITER				TEXT(":")

/** this is the character used to separate a subobject root from its subobjects in a path name, as a char */
#define SUBOBJECT_DELIMITER_CHAR		':'

/** These are the characters that cannot be used in general FNames */
#define INVALID_NAME_CHARACTERS			TEXT("\"' ,\n\r\t")

/** These characters cannot be used in object names */
#define INVALID_OBJECTNAME_CHARACTERS	TEXT("\"' ,/.:|&!\n\r\t@#(){}[]=;^%$`")

/** These characters cannot be used in textboxes which take group names (i.e. Group1.Group2) */
#define INVALID_GROUPNAME_CHARACTERS	TEXT("\"' ,/:|&!\n\r\t@#")

/** These characters cannot be used in long package names */
#define INVALID_LONGPACKAGE_CHARACTERS	TEXT("\\:*?\"<>|' ,.&!\n\r\t@#")




namespace FNameDefs
{
#if !WITH_EDITORONLY_DATA
	// Use a modest bucket count on consoles
	static const uint32 NameHashBucketCount = 4096;
#else
	// On PC platform we use a large number of name hash buckets to accommodate the editor's
	// use of FNames to store asset path and content tags
	static const uint32 NameHashBucketCount = 65536;
#endif
}


enum ELinkerNameTableConstructor    {ENAME_LinkerConstructor};

/** Enumeration for finding name. */
enum EFindName
{
	/** Find a name; return 0 if it doesn't exist. */
	FNAME_Find,

	/** Find a name or add it if it doesn't exist. */
	FNAME_Add,

	/** Finds a name and replaces it. Adds it if missing. This is only used by UHT and is generally not safe for threading. 
	 * All this really is used for is correcting the case of names. In MT conditions you might get a half-changed name.
	 */
	FNAME_Replace_Not_Safe_For_Threading,
};

/*----------------------------------------------------------------------------
	FNameEntry.
----------------------------------------------------------------------------*/

/** 
 * Mask for index bit used to determine whether string is encoded as TCHAR or ANSICHAR. We don't
 * add an extra bool in order to keep the name size to a minimum and 2 billion names is impractical
 * so there are a few bits left in the index.
 */
#define NAME_WIDE_MASK 0x1
#define NAME_INDEX_SHIFT 1

/**
 * A global name, as stored in the global name table.
 */
struct FNameEntry
{
private:
	/** Index of name in hash. */
	NAME_INDEX		Index;

public:
	/** Pointer to the next entry in this hash bin's linked list. */
	FNameEntry*		HashNext;

private:
	/** Name, variable-sized - note that AllocateNameEntry only allocates memory as needed. */
	union
	{
		ANSICHAR	AnsiName[NAME_SIZE];
		WIDECHAR	WideName[NAME_SIZE];
	};

	// DO NOT ADD VARIABLES BELOW UNION!

public:
	/** Default constructor doesn't do anything. AllocateNameEntry is responsible for work. */
	FNameEntry()
	{}

	/** 
	 * Constructor called from the linker name table serialization function. Initializes the index
	 * to a value that indicates widechar as that's what the linker is going to serialize.
	 */
	FNameEntry( enum ELinkerNameTableConstructor )
	{
		Index = NAME_WIDE_MASK;
	}

	/** 
	 * Sets whether or not the NameEntry will have a wide string, or an ansi string
	 *
	 * @param bIsWide true if we are going to serialize a wide string
	 */
	FORCEINLINE void PreSetIsWideForSerialization(bool bIsWide)
	{
		Index = bIsWide ? NAME_WIDE_MASK : 0;
	}

	/**
	 * Returns index of name in hash passed to FNameEntry via AllocateNameEntry. The lower bits
	 * are used for internal state, which is why we need to shift.
	 *
	 * @return Index of name in hash
	 */
	FORCEINLINE int32 GetIndex() const
	{
		return Index >> NAME_INDEX_SHIFT;
	}

	/**
	 * Returns whether this name entry is represented via TCHAR or ANSICHAR
	 */
	FORCEINLINE bool IsWide() const
	{
		return (Index & NAME_WIDE_MASK);
	}

	/**
	 * @return FString of name portion minus number.
	 */
	CORE_API FString GetPlainNameString() const;	

	/**
	 * Appends this name entry to the passed in string.
	 *
	 * @param	String	String to append this name to
	 */
	void AppendNameToString( FString& String ) const;

	/**
	 * @return case insensitive hash of name
	 */
	uint32 GetNameHash() const;

	/**
	 * @return length of name
	 */
	int32 GetNameLength() const;

	/**
	 * Compares name without looking at case.
	 *
	 * @param	InName	Name to compare to
	 * @return	true if equal, false otherwise
	 */
	bool IsEqual( const ANSICHAR* InName ) const;

	/**
	 * Compares name without looking at case.
	 *
	 * @param	InName	Name to compare to
	 * @return	true if equal, false otherwise
	 */
	bool IsEqual( const WIDECHAR* InName ) const;

	/**
	 * @return direct access to ANSI name if stored in ANSI
	 */
	inline ANSICHAR const* GetAnsiName() const
	{
		check(!IsWide());
		return AnsiName;
	}

	/**
	 * @return direct access to wide name if stored in widechars
	 */
	inline WIDECHAR const* GetWideName() const
	{
		check(IsWide());
		return WideName;
	}

	static CORE_API int32 GetSize( const TCHAR* Name );

	/**
	 * Returns the size in bytes for FNameEntry structure. This is != sizeof(FNameEntry) as we only allocated as needed.
	 *
	 * @param	Length			Length of name
	 * @param	bIsPureAnsi		Whether name is pure ANSI or not
	 * @return	required size of FNameEntry structure to hold this string (might be wide or ansi)
	 */
	static int32 GetSize( int32 Length, bool bIsPureAnsi );

	// Functions.
	friend CORE_API FArchive& operator<<( FArchive& Ar, FNameEntry& E );
	friend CORE_API FArchive& operator<<( FArchive& Ar, FNameEntry* E )
	{
		return Ar << *E;
	}

	// Friend for access to Flags.
	friend FNameEntry* AllocateNameEntry( const void* Name, NAME_INDEX Index, FNameEntry* HashNext, bool bIsPureAnsi );
};

/**
 * Simple array type that can be expanded without invalidating existing entries.
 * This is critical to thread safe FNames.
 * @param ElementType Type of the pointer we are storing in the array
 * @param MaxTotalElements absolute maximum number of elements this array can ever hold
 * @param ElementsPerChunk how many elements to allocate in a chunk
 **/
 template<typename ElementType, int32 MaxTotalElements, int32 ElementsPerChunk>
class TStaticIndirectArrayThreadSafeRead
{
	enum
	{
		// figure out how many elements we need in the master table
		ChunkTableSize = (MaxTotalElements + ElementsPerChunk - 1) / ElementsPerChunk
	};
	/** Static master table to chunks of pointers **/
	ElementType** Chunks[ChunkTableSize];
	/** Number of elements we currently have **/
	int32 NumElements;
	/** Number of chunks we currently have **/
	int32 NumChunks;

	/**
	 * Expands the array so that Element[Index] is allocated. New pointers are all zero.
	 * @param Index The Index of an element we want to be sure is allocated
	 **/
	void ExpandChunksToIndex(int32 Index)
	{
		check(Index >= 0 && Index < MaxTotalElements);
		int32 ChunkIndex = Index / ElementsPerChunk;
		while (1)
		{
			if (ChunkIndex < NumChunks)
			{
				break;
			}
			// add a chunk, and make sure nobody else tries
			ElementType*** Chunk = &Chunks[ChunkIndex];
			ElementType** NewChunk = (ElementType**)FMemory::Malloc(sizeof(ElementType*) * ElementsPerChunk);
			FMemory::Memzero(NewChunk, sizeof(ElementType*) * ElementsPerChunk);
			if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)Chunk, NewChunk, NULL))
			{
				// someone else beat us to the add, we don't support multiple concurrent adds
				check(0)
			}
			else
			{
				NumChunks++;
			}
		}
		check(ChunkIndex < NumChunks && Chunks[ChunkIndex]); // should have a valid pointer now
	}
	/**
	 * Return a pointer to the pointer to a given element
	 * @param Index The Index of an element we want to retrieve the pointer-to-pointer for
	 **/
	ElementType const* const* GetItemPtr(int32 Index) const
	{
		int32 ChunkIndex = Index / ElementsPerChunk;
		int32 WithinChunkIndex = Index % ElementsPerChunk;
		check(IsValidIndex(Index) && ChunkIndex < NumChunks && Index < MaxTotalElements);
		ElementType** Chunk = Chunks[ChunkIndex];
		check(Chunk);
		return Chunk + WithinChunkIndex;
	}

public:
	/** Constructor : Probably not thread safe **/
	TStaticIndirectArrayThreadSafeRead()
		: NumElements(0)
		, NumChunks(0)
	{
		FMemory::MemZero(Chunks);
	}
	/** 
	 * Return the number of elements in the array 
	 * Thread safe, but you know, someone might have added more elements before this even returns
	 * @return	the number of elements in the array
	**/
	int32 Num() const
	{
		return NumElements;
	}
	/** 
	 * Return if this index is valid
	 * Thread safe, if it is valid now, it is valid forever. Other threads might be adding during this call.
	 * @param	Index	Index to test
	 * @return	true, if this is a valid
	**/
	bool IsValidIndex(int32 Index) const
	{
		return Index < Num() && Index >= 0;
	}
	/** 
	 * Return a reference to an element
	 * @param	Index	Index to return
	 * @return	a reference to the pointer to the element
	 * Thread safe, if it is valid now, it is valid forever. This might return NULL, but by then, some other thread might have made it non-NULL.
	**/
	ElementType const* const& operator[](int32 Index) const
	{
		ElementType const* const* ItemPtr = GetItemPtr(Index);
		check(ItemPtr);
		return *ItemPtr;
	}
	/** 
	 * Add more elements to the array
	 * @param	NumToAdd	Number of elements to add
	 * @return	the number of elements in the container before we did the add. In other words, the add index.
	 * Not thread safe. This should only be called by one thread, but the other methods can be called while this is going on.
	**/
	int32 AddZeroed(int32 NumToAdd)
	{
		int32 Result = NumElements;
		check(NumElements + NumToAdd <= MaxTotalElements);
		ExpandChunksToIndex(NumElements + NumToAdd - 1);
		check(Result == NumElements);
		NumElements += NumToAdd;
		FPlatformMisc::MemoryBarrier();
		return Result;
	}
	/** 
	 * Return a naked pointer to the fundamental data structure for debug visualizers.
	**/
	ElementType*** GetRootBlockForDebuggerVisualizers()
	{
		return Chunks;
	}
};

// Typedef for the threadsafe master name table. 
// CAUTION: If you change those constants, you probably need to update the debug visualizers.
typedef TStaticIndirectArrayThreadSafeRead<FNameEntry, 2 * 1024 * 1024 /* 2M unique FNames */, 16384 /* allocated in 64K/128K chunks */ > TNameEntryArray;

/**
 * Public name, available to the world.  Names are stored as a combination of
 * an index into a table of unique strings and an instance number.
 * Names are case-insensitive.
 */
class CORE_API FName 
{
public:

	FORCEINLINE NAME_INDEX GetIndex() const
	{
		checkName(Index >= 0 && Index < GetNames().Num());
		checkName(GetNames()[Index]);
		return Index;
	}

	FORCEINLINE int32 GetNumber() const
	{
		return Number;
	}

	FORCEINLINE void SetNumber(int32 NewNumber)
	{
		Number = NewNumber;
	}
	
	/** Returns the pure name string without any trailing numbers */
	FString GetPlainNameString() const
	{
		return GetNames()[Index]->GetPlainNameString();
	}

	/**
	 * Returns the underlying ANSI string pointer.  No allocations.  Will fail if this is actually a wide name.
	 */
	FORCEINLINE ANSICHAR const* GetPlainANSIString() const
	{
		return GetNames()[Index]->GetAnsiName();		
	}

	/**
	 * Returns the underlying WIDE string pointer.  No allocations.  Will fail if this is actually an ANSI name.
	 */
	FORCEINLINE WIDECHAR const* GetPlainWIDEString() const
	{
		return GetNames()[Index]->GetWideName();
	}

	/**
	 * Converts an FName to a readable format
	 *
	 * @return String representation of the name
	 */
	FString ToString() const;

	/**
	 * Converts an FName to a readable format, in place
	 * 
	 * @param Out String to fill ot with the string representation of the name
	 */
	void ToString(FString& Out) const;

	/**
	 * Converts an FName to a readable format, in place, appending to an existing string (ala GetFullName)
	 * 
	 * @param Out String to append with the string representation of the name
	 */
	void AppendString(FString& Out) const;

	FORCEINLINE bool operator==(FName Other) const
	{
		return Index == Other.Index && Number == Other.Number;
	}
	FORCEINLINE bool operator!=(FName Other) const
	{
		return Index != Other.Index || Number != Other.Number;
	}

	/**
	 * Comparison operator used for sorting alphabetically.
	 */
	FORCEINLINE bool operator<( const FName& Other ) const
	{
		return ToString() < Other.ToString();
	}

	FORCEINLINE bool IsNone() const
	{
		return Index == 0 && Number == 0;
	}

	FORCEINLINE bool IsValid() const
	{
		TNameEntryArray& Names = GetNames();
		return Index>=0 && Index<Names.Num() && Names[Index]!=NULL;
	}

	/**
	 * Helper function to check if the index is valid. Does not check if the name itself is valid.
	 */
	FORCEINLINE bool IsValidIndexFast() const
	{
		return Index >= 0 && Index < GetNames().Num();
	}

	/**
	 * Checks to see that a FName follows the rules that Unreal requires.
	 *
	 * @param	InInvalidChars	The set of invalid characters that the name cannot contain
	 * @param	InReason		If the check fails, this string is filled in with the reason why.
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidXName( FString InvalidChars=INVALID_NAME_CHARACTERS, class FText* Reason=NULL ) const;

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires.
	 *
	 * @param	InReason		If the check fails, this string is filled in with the reason why.
	 * @param	InInvalidChars	The set of invalid characters that the name cannot contain
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidXName( class FText& InReason, FString InvalidChars=INVALID_NAME_CHARACTERS ) const
	{
		return IsValidXName(InvalidChars,&InReason);
	}

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires for object names.
	 *
	 * @param	InReason		If the check fails, this string is filled in with the reason why.
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidObjectName( class FText& InReason ) const
	{
		return IsValidXName( InReason, INVALID_OBJECTNAME_CHARACTERS );
	}

	/**
	 * Takes an FName and checks to see that it follows the rules that Unreal requires for package or group names.
	 *
	 * @param	InReason		If the check fails, this string is filled in with the reason why.
	 * @param	bIsGroupName	if true, check legallity for a group name, else check legality for a package name
	 *
	 * @return	true if the name is valid
	 */
	bool IsValidGroupName( class FText& InReason, bool bIsGroupName=false ) const
	{
		return IsValidXName( InReason, INVALID_LONGPACKAGE_CHARACTERS );
	}

#ifdef IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY
	// Assignment operator
	FORCEINLINE FName& operator=(const FName& Other)
	{
		this->Index = Other.Index;
		this->Number = Other.Number;

		return *this;
	}
#endif

	/**
	 * Compares name to passed in one. Sort is alphabetical ascending.
	 *
	 * @param	Other	Name to compare this against
	 * @return	< 0 is this < Other, 0 if this == Other, > 0 if this > Other
	 */
	int32 Compare( const FName& Other ) const;

	/**
	 * Create an FName with a hardcoded string index.
	 *
	 * @param N The harcdcoded value the string portion of the name will have. The number portion will be NAME_NO_NUMBER
	 */
	FORCEINLINE FName( enum EName N )
	: Index( N )
	, Number( NAME_NO_NUMBER_INTERNAL )
	{}

	/**
	 * Create an FName with a hardcoded string index and (instance).
	 *
	 * @param N The harcdcoded value the string portion of the name will have
	 * @param InNumber The hardcoded value for the number portion of the name
	 */
	FORCEINLINE FName( enum EName N, int32 InNumber )
	: Index( N )
	, Number( InNumber )
	{
	}

	/**
	 * Default constructor, initialized to None
	 */
	FORCEINLINE FName()
		: Index( 0 )
		, Number( 0 )
	{
	}

	/**
	 * Scary no init constructor, used for something obscure in UObjectBase
	 */
	explicit FName(ENoInit)
	{
	}

	/**
	 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
	 * doesn't already exist, then the name will be NAME_None
	 *
	 * @param Name			Value for the string portion of the name
	 * @param FindType		Action to take (see EFindName)
	 * @param unused
	 */
	FName( const WIDECHAR*  Name, EFindName FindType=FNAME_Add, bool bUnused=true );
	FName( const ANSICHAR* Name, EFindName FindType=FNAME_Add, bool bUnused=true );

	/**
	 * Create an FName. If FindType is FNAME_Find, and the string part of the name 
	 * doesn't already exist, then the name will be NAME_None
	 *
	 * @param Name Value for the string portion of the name
	 * @param Number Value for the number portion of the name
	 * @param FindType Action to take (see EFindName)
	 */
	FName( const TCHAR* Name, int32 InNumber, EFindName FindType=FNAME_Add );

	/**
	 * Constructor used by ULinkerLoad when loading its name table; Creates an FName with an instance
	 * number of 0 that does not attempt to split the FName into string and number portions.
	 */
	FName( ELinkerNameTableConstructor, const WIDECHAR* Name );

	/**
	 * Constructor used by ULinkerLoad when loading its name table; Creates an FName with an instance
	 * number of 0 that does not attempt to split the FName into string and number portions.
	 */
	FName( ELinkerNameTableConstructor, const ANSICHAR* Name );

	/**
	 * Create an FName with a hardcoded string index.
	 *
	 * @param HardcodedIndex	The harcdcoded value the string portion of the name will have. 
	 * @param Name				The harcdcoded name to intialize
	 */
	explicit FName( enum EName HardcodedIndex, const TCHAR* Name );

	/**
	 * Comparision operator.
	 *
	 * @param	Other	String to compare this name to
	 * @return true if name matches the string, false otherwise
	 */
	bool operator==( const TCHAR * Other ) const;

	static void StaticInit();
	static void DisplayHash( class FOutputDevice& Ar );
	static FString SafeString( EName Index, int32 InstanceNumber=NAME_NO_NUMBER_INTERNAL )
	{
		TNameEntryArray& Names = GetNames();
		return GetIsInitialized()
			? (Names.IsValidIndex(Index) && Names[Index])
				? FName(Index, InstanceNumber).ToString()
				: FString(TEXT("*INVALID*"))
			: FString(TEXT("*UNINITIALIZED*"));
	}
	static int32 GetMaxNames()
	{
		return GetNames().Num();
	}
	/**
	 * @return Size of all name entries.
	 */
	static int32 GetNameEntryMemorySize()
	{
		return NameEntryMemorySize;
	}
	/**
	* @return Size of Name Table object as a whole
	*/
	static int32 GetNameTableMemorySize()
	{
		return GetNameEntryMemorySize() + GetMaxNames() * sizeof(FNameEntry*) + sizeof(NameHash);
	}

	/**
	 * @return number of ansi names in name table
	 */
	static int32 GetNumAnsiNames()
	{
		return NumAnsiNames;
	}
	/**
	 * @return number of wide names in name table
	 */
	static int32 GetNumWideNames()
	{
		return NumWideNames;
	}
	static FNameEntry const* GetEntry( int i )
	{
		return GetNames()[i];
	}
	//@}

	/**
	 * Helper function to split an old-style name (Class_Number, ie Rocket_17) into
	 * the component parts usable by new-style FNames. Only use results if this function
	 * returns true.
	 *
	 * @param OldName		Old-style name
	 * @param NewName		Ouput string portion of the name/number pair
	 * @param NewNameLen	Size of NewName buffer (in TCHAR units)
	 * @param NewNumber		Number portion of the name/number pair
	 *
	 * @return true if the name was split, only then will NewName/NewNumber have valid values
	 */
	static bool SplitNameWithCheck(const WIDECHAR* OldName, WIDECHAR* NewName, int32 NewNameLen, int32& NewNumber);

	/** Singleton to retrieve a table of all names (single threaded) for debug visualizers. */
	static TArray<FNameEntry const*>* GetNameTableForDebuggerVisualizers_ST();
	/** Singleton to retrieve a table of all names (multithreaded) for debug visualizers. */
	static FNameEntry*** GetNameTableForDebuggerVisualizers_MT();
	/** Run autotest on FNames. */
	static void AutoTest();
private:

	/** Index into the Names array (used to find String portion of the string/number pair) */
	NAME_INDEX	Index;
	/** Number portion of the string/number pair (stored internally as 1 more than actual, so zero'd memory will be the default, no-instance case) */
	int32		Number;

	/** Name hash.												*/
	static FNameEntry*						NameHash[ FNameDefs::NameHashBucketCount ];
	/** Size of all name entries.								*/
	static int32							NameEntryMemorySize;	
	/** Number of ANSI names in name table.						*/
	static int32							NumAnsiNames;			
	/** Number of wide names in name table.					*/
	static int32							NumWideNames;

	/** Singleton to retrieve a table of all names. */
	static TNameEntryArray& GetNames();
	/**
	 * Return the static initialized flag. Must be in a function like this so we don't have problems with 
	 * different initialization order of static variables across the codebase. Use this function to get or set the variable.
	 */
	static bool& GetIsInitialized();

	friend const TCHAR* DebugFName(int32);
	friend const TCHAR* DebugFName(int32, int32);
	friend const TCHAR* DebugFName(FName&);
	friend FNameEntry* AllocateNameEntry( const void* Name, NAME_INDEX Index, FNameEntry* HashNext, bool bIsPureAnsi );

	/**
	 * Shared initialization code (between two constructors)
	 * 
	 * @param InName String name of the name/number pair
	 * @param InNumber Number part of the name/number pair
	 * @param FindType Operation to perform on names
	 * @param bSplitName If true, this function will attempt to split a number off of the string portion (turning Rocket_17 to Rocket and number 17)
	 * @param HardcodeIndex If >= 0, this represents a hardcoded FName and so automatically gets this index
	 */
	void Init(const WIDECHAR* InName, int32 InNumber, EFindName FindType, bool bSplitName=true, int32 HardcodeIndex = -1);

	/**
	 * Non-optimized initialization code for ansi names.
	 * 
	 * @param InName		String name of the name/number pair
	 * @param InNumber		Number part of the name/number pair
	 * @param FindType		Operation to perform on names
	 * @param bSplitName	If true, this function will attempt to split a number off of the string portion (turning Rocket_17 to Rocket and number 17)
	 */
	void Init(const ANSICHAR* InName, int32 InNumber, EFindName FindType, bool bSplitName=true, int32 HardcodeIndex = -1)
	{
		Init(StringCast<WIDECHAR>(InName).Get(), InNumber, FindType, bSplitName, HardcodeIndex);
	}

	/** Singleton to retrieve the critical section. */
	static FCriticalSection* GetCriticalSection();

};

template<> struct TIsZeroConstructType<class FName> { enum { Value = true }; };
Expose_TNameOf(FName)


inline uint32 GetTypeHash( const FName N )
{
	return N.GetIndex();
}

/**
 * Comparison operator with TCHAR* on left hand side and FName on right hand side
 * 
 * @param	LHS		TCHAR to compare to FName
 * @param	RHS		FName to compare to TCHAR
 * @return true if strings match, false otherwise
 */
inline bool operator==( const TCHAR *LHS, const FName &RHS )
{
	return RHS == LHS;
}

/** FNames act like PODs. */
template <> struct TIsPODType<FName> { enum { Value = true }; };


