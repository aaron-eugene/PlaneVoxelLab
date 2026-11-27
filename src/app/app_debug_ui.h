///////////////////////////////////////////////////////////////////////////////
// app/app_debug_ui.h
// ==================
//
// Declares the application-level debug UI.
//
// Owns the presentation of shared Lab and active-experiment
// debug windows.
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

struct DevelopmentCamera;
struct Lab;

/***********************************************************
* Application Debug UI
************************************************************/

void renderAppDebugUi(
	Lab& lab,
	const DevelopmentCamera& camera);
