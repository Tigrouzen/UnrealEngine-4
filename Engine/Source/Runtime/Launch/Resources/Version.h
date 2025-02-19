// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#define ENGINE_MAJOR_VERSION	4
#define ENGINE_MINOR_VERSION	0
#define ENGINE_PATCH_VERSION	1

#define ENGINE_VERSION 0
#define ENGINE_VERSION_HIWORD 0
#define ENGINE_VERSION_LOWORD 0

#define BUILT_FROM_CHANGELIST 0
#define BRANCH_NAME "UE4"

#define EPIC_COMPANY_NAME  "Epic Games, Inc."
#define EPIC_COPYRIGHT_STRING "Copyright 1998-2014 Epic Games, Inc. All Rights Reserved."
#define EPIC_PRODUCT_NAME "Unreal Engine"
#define EPIC_PRODUCT_IDENTIFIER "UnrealEngine"

#define ENGINE_VERSION_TEXT_2(x) L ## x
#define ENGINE_VERSION_TEXT(x) ENGINE_VERSION_TEXT_2(x)
#define ENGINE_VERSION_STRINGIFY_2(x) ENGINE_VERSION_TEXT(#x)
#define ENGINE_VERSION_STRINGIFY(x) ENGINE_VERSION_STRINGIFY_2(x)

#define ENGINE_VERSION_STRING \
	ENGINE_VERSION_STRINGIFY(ENGINE_MAJOR_VERSION) \
	ENGINE_VERSION_TEXT(".") \
	ENGINE_VERSION_STRINGIFY(ENGINE_MINOR_VERSION) \
	ENGINE_VERSION_TEXT(".") \
	ENGINE_VERSION_STRINGIFY(ENGINE_PATCH_VERSION) \
	ENGINE_VERSION_TEXT("-") \
	ENGINE_VERSION_STRINGIFY(BUILT_FROM_CHANGELIST) \
	ENGINE_VERSION_TEXT("+") \
	ENGINE_VERSION_TEXT(BRANCH_NAME)
