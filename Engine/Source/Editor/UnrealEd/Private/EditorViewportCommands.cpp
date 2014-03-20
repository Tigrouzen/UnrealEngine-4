// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "EditorViewportCommands.h"



void FEditorViewportCommands::RegisterCommands()
{
	UI_COMMAND( Perspective, "Perspective", "Switches the viewport to perspective view", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::G ) );
	UI_COMMAND( Front, "Front", "Switches the viewport to front view", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::H ) );
	UI_COMMAND( Top, "Top", "Switches the viewport to top view", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::J ) );
	UI_COMMAND( Side, "Side", "Switches the viewport to side view", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::K ) );

	UI_COMMAND( WireframeMode, "Brush Wireframe View Mode", "Renders the scene in brush wireframe", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::Two ) );
	UI_COMMAND( UnlitMode, "Unlit View Mode", "Renders the scene with no lights", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::Three ) );
	UI_COMMAND( LitMode, "Lit View Mode", "Renders the scene with normal lighting", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::Four ) );
	UI_COMMAND( DetailLightingMode, "Detail Lighting View Mode", "Renders the scene with detailed lighting only", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::Five ) );
	UI_COMMAND( LightingOnlyMode, "Lighting Only View Mode", "Renders the scene with lights only, no textures", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::Six ) );
	UI_COMMAND( LightComplexityMode, "Light Complexity View Mode", "Renders the scene with light complexity visualization", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::Seven ) );
	UI_COMMAND( ShaderComplexityMode, "Shader Complexity View Mode", "Renders the scene with shader complexity visualization", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::Eight ) );
	UI_COMMAND( StationaryLightOverlapMode, "Stationary Light Overlap View Mode", "Visualizes overlap of stationary lights", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( LightmapDensityMode, "Lightmap Density View Mode", "Renders the scene with lightmap density visualization", EUserInterfaceActionType::RadioButton, FInputGesture( EModifierKey::Alt, EKeys::Zero ) );
	UI_COMMAND( VisualizeBufferMode, "Buffer Visualization View Mode", "Renders a set of selected post process materials, which visualize various intermediate render buffers (material attributes)", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( ReflectionOverrideMode, "Reflections View Mode", "Renders the scene with reflections only", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( CollisionPawn, "Player Collision", "Renders player collision visualization", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( CollisionVisibility, "Visibility Collision", "Renders visibility collision visualization", EUserInterfaceActionType::RadioButton, FInputGesture() );

	UI_COMMAND( ToggleRealTime, "Realtime", "Toggles real time rendering in this viewport", EUserInterfaceActionType::ToggleButton, FInputGesture( EModifierKey::Control, EKeys::R ) );
	UI_COMMAND( ToggleStats, "Show Stats", "Toggles the ability to show stats in this viewport (enables realtime)", EUserInterfaceActionType::ToggleButton, FInputGesture( EModifierKey::Shift, EKeys::L ) );
	UI_COMMAND( ToggleFPS, "Show FPS", "Toggles showing frames per section in this viewport (enables realtime)", EUserInterfaceActionType::ToggleButton, FInputGesture( EModifierKey::Control|EModifierKey::Shift, EKeys::H ) );

	UI_COMMAND( ScreenCapture, "Screen Capture", "Take a screenshot of the active viewport.", EUserInterfaceActionType::Button, FInputGesture(EKeys::F9) );
	UI_COMMAND( ScreenCaptureForProjectThumbnail, "Update Project Thumbnail", "Take a screenshot of the active viewport for use as the project thumbnail.", EUserInterfaceActionType::Button, FInputGesture() );

	UI_COMMAND( IncrementPositionGridSize, "Grid Size (Position): Increment", "Increases the position grid size setting by one", EUserInterfaceActionType::Button, FInputGesture( EKeys::RightBracket ) );
	UI_COMMAND( DecrementPositionGridSize, "Grid Size (Position): Decrement", "Decreases the position grid size setting by one", EUserInterfaceActionType::Button, FInputGesture( EKeys::LeftBracket ) );
	UI_COMMAND( IncrementRotationGridSize, "Grid Size (Rotation): Increment", "Increases the rotation grid size setting by one", EUserInterfaceActionType::Button, FInputGesture( EModifierKey::Shift, EKeys::RightBracket ) );
	UI_COMMAND( DecrementRotationGridSize, "Grid Size (Rotation): Decrement", "Decreases the rotation grid size setting by one", EUserInterfaceActionType::Button, FInputGesture( EModifierKey::Shift, EKeys::LeftBracket ) );

	
	UI_COMMAND( TranslateMode, "Translate Mode", "Select and translate objects", EUserInterfaceActionType::ToggleButton, FInputGesture(EKeys::W) );
	UI_COMMAND( RotateMode, "Rotate Mode", "Select and rotate objects", EUserInterfaceActionType::ToggleButton, FInputGesture(EKeys::E) );
	UI_COMMAND( ScaleMode, "Scale Mode", "Select and scale objects", EUserInterfaceActionType::ToggleButton, FInputGesture(EKeys::R) );
	UI_COMMAND( TranslateRotateMode, "Combined Translate and Rotate Mode", "Select and translate or rotate objects", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( RelativeCoordinateSystem_World, "World-relative Transform", "Move and rotate objects relative to the cardinal world axes", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( RelativeCoordinateSystem_Local, "Local-relative Transform", "Move and rotate objects relative to the object's local axes", EUserInterfaceActionType::RadioButton, FInputGesture() );

	UI_COMMAND( CycleTransformGizmoCoordSystem, "Cycle Transform Coordinate System", "Cycles the transform gizmo coordinate systems between world and local (object) space", EUserInterfaceActionType::Button, FInputGesture(EKeys::Tilde));
	UI_COMMAND( CycleTransformGizmos, "Cycle Between Translate, Rotate, and Scale", "Cycles the transform gizmos between translate, rotate, and scale", EUserInterfaceActionType::Button, FInputGesture(EKeys::SpaceBar) );
	
	UI_COMMAND( FocusViewportToSelection, "Focus Selected", "Moves the camera in front of the selection", EUserInterfaceActionType::Button, FInputGesture( EKeys::F ) );

	UI_COMMAND( LocationGridSnap, "Grid Snap", "Enables or disables snapping to the grid when dragging objects around", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( RotationGridSnap, "Rotation Snap", "Enables or disables snapping objects to a rotation grid", EUserInterfaceActionType::ToggleButton, FInputGesture() );
	UI_COMMAND( ScaleGridSnap, "Scale Snap", "Enables or disables snapping objects to a scale grid", EUserInterfaceActionType::ToggleButton, FInputGesture() );

	UI_COMMAND( ToggleAutoExposure, "Automatic (Default in-game)", "Enable automatic expose", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure4m, "Fixed Exposure: -4", "Set the fixed exposure to -4", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure3m, "Fixed Exposure: -3", "Set the fixed exposure to -3", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure2m, "Fixed Exposure: -2", "Set the fixed exposure to -2", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure1m, "Fixed Exposure: -1", "Set the fixed exposure to -1", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure0, "Fixed Exposure: 0 (Indoor)", "Set the fixed exposure to 0", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure1p, "Fixed Exposure: +1", "Set the fixed exposure to 1", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure2p, "Fixed Exposure: +2", "Set the fixed exposure to 2", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure3p, "Fixed Exposure: +3", "Set the fixed exposure to 3", EUserInterfaceActionType::RadioButton, FInputGesture() );
	UI_COMMAND( FixedExposure4p, "Fixed Exposure: +4", "Set the fixed exposure to 4", EUserInterfaceActionType::RadioButton, FInputGesture() );


}

