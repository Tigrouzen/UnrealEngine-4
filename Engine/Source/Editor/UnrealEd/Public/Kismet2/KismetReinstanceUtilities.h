// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#ifndef __KismetReinstanceUtilities_h__
#define __KismetReinstanceUtilities_h__

#pragma once


#include "Engine.h"

class UNREALED_API FBlueprintCompileReinstancer
{
private:
	/** Reference to the class we're actively reinstancing */
	UClass* ClassToReinstance;

	/** Reference to the duplicate of ClassToReinstance, which all previous instances are now instances of */
	UClass* DuplicatedClass;

	/** The original CDO object for the class being actively reinstanced */
	UObject*	OriginalCDO;

	/** Children of this blueprint, which will need to be recompiled and relinked temporarily to maintain the class layout */
	TArray<UBlueprint*> Children;

	/** Bytecode dependent blueprints, which will need to be updated after the compilation */
	TArray<UBlueprint*> Dependencies;

	/** Mappings from old fields before recompilation to their new equivalents */
	TMap<FName, UProperty*> PropertyMap;
	TMap<FName, UFunction*> FunctionMap;

	/** Whether or not this resinstancer has already reinstanced  */
	bool bHasReinstanced;

	/** Don't call GC */
	bool bSkipGarbageCollection;

public:
	~FBlueprintCompileReinstancer();

	/** Sets the reinstancer up to work on every object of the specified class */
	FBlueprintCompileReinstancer(UClass* InClassToReinstance, bool bIsBytecodeOnly = false, bool bSkipGC = false);

	/** Saves a mapping of field names to their UField equivalents, so we can remap any bytecode that references them later */
	void SaveClassFieldMapping();

	/** Helper to gather mappings from the old class's fields to the new class's version */
	void GenerateFieldMappings(TMap<UObject*, UObject*>& FieldMapping);

	/** Reinstances all objects in the ObjectReinstancingMap */
	void ReinstanceObjects();

	/** Updates references to properties and functions of the class that has in the bytecode of dependent blueprints */
	void UpdateBytecodeReferences();

	/** Worker function to replace all instances of OldClass with a new instance of NewClass */
	static void ReplaceInstancesOfClass(UClass* OldClass, UClass* NewClass, UObject* OriginalCDO = NULL);
	
	/** Verify that all instances of the duplicated class have been replaced and collected */
	void VerifyReplacement();

private:
	/** Hide the default constructor for the reinstancer */
	FBlueprintCompileReinstancer();

	/** Reparents the specified blueprint or class to be the duplicated class in order to allow properties to be copied from the previous CDO to the new one */
	void ReparentChild(UBlueprint* ChildBP);
	void ReparentChild(UClass* ChildClass);
};


#endif//__KismetEditorUtilities_h__
