// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxFactory.generated.h"

UCLASS(hidecategories=Object)
class UFbxFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class UFbxImportUI* ImportUI;


	/**  Set import batch **/
	void EnableShowOption() { bShowOption = true; }


	// Begin UObject Interface
	virtual void CleanUp() OVERRIDE;
	virtual bool ConfigureProperties() OVERRIDE;
	virtual void PostInitProperties() OVERRIDE;
	// End UObject Interface

	// Begin UFactory Interface
	virtual bool DoesSupportClass(UClass * Class) OVERRIDE;
	virtual UClass* ResolveSupportedClass() OVERRIDE;
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled) OVERRIDE;
	// End UFactory Interface
	
	/**
	 * Detect mesh type to import: Static Mesh or Skeletal Mesh.
	 * Only the first mesh will be detected.
	 *
	 * @param InFilename	FBX file name
	 * @return bool	return true if parse the file successfully
	 */
	bool DetectImportType(const FString& InFilename);

protected:
	// @todo document
	UObject* RecursiveImportNode(void* FFbxImporter, void* Node, UObject* InParent, FName InName, EObjectFlags Flags, int32& Index, int32 Total, TArray<UObject*>& OutNewAssets);

	// @todo document
	UObject* ImportANode(void* VoidFbxImporter, void* VoidNode, UObject* InParent, FName InName, EObjectFlags Flags, int32& NodeIndex, int32 Total = 0, UObject* InMesh = NULL, int LODIndex = 0);

	bool bShowOption;
	bool bDetectImportTypeOnImport;

	/** true if the import operation was canceled. */
	bool bOperationCanceled;
};



