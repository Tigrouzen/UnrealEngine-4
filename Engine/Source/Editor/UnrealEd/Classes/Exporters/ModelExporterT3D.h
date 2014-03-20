// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// ModelExporterT3D
//=============================================================================

#pragma once
#include "ModelExporterT3D.generated.h"

UCLASS()
class UModelExporterT3D : public UExporter
{
	GENERATED_UCLASS_BODY()


	// Begin UExporter Interface
	virtual bool ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, uint32 PortFlags=0 ) OVERRIDE;
	// End UExporter Interface
};



