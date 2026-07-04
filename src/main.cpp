///////////////////////////////////////////////////////////////////////////////
// main.cpp
// ========
//
// Application entry point for the Plane Voxel Lab.
//
// This file owns the platform/application shell:
// - GLFW window creation
// - GLAD initialization
// - ImGui initialization
// - renderer initialization
// - input/camera updates
// - fixed-step simulation timing
// - lab update/render calls
//
///////////////////////////////////////////////////////////////////////////////

#include "camera/dev_camera.h"
#include "input/input.h"
#include "lab/lab.h"
#include "renderer/renderer.h"

#include <glad/glad.h>
#include <glfw/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <glm/mat4x4.hpp>

#include <cassert>
#include <cstdio>

/***********************************************************
* GPU Preference (Windows)
************************************************************/

#ifdef _WIN32
typedef unsigned long DWORD;
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

/***********************************************************
* Application Constants
************************************************************/

static constexpr int WINDOW_WIDTH = 1600;
static constexpr int WINDOW_HEIGHT = 900;

static constexpr const char* WINDOW_TITLE = "Plane Voxel Lab";
static constexpr const char* GLSL_VERSION = "#version 450";

/***********************************************************
* Application State
************************************************************/

namespace
{
	struct ApplicationState
	{
		GLFWwindow* window = nullptr;

		InputState input = {};
		Renderer renderer = {};
		DevelopmentCamera developmentCamera = {};

		Lab lab = {};
	};

	struct FrameRenderInfo
	{
		int framebufferWidth = 0;
		int framebufferHeight = 0;

		float aspectRatio = 1.0f;
	};
}

/***********************************************************
* Library Initialization
************************************************************/

static bool initializeGlfw()
{
	if (!glfwInit())
	{
		std::printf("Failed to initialize GLFW.\n");
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	return true;
}

static GLFWwindow* createApplicationWindow()
{
	GLFWwindow* window = glfwCreateWindow(
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		WINDOW_TITLE,
		nullptr,
		nullptr);

	if (window == nullptr)
	{
		std::printf("Failed to create GLFW window.\n");
		return nullptr;
	}

	glfwMakeContextCurrent(window);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSwapInterval(1);

	return window;
}

static bool initializeGlad()
{
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::printf("Failed to initialize GLAD.\n");
		return false;
	}

	return true;
}

static bool initializeImGui(GLFWwindow* window)
{
	assert(window != nullptr);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
	{
		std::printf("Failed to initialize ImGui GLFW backend.\n");
		ImGui::DestroyContext();
		return false;
	}

	if (!ImGui_ImplOpenGL3_Init(GLSL_VERSION))
	{
		std::printf("Failed to initialize ImGui OpenGL backend.\n");
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	return true;
}

static void initializeCamera(DevelopmentCamera& camera)
{
	initializeDevelopmentCamera(camera);
}

/***********************************************************
* Library Shutdown
************************************************************/

static void shutdownImGui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

static void shutdownGlfw(GLFWwindow* window)
{
	if (window != nullptr)
	{
		glfwDestroyWindow(window);
	}

	glfwTerminate();
}

/***********************************************************
* Frame Helpers
************************************************************/

static void beginImGuiFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

static FrameRenderInfo beginAppRenderFrame(GLFWwindow* window)
{
	FrameRenderInfo frameRenderInfo = {};

	glfwGetFramebufferSize(
		window,
		&frameRenderInfo.framebufferWidth,
		&frameRenderInfo.framebufferHeight);

	assert(frameRenderInfo.framebufferWidth > 0);
	assert(frameRenderInfo.framebufferHeight > 0);

	frameRenderInfo.aspectRatio =
		static_cast<float>(frameRenderInfo.framebufferWidth) /
		static_cast<float>(frameRenderInfo.framebufferHeight);

	beginRenderFrame(
		frameRenderInfo.framebufferWidth,
		frameRenderInfo.framebufferHeight);

	return frameRenderInfo;
}

static void endAppRenderFrame(GLFWwindow* window)
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	endRenderFrame();

	glfwSwapBuffers(window);
}

static DevelopmentCameraControls getDevelopmentCameraControls(
	const ApplicationState& app)
{
	DevelopmentCameraControls controls = {};

	if (isActionDown(app.input, InputAction::MoveForward))
	{
		controls.localMovement.z += 1.0f;
	}

	if (isActionDown(app.input, InputAction::MoveBackward))
	{
		controls.localMovement.z -= 1.0f;
	}

	if (isActionDown(app.input, InputAction::MoveRight))
	{
		controls.localMovement.x += 1.0f;
	}

	if (isActionDown(app.input, InputAction::MoveLeft))
	{
		controls.localMovement.x -= 1.0f;
	}

	if (isActionDown(app.input, InputAction::MoveUp))
	{
		controls.localMovement.y += 1.0f;
	}

	if (isActionDown(app.input, InputAction::MoveDown))
	{
		controls.localMovement.y -= 1.0f;
	}

	if (isActionDown(app.input, InputAction::SpeedBoost))
	{
		controls.speedBoost = true;
	}

	controls.lookDelta.x = app.input.mouseDeltaX;
	controls.lookDelta.y = app.input.mouseDeltaY;

	return controls;
}

/***********************************************************
* Debug UI
************************************************************/

