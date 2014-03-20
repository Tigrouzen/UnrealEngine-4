// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAA.h: Post processing input implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount> 
class FRCPassPostProcessInput : public TRenderingCompositePassBase<0, 1>
{
public:
	// constructor
	FRCPassPostProcessInput(TRefCountPtr<IPooledRenderTarget>& InData);

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;

protected:

	TRefCountPtr<IPooledRenderTarget> Data;
};

//
class FRCPassPostProcessInputSingleUse : public FRCPassPostProcessInput
{
public:
	// constructor
	FRCPassPostProcessInputSingleUse(TRefCountPtr<IPooledRenderTarget>& InData)
		: FRCPassPostProcessInput(InData)
	{
	}

	// interface FRenderingCompositePass ---------

	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
};
