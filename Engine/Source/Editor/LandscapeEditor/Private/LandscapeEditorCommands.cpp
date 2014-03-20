// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "LandscapeEditorPrivatePCH.h"
#include "LandscapeEditorCommands.h"

void FLandscapeEditorCommands::RegisterCommands()
{
	UI_COMMAND( ManageMode, "Mode - Manage", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolMode_Manage", ManageMode);
	UI_COMMAND( SculptMode, "Mode - Sculpt", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolMode_Sculpt", SculptMode);
	UI_COMMAND( PaintMode,  "Mode - Paint",  "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolMode_Paint", PaintMode);

	UI_COMMAND( NewLandscape, "Tool - New Landscape", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_NewLandscape", NewLandscape);

	UI_COMMAND( ResizeLandscape, "Tool - Change Component Size", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_ResizeLandscape", ResizeLandscape);

	UI_COMMAND( SculptTool, "Tool - Sculpt", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Sculpt", SculptTool);
	UI_COMMAND( PaintTool, "Tool - Paint", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Paint", PaintTool);
	UI_COMMAND( SmoothTool, "Tool - Smooth", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Smooth", SmoothTool);
	UI_COMMAND( FlattenTool, "Tool - Flatten", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Flatten", FlattenTool);
	UI_COMMAND( RampTool, "Tool - Ramp", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Ramp", RampTool);
	UI_COMMAND( ErosionTool, "Tool - Erosion", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Erosion", ErosionTool);
	UI_COMMAND( HydroErosionTool, "Tool - Hydraulic Erosion", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_HydraErosion", HydroErosionTool);
	UI_COMMAND( NoiseTool, "Tool - Noise", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Noise", NoiseTool);
	UI_COMMAND( RetopologizeTool, "Tool - Retopologize", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Retopologize", RetopologizeTool);
	UI_COMMAND( VisibilityTool, "Tool - Visibility", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Visibility", VisibilityTool);

	UI_COMMAND( SelectComponentTool, "Tool - Component Selection", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Select", SelectComponentTool);
	UI_COMMAND( AddComponentTool, "Tool - Add Components", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_AddComponent", AddComponentTool);
	UI_COMMAND( DeleteComponentTool, "Tool - Delete Components", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_DeleteComponent", DeleteComponentTool);
	UI_COMMAND( MoveToLevelTool, "Tool - Move to Level", "Moves the selected landscape components to the current streaming level", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_MoveToLevel", MoveToLevelTool);

	UI_COMMAND( RegionSelectTool, "Tool - Region Selection", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Mask", RegionSelectTool);
	UI_COMMAND( RegionCopyPasteTool, "Tool - Copy/Paste", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_CopyPaste", RegionCopyPasteTool);

	UI_COMMAND( SplineTool, "Tool - Edit Splines", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("ToolSet_Splines", SplineTool);

	UI_COMMAND( CircleBrush, "Brush - Circle", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("BrushSet_Circle", CircleBrush);
	UI_COMMAND( AlphaBrush, "Brush - Alpha", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("BrushSet_Alpha", AlphaBrush);
	UI_COMMAND( AlphaBrush_Pattern, "Brush - Pattern", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("BrushSet_Pattern", AlphaBrush_Pattern);
	UI_COMMAND( ComponentBrush, "Brush - Component", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("BrushSet_Component", ComponentBrush);
	UI_COMMAND( GizmoBrush, "Brush - Gizmo", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("BrushSet_Gizmo", GizmoBrush);
	NameToCommandMap.Add("BrushSet_Splines", SplineTool);

	UI_COMMAND( CircleBrush_Smooth, "Circle Brush - Smooth Falloff", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("Circle_Smooth", CircleBrush_Smooth);
	UI_COMMAND( CircleBrush_Linear, "Circle Brush - Linear Falloff", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("Circle_Linear", CircleBrush_Linear);
	UI_COMMAND( CircleBrush_Spherical, "Circle Brush - Spherical Falloff", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("Circle_Spherical", CircleBrush_Spherical);
	UI_COMMAND( CircleBrush_Tip, "Circle Brush - Tip Falloff", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	NameToCommandMap.Add("Circle_Tip", CircleBrush_Tip);

	UI_COMMAND( ViewModeNormal, "Normal", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ViewModeLOD, "LOD", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ViewModeLayerDensity, "Layer Density", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ViewModeLayerDebug, "Layer Debug", "", EUserInterfaceActionType::RadioButton, FInputGesture() );
}
