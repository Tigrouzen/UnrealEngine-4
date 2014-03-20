// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessOutput : public TRenderingCompositePassBase<1, 1>
{
public:
	// constructor
	FRCPassPostProcessOutput(TRefCountPtr<IPooledRenderTarget>* InExternalRenderTarget);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;

protected:

	TRefCountPtr<IPooledRenderTarget>* ExternalRenderTarget;
};