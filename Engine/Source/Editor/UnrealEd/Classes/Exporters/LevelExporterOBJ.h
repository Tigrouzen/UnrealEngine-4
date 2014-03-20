// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// LevelExporterOBJ
//=============================================================================

#pragma once
#include "LevelExporterOBJ.generated.h"

UCLASS()
class ULevelExporterOBJ : public UExporter
{
	GENERATED_UCLASS_BODY()


	// Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) OVERRIDE;
	// End UExporter Interface
};