static void renderDebugUi(ApplicationState& app)
{
	ImGui::Begin("Plane Voxel Lab Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("OpenGL, GLFW, GLAD, GLM, and ImGui are working.");
	ImGui::Text("OpenGL Version: %s", glGetString(GL_VERSION));

	ImGui::Separator();

	ImGui::Text(
		"Camera Position: %.2f, %.2f, %.2f",
		app.developmentCamera.position.x,
		app.developmentCamera.position.y,
		app.developmentCamera.position.z);

	ImGui::Separator();

	renderLabDebugUi(app.lab);

	ImGui::End();
}

/***********************************************************
* Frame Timing
************************************************************/

namespace
{
	struct FrameTiming
	{
		double previousTime = 0.0;
		double currentTime = 0.0;
		double totalGameTime = 0.0;

		float frameDeltaTime = 0.0f;
		float simulationAccumulator = 0.0f;
	};

	constexpr float FIXED_SIMULATION_DELTA_TIME = 1.0f / 60.0f;
	constexpr float MAX_FRAME_DELTA_TIME = 0.25f;
}

static void initializeFrameTiming(FrameTiming& timing)
{
	timing.previousTime = glfwGetTime();
	timing.currentTime = timing.previousTime;
	timing.totalGameTime = 0.0;

	timing.frameDeltaTime = 0.0f;
	timing.simulationAccumulator = 0.0f;
}

static void updateFrameTiming(FrameTiming& timing)
{
	timing.currentTime = glfwGetTime();

	timing.frameDeltaTime =
		static_cast<float>(timing.currentTime - timing.previousTime);

	timing.previousTime = timing.currentTime;

	if (timing.frameDeltaTime > MAX_FRAME_DELTA_TIME)
	{
		timing.frameDeltaTime = MAX_FRAME_DELTA_TIME;
	}

	timing.simulationAccumulator += timing.frameDeltaTime;
	timing.totalGameTime += static_cast<double>(timing.frameDeltaTime);
}

/***********************************************************
* Application Update
************************************************************/

static void updateFrame(ApplicationState& app, float deltaTime)
{
	if (isActionDown(app.input, InputAction::Quit))
	{
		glfwSetWindowShouldClose(app.window, GLFW_TRUE);
		return;
	}

	DevelopmentCameraControls controls = getDevelopmentCameraControls(app);

	updateDevelopmentCamera(
		app.developmentCamera,
		controls,
		deltaTime);

	updateLab(
		app.lab,
		app.input,
		deltaTime);
}

static void updateSimulation(ApplicationState& app, float fixedDeltaTime)
{
	(void)app;
	(void)fixedDeltaTime;

	// Fixed-step simulation updates will live here if the lab needs them.
}

/***********************************************************
* Application Rendering
************************************************************/

static void renderApp(
	ApplicationState& app,
	float interpolationAlpha,
	const FrameRenderInfo& frameRenderInfo)
{
	(void)interpolationAlpha;

	glm::mat4 viewProjection = buildDevelopmentCameraViewProjectionMatrix(
		app.developmentCamera,
		frameRenderInfo.aspectRatio);

	renderLab(
		app.lab,
		app.renderer,
		viewProjection);
}

/***********************************************************
* Main Loop
************************************************************/

static void runGameLoop(ApplicationState& app)
{
	FrameTiming timing = {};
	initializeFrameTiming(timing);

	while (!glfwWindowShouldClose(app.window))
	{
		updateFrameTiming(timing);

		glfwPollEvents();

		updateInput(app.input, app.window);

		updateFrame(app, timing.frameDeltaTime);

		while (timing.simulationAccumulator >= FIXED_SIMULATION_DELTA_TIME)
		{
			updateSimulation(app, FIXED_SIMULATION_DELTA_TIME);

			timing.simulationAccumulator -= FIXED_SIMULATION_DELTA_TIME;
		}

		const float interpolationAlpha =
			timing.simulationAccumulator / FIXED_SIMULATION_DELTA_TIME;

		beginImGuiFrame();

		FrameRenderInfo frameRenderInfo = beginAppRenderFrame(app.window);

		renderApp(app, interpolationAlpha, frameRenderInfo);
		renderDebugUi(app);

		endAppRenderFrame(app.window);
	}
}

/***********************************************************
* Main
************************************************************/

int main()
{
	ApplicationState app = {};

	if (!initializeGlfw())
	{
		return -1;
	}

	app.window = createApplicationWindow();
	if (app.window == nullptr)
	{
		shutdownGlfw(app.window);
		return -1;
	}

	initializeInput(app.input, app.window);

	if (!initializeGlad())
	{
		shutdownGlfw(app.window);
		return -1;
	}

	if (!initializeRenderer(app.renderer))
	{
		shutdownGlfw(app.window);
		return -1;
	}

	initializeCamera(app.developmentCamera);

	if (!initializeLab(app.lab))
	{
		shutdownRenderer(app.renderer);
		shutdownGlfw(app.window);
		return -1;
	}

	if (!initializeImGui(app.window))
	{
		shutdownLab(app.lab);
		shutdownRenderer(app.renderer);
		shutdownGlfw(app.window);
		return -1;
	}

	runGameLoop(app);

	shutdownLab(app.lab);
	shutdownImGui();
	shutdownRenderer(app.renderer);
	shutdownGlfw(app.window);

	return 0;
}
