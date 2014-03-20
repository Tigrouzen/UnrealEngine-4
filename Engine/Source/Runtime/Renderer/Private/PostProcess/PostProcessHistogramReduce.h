// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessHistogramReduce.h: Post processing histogram reduce implementation.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"
#include "PostProcessHistogram.h"

// derives from TRenderingCompositePassBase<InputCount, OutputCount>
class FRCPassPostProcessHistogramReduce : public TRenderingCompositePassBase<1, 1>
{
public:
	// interface FRenderingCompositePass ---------
	virtual void Process(FRenderingCompositePassContext& Context);
	virtual void Release() OVERRIDE { delete this; }
	virtual FPooledRenderTargetDesc ComputeOutputDesc(EPassOutputId InPassOutputId) const;
	
	static const uint32 ThreadGroupSizeX = FRCPassPostProcessHistogram::HistogramTexelCount;
	static const uint32 ThreadGroupSizeY = 4;

private:

	static uint32 ComputeLoopSize(FIntPoint PixelExtent);
};
