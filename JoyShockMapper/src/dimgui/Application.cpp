#include "dimgui/Application.h"
#include "InputHelpers.h"
#include "JSMVariable.hpp"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer.h"
#include "SettingsManager.h"
#include "CmdRegistry.h"
#include <regex>
#include <span>

#define ImTextureID SDL_Texture

extern vector<JSMButton> mappings;

using namespace magic_enum;
using namespace ImGui;
#define EndMenu() ImGui::EndMenu(); // Symbol conflict with windows

static void HelpMarker(string_view desc)
{
	SameLine();
	TextDisabled("(?)");
	if (IsItemHovered(ImGuiHoveredFlags_DelayShort))
	{
		BeginTooltip();
		PushTextWrapPos(GetFontSize() * 35.0f);
		TextUnformatted(desc.data());
		PopTextWrapPos();
		EndTooltip();
	}
}

Application::Application(const CmdRegistry& cmds)
  : _cmds(cmds)
{
}

std::string_view getButtonLabel(ButtonID id)
{
	return mappings[enum_integer(id)].label();
}

template<typename T>
void drawCombo(JSMSetting<T>& setting, ImGuiComboFlags flags = 0)
{
	SetNextItemWidth(150.f);
	if (BeginCombo(enum_name(setting._id).data(), enum_name(setting.value()).data(), flags))
	{
		for (auto [enumVal, enumStr] : enum_entries<T>())
		{
			if (enumVal == T::INVALID)
				continue;

			bool disabled = false;
			if constexpr (is_same_v<T, TriggerMode>)
			{
				if (enumVal == TriggerMode::X_LT || enumVal == TriggerMode::X_RT)
				{
					auto virtual_controller = SettingsManager::getV<ControllerScheme>(SettingID::VIRTUAL_CONTROLLER)->value();
					disabled = virtual_controller == ControllerScheme::NONE;
					if (virtual_controller == ControllerScheme::DS4)
					{
						enumStr = enumVal == TriggerMode::X_LT ? "PS_L2" : "PS_R2";
					}
				}
			}
			if constexpr (is_same_v<T, StickMode>)
			{
				auto virtual_controller = SettingsManager::getV<ControllerScheme>(SettingID::VIRTUAL_CONTROLLER)->value();
				if (enumVal >= StickMode::LEFT_STICK && enumVal <= StickMode::RIGHT_STEER_X)
				{
					disabled = virtual_controller == ControllerScheme::NONE;
				}
				if (setting._id != SettingID::MOTION_STICK_MODE &&
				  (enumVal == StickMode::LEFT_STEER_X || enumVal == StickMode::RIGHT_STEER_X))
				{
					continue;
				}
			}
			BeginDisabled(disabled);
			if (Selectable(enumStr.data(), enumVal == setting.value()))
			{
				setting.set(enumVal); // set global variable
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (enumVal == setting.value())
				SetItemDefaultFocus();
			EndDisabled();
		}
		EndCombo();
	}
}

void Application::drawButton(ButtonID btn, ImVec2 size, bool enabled)
{
	std::stringstream description;
	description << mappings[enum_integer(btn)].value().description(); // Button label to display
	description << "###" << enum_name(btn);                           // Button ID for ImGui

	BeginDisabled(!enabled);
	if (Button(description.str().data(), size))
	{
		_inputSelector.show(btn);
	}

	string_view label = getButtonLabel(btn).data();
	if (IsItemHovered() && !label.empty())
	{
		SetTooltip(label.data());
	}
	EndDisabled();
}

void Application::run()
{
	threadDone = std::async([this]()
	  { return DrawLoop(); });
}

void Application::StopLoop()
{
	done = true;
	threadDone.get();
}

bool Application::DrawLoop()
{
	// Setup window
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI); //  | SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN_DESKTOP
	window = SDL_CreateWindow("JoyShockMapper", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);


	// Setup SDL_Renderer instance
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
	if (!renderer || !window)
		exit(0);
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(renderer, &info);
	// SDL_Log("Current SDL_Renderer: %s", info.name);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	CreateContext();
	ImGuiIO& io = GetIO();
	(void)io;
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	StyleColorsDark();
	// StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer_Init(renderer);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	// io.Fonts->AddFontDefault();
	// io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	// ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	// IM_ASSERT(font != NULL);

	GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	SDL_Surface* image = SDL_LoadBMP("ds4.bmp");
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, image);

	// Add window transparency
	// MakeWindowTransparent(window, RGB(Uint8(clear_color.x * 255), Uint8(clear_color.y * 255), Uint8(clear_color.z * 255)), Uint8(clear_color.w * 255));
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version); // Initialize wmInfo
	SDL_GetWindowWMInfo(window, &wmInfo);
	HWND hWnd = wmInfo.info.win.window;

	// Main loop
	while (!done)
	{
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		SDL_Event event;
		while (!done && SDL_PollEvent(&event) != 0)
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
				done = true;
		}

		if (done)
			break;

		auto &style = GetStyle();
		style.Alpha = 1.0f;
		style.Colors[ImGuiCol_WindowBg].w = 0;

		// Start the Dear ImGui frame
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		NewFrame();

		DockSpaceOverViewport(GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

		// 1. Show the big demo window (Most of the sample code is in ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
		// static float f = 0.0f;
		using namespace ImGui;
		if (BeginMainMenuBar())
		{
			if (BeginMenu("File"))
			{
				if (MenuItem("New", "CTRL+N", nullptr, false))
				{
				}
				if (MenuItem("Open", "CTRL+O", nullptr, false))
				{
				}
				if (MenuItem("Save", "CTRL+S", nullptr, false))
				{
				}
				if (MenuItem("Save As...", "SHIFT+CTRL+S", nullptr, false))
				{
				}
				Separator();
				if (MenuItem("On Startup", nullptr, nullptr, false))
				{
				}
				if (MenuItem("On Reset", nullptr, nullptr, false))
				{
				}
				if (BeginMenu("Templates"))
				{
					if (MenuItem("_2DMouse", nullptr, nullptr, false))
					{
					}
					if (MenuItem("_3DMouse", nullptr, nullptr, false))
					{
					}
					EndMenu();
				}
				if (BeginMenu("AutoLoad"))
				{
					if (MenuItem("game1", nullptr, nullptr, false))
					{
					}
					if (MenuItem("game2", nullptr, nullptr, false))
					{
					}
					EndMenu();
				}
				Separator();
				if (MenuItem("Quit"))
				{
					done = true;
				}
				EndMenu();
			}
			if (BeginMenu("Commands"))
			{
				if (MenuItem("Reset Mappings"))
				{
					WriteToConsole("RESET_MAPPINGS");
				}
				HelpMarker(_cmds.GetHelp("RESET_MAPPINGS"));
				static bool mergeJoycons = true;
				Checkbox("Merge Joycons", &mergeJoycons);
				HelpMarker("If checked, complimentary joycons will be consider two parts of a single controller");
				if (MenuItem("Reconnect Controllers"))
				{
					if (mergeJoycons)
						WriteToConsole("RECONNECT_CONTROLLERS MERGE");
					else
						WriteToConsole("RECONNECT_CONTROLLERS SPLIT");
				}
				HelpMarker(_cmds.GetHelp("RECONNECT_CONTROLLERS"));
				static bool counterOsMouseSpeed = false;
				if (Checkbox("Counter OS Mouse Speed", &counterOsMouseSpeed))
				{
					if (counterOsMouseSpeed)
						WriteToConsole("COUNTER_OS_MOUSE_SPEED");
					else
						WriteToConsole("IGNORE_OS_MOUSE_SPEED");
				}
				HelpMarker(_cmds.GetHelp("COUNTER_OS_MOUSE_SPEED"));
				if (MenuItem("Calculate Real World Calibration"))
				{
					WriteToConsole("CALCULATE_REAL_WORLD_CALIBRATION");
				}
				HelpMarker(_cmds.GetHelp("CALCULATE_REAL_WORLD_CALIBRATION"));
				Separator();
				static float duration = 3.f;
				SliderFloat("Calibration duration", &duration, 0.5f, 5.0f);
				if (MenuItem("Calibrate All Controllers"))
				{
					auto t = std::thread([]()
					  {
					WriteToConsole("RESTART_GYRO_CALIBRATION");
					int32_t ms = duration * 1000.f;
					Sleep(ms); // ms
					WriteToConsole("FINISH_GYRO_CALIBRATION"); });
					t.detach();
				}
				HelpMarker(_cmds.GetHelp("RESTART_GYRO_CALIBRATION"));
				if (MenuItem("Reset Motion Stick"))
				{
					WriteToConsole("SET_MOTION_STICK_NEUTRAL");
				}
				HelpMarker(_cmds.GetHelp("SET_MOTION_STICK_NEUTRAL"));
				Separator();
				static bool whitelistAdd = false;
				if (Checkbox("Add to Whitelister", &whitelistAdd))
				{
					if (whitelistAdd)
						WriteToConsole("WHITELIST_ADD");
					else
						WriteToConsole("WHITELIST_REMOVE");
				}
				HelpMarker(_cmds.GetHelp("WHITELIST_ADD"));
				if (MenuItem("Show Whitelister"))
				{
					WriteToConsole("WHITELIST_SHOW");
				}
				HelpMarker(_cmds.GetHelp("WHITELIST_SHOW"));
				if (MenuItem("Calibrate Adaptive Triggers"))
				{
					WriteToConsole("CALIBRATE_TRIGGERS");
					ShowConsole();
				}
				HelpMarker(_cmds.GetHelp("CALIBRATE_TRIGGERS"));
				EndMenu();
			}
			if (BeginMenu("Debug"))
			{
				Checkbox("Show ImGui demo", &show_demo_window);
				MenuItem("Record a bug", nullptr, false, false);
				EndMenu();
			}
			if (BeginMenu("Help"))
			{
				MenuItem("Read Me", nullptr, false, false);
				MenuItem("Check For Updates", nullptr, false, false);
				MenuItem("About", nullptr, false, false);
				EndMenu();
			}
			EndMainMenuBar();
		}

		ImVec2 renderingAreaPos;
		ImVec2 renderingAreaSize;
		Begin("MainWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove);

		bool showBackground = BeginTabBar("Main");
		//SetNextWindowBgAlpha(0.f);
		if (ImGui::BeginTabItem("Button Bindings"))
		{
			auto drawlabel = [this](ButtonID btn)
			{
				AlignTextToFramePadding();
				Text(enum_name(btn).data());
				if (IsItemHovered())
				{
					SetTooltip(_cmds.GetHelp(enum_name(btn)).data());
				}
			};
			auto mainWindowSize = ImGui::GetContentRegionAvail();
			// Left
			BeginChild("Left Bindings", { mainWindowSize.x * 1.f / 5.f, 0.f });
			ImVec2 buttonSize = ImVec2{ 0.f, 0.f };
			drawButton(ButtonID::ZLF, buttonSize, SettingsManager::get<TriggerMode>(SettingID::ZL_MODE)->value() != TriggerMode::NO_FULL);
			SameLine();
			drawlabel(ButtonID::ZLF);
			drawCombo(*SettingsManager::get<TriggerMode>(SettingID::ZL_MODE));
			if (IsItemHovered())
			{
				SetTooltip(_cmds.GetHelp(enum_name(SettingID::ZL_MODE)).data());
			}
			drawButton(ButtonID::ZL, buttonSize);
			SameLine();
			drawlabel(ButtonID::ZL);
			drawButton(ButtonID::L, buttonSize);
			SameLine();
			drawlabel(ButtonID::L);
			drawButton(ButtonID::UP, buttonSize);
			SameLine();
			drawlabel(ButtonID::UP);
			drawButton(ButtonID::LEFT, buttonSize);
			SameLine();
			drawlabel(ButtonID::LEFT);
			drawButton(ButtonID::RIGHT, buttonSize);
			SameLine();
			drawlabel(ButtonID::RIGHT);
			drawButton(ButtonID::DOWN, buttonSize);
			SameLine();
			drawlabel(ButtonID::DOWN);
			drawButton(ButtonID::L3, buttonSize);
			SameLine();
			drawlabel(ButtonID::L3);
			drawCombo(*SettingsManager::get<StickMode>(SettingID::LEFT_STICK_MODE));
			if (IsItemHovered())
			{
				SetTooltip(_cmds.GetHelp(enum_name(SettingID::LEFT_STICK_MODE)).data());
			}
			_inputSelector.draw();
			EndChild();
			ImGui::SameLine();

			// Right
			ImGui::BeginGroup();
			BeginChild("Top buttons", { mainWindowSize.x * 3.f / 5.f, 60.f });
			drawButton(ButtonID::MINUS, buttonSize);
			SameLine();
			drawButton(ButtonID::TOUCH, buttonSize);
			SameLine();
			drawCombo(*SettingsManager::get<TriggerMode>(SettingID::TOUCHPAD_DUAL_STAGE_MODE), ImGuiComboFlags_NoPreview);
			SameLine();
			drawButton(ButtonID::CAPTURE, buttonSize);
			SameLine();
			drawButton(ButtonID::PLUS, buttonSize);
			_inputSelector.draw();
			EndChild();

			renderingAreaSize = { mainWindowSize.x * 3.f / 5.f, GetContentRegionAvail().y };
			BeginChild("Rendering window", renderingAreaSize, false);
			renderingAreaPos = ImGui::GetWindowPos();
			EndChild();
			EndGroup();

			SameLine();
			BeginChild("Right Bindings");
			drawButton(ButtonID::ZRF, buttonSize, SettingsManager::get<TriggerMode>(SettingID::ZR_MODE)->value() != TriggerMode::NO_FULL);
			drawCombo(*SettingsManager::get<TriggerMode>(SettingID::ZR_MODE));
			drawButton(ButtonID::ZR, buttonSize);
			drawButton(ButtonID::R, buttonSize);
			drawButton(ButtonID::N, buttonSize);
			drawButton(ButtonID::E, buttonSize);
			drawButton(ButtonID::W, buttonSize);
			drawButton(ButtonID::S, buttonSize);
			drawButton(ButtonID::R3, buttonSize);
			drawCombo(*SettingsManager::get<StickMode>(SettingID::RIGHT_STICK_MODE));
			_inputSelector.draw();
			EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Settings"))
		{
			ImGui::Text("This is the Avocado tab!\nblah blah blah blah blah");
			ImGui::EndTabItem();
		}
		EndTabBar(); // Main
		End();       // MainWindow

		SDL_Rect bgDims{
			renderingAreaPos.x,
			renderingAreaPos.y,
			renderingAreaSize.x,
			renderingAreaSize.y
		};

		ImVec2 upLeft{ float(bgDims.x), float(bgDims.y) };
		ImVec2 lowRight{ float(bgDims.x + bgDims.w), float(bgDims.y + bgDims.h) };
		GetBackgroundDrawList()->AddImage(texture, upLeft, lowRight);

		// Rendering
		SDL_RenderClear(renderer);
		Render();
		if (showBackground && !_inputSelector.isShowing())
		{
			SDL_SetTextureBlendMode(texture, SDL_BlendMode::SDL_BLENDMODE_BLEND);
			SDL_RenderCopy(renderer, texture, nullptr, &bgDims);
		}
		ImGui_ImplSDLRenderer_RenderDrawData(GetDrawData());
		SDL_RenderPresent(renderer);
	}

	// Cleanup
	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	DestroyContext();

	SDL_DestroyTexture(texture);
	SDL_FreeSurface(image);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	WriteToConsole("QUIT");

	return done;
}