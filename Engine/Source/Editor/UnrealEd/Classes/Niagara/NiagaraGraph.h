// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraGraph.generated.h"

UCLASS(MinimalAPI)
class UNiagaraGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	/** Get the source that owns this graph */
	class UNiagaraScriptSource* GetSource() const;
};

