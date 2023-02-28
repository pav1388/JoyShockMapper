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

InputSelector Application::BindingTab::_inputSelector;

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
	_tabs.emplace_back("Core Bindings", _cmds);
}

std::string_view getButtonLabel(ButtonID id)
{
	return mappings[enum_integer(id)].label();
}

template<typename T>
void drawCombo(JSMSetting<T>& setting, ImGuiComboFlags flags = 0)
{
	SetNextItemWidth(150.f);
	string name("##");
	name.append(enum_name(setting.value()).data());
	if (BeginCombo(name.c_str(), enum_name(setting.value()).data(), flags))
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
					// steer stick mode is only valid for the motion stick, don't add to other combo boxes
					continue;
				}
				if (enumVal == StickMode::INNER_RING || enumVal == StickMode::OUTER_RING)
				{
					// Don't add legacy commands to UI. Use setting "XXXX_RING_MODE = [INNER|OUTER]" instead
					continue;
				}
			}
			if (disabled)
				BeginDisabled();
			if (Selectable(enumStr.data(), enumVal == setting.value()))
			{
				setting.set(enumVal); // set global variable
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (enumVal == setting.value())
				SetItemDefaultFocus();
			if (disabled)
				EndDisabled();
		}
		EndCombo();
	}
}

void Application::BindingTab::drawButton(ButtonID btn, ImVec2 size)
{
	std::stringstream labelID;
	string desc(mappings[enum_integer(btn)].value().description());
	for (auto pos = desc.find("and"); pos != string::npos; pos = desc.find("and", pos))
	{
		desc.insert(pos, "\n");
		pos += 2;
	}
	labelID << desc;                    // Button label to display
	labelID << "###" << enum_name(btn); // Button ID for ImGui

	if (Button(labelID.str().data(), size))
	{
		_showPopup = btn;
	}

	string_view label = getButtonLabel(btn).data();
	if (IsItemHovered() && !label.empty())
	{
		SetTooltip(label.data());
	}
	if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight))
	{
		if (MenuItem("Set", "Left Click"))
		{
			_showPopup = btn;
		}
		if (MenuItem("Clear", ""))
		{
			mappings[enum_integer(btn)].set(Mapping::NO_MAPPING);
			mappings[enum_integer(btn)].updateLabel("");
		}
		MenuItem("Chord this button", "Middle Click", false, false);
		if (BeginMenu("Simultaneous Press with"))
		{
			for (auto pair = enum_entries<ButtonID>().begin(); pair->first < ButtonID::SIZE; ++pair)
			{

				if (pair->first != btn)
				{
					if (MenuItem(pair->second.data(), nullptr, false, false))
					{
						auto simMap = mappings[enum_integer(btn)].atSimPress(pair->first);
						// TODO: set simMap to some value using the input selectore and create a display for it somewhere in the UI
					}
				}
			}
			EndMenu();
		}
		drawLabel(SettingID::HOLD_PRESS_TIME);
		SameLine();
		drawAnyFloat(SettingID::HOLD_PRESS_TIME);

		drawLabel(SettingID::TURBO_PERIOD);
		SameLine();
		drawAnyFloat(SettingID::TURBO_PERIOD);

		drawLabel(SettingID::DBL_PRESS_WINDOW);
		SameLine();
		drawAnyFloat(SettingID::DBL_PRESS_WINDOW);

		drawLabel(SettingID::SIM_PRESS_WINDOW);
		SameLine();
		drawAnyFloat(SettingID::SIM_PRESS_WINDOW);

		EndPopup();
	}
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

		auto& style = GetStyle();
		style.Alpha = 1.0f;

		// Start the Dear ImGui frame
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		NewFrame();

		PushStyleColor(ImGuiCol_WindowBg, { 0.f, 0.f, 0.f, 0.f });
		DockSpaceOverViewport(GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		PopStyleColor();

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
		Begin("MainWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

		bool showBackground = BeginTabBar("BindingsTab");
		for (auto bindingTab : _tabs)
		{
			bindingTab.draw(renderingAreaPos, renderingAreaSize);
		}
		EndTabBar(); // BindingsTab
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
		SDL_SetTextureBlendMode(texture, SDL_BlendMode::SDL_BLENDMODE_BLEND);
		SDL_RenderCopy(renderer, texture, nullptr, &bgDims);
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

Application::BindingTab::BindingTab(string_view name, const CmdRegistry& cmds, ButtonID chord)
  : _name(name)
  , _cmds(cmds)
  , _chord(chord)
{
}

void Application::BindingTab::drawLabel(ButtonID btn)
{
	AlignTextToFramePadding();
	Text(enum_name(btn).data());
	if (IsItemHovered())
	{
		SetTooltip(_cmds.GetHelp(enum_name(btn)).data());
	}
};
void Application::BindingTab::drawLabel(SettingID stg)
{
	AlignTextToFramePadding();
	Text(enum_name(stg).data());
	if (IsItemHovered())
	{
		SetTooltip(_cmds.GetHelp(enum_name(stg)).data());
	}
};
void Application::BindingTab::drawLabel(string_view cmd)
{
	AlignTextToFramePadding();
	Text(cmd.data());
	if (IsItemHovered())
	{
		SetTooltip(_cmds.GetHelp(cmd).data());
	}
}

void Application::BindingTab::drawAnyFloat(SettingID stg)
{
	auto setting = SettingsManager::get<float>(stg);
	auto value = setting->value();
	stringstream ss;
	ss << "##" << enum_name(stg);
	if (InputFloat(ss.str().data(), &value, 0.f, 0.f, "%3f", ImGuiInputTextFlags_EnterReturnsTrue))
	{
		setting->set(value);
	}
}

void Application::BindingTab::drawPercentFloat(SettingID stg)
{
	auto setting = SettingsManager::get<float>(stg);
	float value = setting->value();
	stringstream ss;
	ss << "##" << enum_name(stg);
	if (SliderFloat(ss.str().data(), &value, 0.f, 1.f, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue))
	{
		setting->set(value);
	}
}

void Application::BindingTab::drawAny2Floats(SettingID stg)
{
	auto setting = SettingsManager::get<FloatXY>(stg);
	FloatXY value = setting->value();
	stringstream ss;
	ss << "##" << enum_name(stg);
	if (InputFloat2(ss.str().data(), &value.first, "%.0f", ImGuiInputTextFlags_EnterReturnsTrue))
	{
		setting->set(value);
	}
}

void Application::BindingTab::draw(ImVec2& renderingAreaPos, ImVec2& renderingAreaSize)
{
	if (ImGui::BeginTabItem(_name.data()))
	{
		auto mainWindowSize = ImGui::GetContentRegionAvail();

		// static int sizingPolicy = 0;
		// int sizingPolicies[] = { ImGuiTableFlags_SizingFixedFit, ImGuiTableFlags_SizingFixedSame, ImGuiTableFlags_SizingStretchProp, ImGuiTableFlags_SizingStretchSame };
		// Combo("Sizing policy", &sizingPolicy, "FixedFit\0FixedSame\0StretchProp\0StretchSame");

		// Left
		BeginChild("Left Bindings", { mainWindowSize.x * 1.f / 5.f, 0.f }, false, ImGuiWindowFlags_AlwaysAutoResize);
		if (BeginTable("LeftTable", 2, ImGuiTableFlags_SizingStretchSame))
		{
			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::ZLF);
			TableSetColumnIndex(1);
			bool disabled = SettingsManager::get<TriggerMode>(SettingID::ZL_MODE)->value() == TriggerMode::NO_FULL;
			if (disabled)
				BeginDisabled();
			drawButton(ButtonID::ZLF);
			if (disabled)
				EndDisabled();

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(SettingID::ZL_MODE);
			TableSetColumnIndex(1);
			drawCombo(*SettingsManager::get<TriggerMode>(SettingID::ZL_MODE));

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::ZL);
			TableSetColumnIndex(1);
			drawButton(ButtonID::ZL);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::L);
			TableSetColumnIndex(1);
			drawButton(ButtonID::L);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::UP);
			TableSetColumnIndex(1);
			drawButton(ButtonID::UP);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::LEFT);
			TableSetColumnIndex(1);
			drawButton(ButtonID::LEFT);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::RIGHT);
			TableSetColumnIndex(1);
			drawButton(ButtonID::RIGHT);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::DOWN);
			TableSetColumnIndex(1);
			drawButton(ButtonID::DOWN);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::LSL);
			TableSetColumnIndex(1);
			drawButton(ButtonID::LSL);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::LSR);
			TableSetColumnIndex(1);
			drawButton(ButtonID::LSR);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(ButtonID::L3);
			TableSetColumnIndex(1);
			drawButton(ButtonID::L3);

			TableNextRow();
			TableSetColumnIndex(0);
			drawCombo(*SettingsManager::get<RingMode>(SettingID::LEFT_RING_MODE), ImGuiComboFlags_NoPreview);
			if (IsItemHovered())
				SetTooltip(_cmds.GetHelp(enum_name(SettingID::LEFT_RING_MODE)).data());
			SameLine();
			drawLabel(ButtonID::LRING);
			TableSetColumnIndex(1);
			drawButton(ButtonID::LRING);

			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel(SettingID::LEFT_STICK_MODE);
			TableSetColumnIndex(1);
			drawCombo(*SettingsManager::get<StickMode>(SettingID::LEFT_STICK_MODE));

			StickMode leftStickMode = SettingsManager::get<StickMode>(SettingID::LEFT_STICK_MODE)->value();
			if (leftStickMode == StickMode::NO_MOUSE || leftStickMode == StickMode::OUTER_RING || leftStickMode == StickMode::INNER_RING)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(ButtonID::LUP);
				TableSetColumnIndex(1);
				drawButton(ButtonID::LUP);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(ButtonID::LLEFT);
				TableSetColumnIndex(1);
				drawButton(ButtonID::LLEFT);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(ButtonID::LRIGHT);
				TableSetColumnIndex(1);
				drawButton(ButtonID::LRIGHT);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(ButtonID::LDOWN);
				TableSetColumnIndex(1);
				drawButton(ButtonID::LDOWN);
			}
			else if (leftStickMode == StickMode::FLICK || leftStickMode == StickMode::FLICK_ONLY || leftStickMode == StickMode::ROTATE_ONLY)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::FLICK_SNAP_MODE);
				TableSetColumnIndex(1);
				drawCombo(*SettingsManager::get<FlickSnapMode>(SettingID::FLICK_SNAP_MODE));

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::FLICK_SNAP_STRENGTH);
				TableSetColumnIndex(1);
				drawPercentFloat(SettingID::FLICK_SNAP_STRENGTH);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::FLICK_DEADZONE_ANGLE);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::FLICK_DEADZONE_ANGLE);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::FLICK_STICK_OUTPUT);
				TableSetColumnIndex(1);
				auto& fsOut = *SettingsManager::get<GyroOutput>(SettingID::FLICK_STICK_OUTPUT);
				drawCombo(fsOut);

				if (fsOut == GyroOutput::MOUSE)
				{
					TableNextRow();
					TableSetColumnIndex(0);
					drawLabel(SettingID::FLICK_TIME);
					TableSetColumnIndex(1);
					drawAnyFloat(SettingID::FLICK_TIME);

					TableNextRow();
					TableSetColumnIndex(0);
					drawLabel(SettingID::FLICK_TIME_EXPONENT);
					TableSetColumnIndex(1);
					drawAnyFloat(SettingID::FLICK_TIME_EXPONENT);

					TableNextRow();
					TableSetColumnIndex(0);
					drawLabel(SettingID::REAL_WORLD_CALIBRATION);
					TableSetColumnIndex(1);
					drawAnyFloat(SettingID::REAL_WORLD_CALIBRATION);

					TableNextRow();
					TableSetColumnIndex(0);
					drawLabel(SettingID::IN_GAME_SENS);
					TableSetColumnIndex(1);
					drawAnyFloat(SettingID::IN_GAME_SENS);
				}
				else // LEFT_STICK, RIGHT_STICK, PS_MOTION
				{
					TableNextRow();
					TableSetColumnIndex(0);
					drawLabel(SettingID::VIRTUAL_STICK_CALIBRATION);
					TableSetColumnIndex(1);
					drawAnyFloat(SettingID::VIRTUAL_STICK_CALIBRATION);
				}
			}
			else if (leftStickMode == StickMode::AIM)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::STICK_POWER);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::STICK_POWER);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::STICK_SENS);
				TableSetColumnIndex(1);
				drawAny2Floats(SettingID::STICK_SENS);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::REAL_WORLD_CALIBRATION);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::REAL_WORLD_CALIBRATION);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::IN_GAME_SENS);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::IN_GAME_SENS);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::STICK_ACCELERATION_RATE);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::STICK_ACCELERATION_RATE);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::STICK_ACCELERATION_CAP);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::STICK_ACCELERATION_CAP);
			}
			else if (leftStickMode == StickMode::MOUSE_AREA)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::MOUSE_RING_RADIUS);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::MOUSE_RING_RADIUS);
			}
			else if (leftStickMode == StickMode::MOUSE_RING)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::MOUSE_RING_RADIUS);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::MOUSE_RING_RADIUS);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::SCREEN_RESOLUTION_X);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::SCREEN_RESOLUTION_X);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::SCREEN_RESOLUTION_X);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::SCREEN_RESOLUTION_X);
			}
			else if (leftStickMode == StickMode::SCROLL_WHEEL)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(SettingID::SCROLL_SENS);
				TableSetColumnIndex(1);
				drawAnyFloat(SettingID::SCROLL_SENS);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(ButtonID::LLEFT);
				TableSetColumnIndex(1);
				drawButton(ButtonID::LLEFT);

				TableNextRow();
				TableSetColumnIndex(0);
				drawLabel(ButtonID::LRIGHT);
				TableSetColumnIndex(1);
				drawButton(ButtonID::LRIGHT);
			}
			// Handle Controller ones
			else
			{
				Text("%s, Lorem ipsum dolor sit amet, consectetur adipiscing elit...", enum_name(leftStickMode).data());
			}

			EndTable();
		}
		EndChild();

		ImGui::SameLine();

		// Right
		BeginGroup();
		BeginChild("Top buttons", { mainWindowSize.x * 3.f / 5.f, 60.f }, ImGuiWindowFlags_None);
		if (BeginTable("TopTable", 6, ImGuiTableFlags_SizingStretchSame))
		{
			TableNextRow();
			TableSetColumnIndex(0);
			drawLabel("-");
			TableSetColumnIndex(1);
			drawLabel(ButtonID::TOUCH);
			SameLine();
			drawCombo(*SettingsManager::get<TriggerMode>(SettingID::TOUCHPAD_DUAL_STAGE_MODE), ImGuiComboFlags_NoPreview);
			if (IsItemHovered())
				SetTooltip(_cmds.GetHelp(enum_name(SettingID::TOUCHPAD_DUAL_STAGE_MODE)).data());
			TableSetColumnIndex(2);
			drawLabel(ButtonID::CAPTURE);
			TableSetColumnIndex(3);
			drawLabel(ButtonID::HOME);
			TableSetColumnIndex(4);
			drawLabel(ButtonID::MIC);
			TableSetColumnIndex(5);
			drawLabel("+");

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::MINUS);
			TableSetColumnIndex(1);
			drawButton(ButtonID::TOUCH);
			TableSetColumnIndex(2);
			bool disabled = SettingsManager::get<TriggerMode>(SettingID::TOUCHPAD_DUAL_STAGE_MODE)->value() == TriggerMode::NO_FULL;
			if (disabled)
				BeginDisabled();
			drawButton(ButtonID::CAPTURE);
			if (disabled)
				EndDisabled();
			TableSetColumnIndex(3);
			drawButton(ButtonID::HOME);
			TableSetColumnIndex(4);
			drawButton(ButtonID::MIC);
			TableSetColumnIndex(5);
			drawButton(ButtonID::PLUS);

			EndTable();
		}
		EndChild();

		renderingAreaSize = { mainWindowSize.x * 3.f / 5.f, GetContentRegionAvail().y };
		BeginChild("Rendering window", renderingAreaSize, false);
		renderingAreaPos = ImGui::GetWindowPos();
		EndChild(); // Top buttons
		EndGroup();

		SameLine();
		BeginChild("Right Bindings");
		if (BeginTable("RightTable", 2, ImGuiTableFlags_SizingStretchSame))
		{
			TableNextRow();
			TableSetColumnIndex(0);
			bool disabled = SettingsManager::get<TriggerMode>(SettingID::ZL_MODE)->value() == TriggerMode::NO_FULL;
			if (disabled)
				BeginDisabled();
			drawButton(ButtonID::ZRF);
			if (disabled)
				EndDisabled();
			TableSetColumnIndex(1);
			drawLabel(ButtonID::ZRF);

			TableNextRow();
			TableSetColumnIndex(0);
			drawCombo(*SettingsManager::get<TriggerMode>(SettingID::ZR_MODE));
			TableSetColumnIndex(1);
			drawLabel(SettingID::ZR_MODE);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::ZR);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::ZR);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::R);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::R);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::N);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::N);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::E);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::E);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::W);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::W);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::S);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::S);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::RSR);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::RSR);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::RSL);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::RSL);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::R3);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::R3);

			TableNextRow();
			TableSetColumnIndex(0);
			drawButton(ButtonID::RRING);
			TableSetColumnIndex(1);
			drawLabel(ButtonID::RRING);
			SameLine();
			drawCombo(*SettingsManager::get<RingMode>(SettingID::RIGHT_RING_MODE), ImGuiComboFlags_NoPreview);
			if (IsItemHovered())
				SetTooltip(_cmds.GetHelp(enum_name(SettingID::RIGHT_RING_MODE)).data());

			TableNextRow();
			TableSetColumnIndex(0);
			drawCombo(*SettingsManager::get<StickMode>(SettingID::RIGHT_STICK_MODE));
			TableSetColumnIndex(1);
			drawLabel(SettingID::RIGHT_RING_MODE);

			StickMode rightStickMode = SettingsManager::get<StickMode>(SettingID::RIGHT_STICK_MODE)->value();
			if (rightStickMode == StickMode::NO_MOUSE || rightStickMode == StickMode::OUTER_RING || rightStickMode == StickMode::INNER_RING)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawButton(ButtonID::RUP);
				TableSetColumnIndex(1);
				drawLabel(ButtonID::RUP);

				TableNextRow();
				TableSetColumnIndex(0);
				drawButton(ButtonID::RLEFT);
				TableSetColumnIndex(1);
				drawLabel(ButtonID::RLEFT);

				TableNextRow();
				TableSetColumnIndex(0);
				drawButton(ButtonID::RRIGHT);
				TableSetColumnIndex(1);
				drawLabel(ButtonID::RRIGHT);

				TableNextRow();
				TableSetColumnIndex(0);
				drawButton(ButtonID::RDOWN);
				TableSetColumnIndex(1);
				drawLabel(ButtonID::RDOWN);
			}
			else if (rightStickMode == StickMode::FLICK || rightStickMode == StickMode::FLICK_ONLY || rightStickMode == StickMode::ROTATE_ONLY)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawCombo(*SettingsManager::get<FlickSnapMode>(SettingID::FLICK_SNAP_MODE));
				TableSetColumnIndex(1);
				drawLabel(SettingID::FLICK_SNAP_MODE);

				TableNextRow();
				TableSetColumnIndex(0);
				drawPercentFloat(SettingID::FLICK_SNAP_STRENGTH);
				TableSetColumnIndex(1);
				drawLabel(SettingID::FLICK_SNAP_STRENGTH);

				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::FLICK_DEADZONE_ANGLE);
				TableSetColumnIndex(1);
				drawLabel(SettingID::FLICK_DEADZONE_ANGLE);

				TableNextRow();
				TableSetColumnIndex(0);
				auto& fsOut = *SettingsManager::get<GyroOutput>(SettingID::FLICK_STICK_OUTPUT);
				drawCombo(fsOut);
				TableSetColumnIndex(1);
				drawLabel(SettingID::FLICK_STICK_OUTPUT);

				if (fsOut == GyroOutput::MOUSE)
				{
					TableNextRow();
					TableSetColumnIndex(0);
					drawAnyFloat(SettingID::FLICK_TIME);
					TableSetColumnIndex(1);
					drawLabel(SettingID::FLICK_TIME);

					TableNextRow();
					TableSetColumnIndex(0);
					drawAnyFloat(SettingID::FLICK_TIME_EXPONENT);
					TableSetColumnIndex(1);
					drawLabel(SettingID::FLICK_TIME_EXPONENT);

					TableNextRow();
					TableSetColumnIndex(0);
					drawAnyFloat(SettingID::REAL_WORLD_CALIBRATION);
					TableSetColumnIndex(1);
					drawLabel(SettingID::REAL_WORLD_CALIBRATION);

					TableNextRow();
					TableSetColumnIndex(0);
					drawAnyFloat(SettingID::IN_GAME_SENS);
					TableSetColumnIndex(1);
					drawLabel(SettingID::IN_GAME_SENS);
				}
				else // LEFT_STICK, RIGHT_STICK, PS_MOTION
				{
					TableNextRow();
					TableSetColumnIndex(0);
					drawAnyFloat(SettingID::VIRTUAL_STICK_CALIBRATION);
					TableSetColumnIndex(1);
					drawLabel(SettingID::VIRTUAL_STICK_CALIBRATION);
				}
			}
			else if (rightStickMode == StickMode::AIM)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::STICK_POWER);
				TableSetColumnIndex(1);
				drawLabel(SettingID::STICK_POWER);

				TableNextRow();
				TableSetColumnIndex(0);
				drawAny2Floats(SettingID::STICK_SENS);
				TableSetColumnIndex(1);
				drawLabel(SettingID::STICK_SENS);

				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::REAL_WORLD_CALIBRATION);
				TableSetColumnIndex(1);
				drawLabel(SettingID::REAL_WORLD_CALIBRATION);

				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::IN_GAME_SENS);
				TableSetColumnIndex(1);
				drawLabel(SettingID::IN_GAME_SENS);

				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::STICK_ACCELERATION_RATE);
				TableSetColumnIndex(1);
				drawLabel(SettingID::STICK_ACCELERATION_RATE);

				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::STICK_ACCELERATION_CAP);
				TableSetColumnIndex(1);
				drawLabel(SettingID::STICK_ACCELERATION_CAP);
			}
			else if (rightStickMode == StickMode::MOUSE_AREA)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::MOUSE_RING_RADIUS);
				TableSetColumnIndex(1);
				drawLabel(SettingID::MOUSE_RING_RADIUS);
			}
			else if (rightStickMode == StickMode::MOUSE_RING)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::MOUSE_RING_RADIUS);
				TableSetColumnIndex(1);
				drawLabel(SettingID::MOUSE_RING_RADIUS);

				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::SCREEN_RESOLUTION_X);
				TableSetColumnIndex(1);
				drawLabel(SettingID::SCREEN_RESOLUTION_X);

				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::SCREEN_RESOLUTION_X);
				TableSetColumnIndex(1);
				drawLabel(SettingID::SCREEN_RESOLUTION_X);
			}
			else if (rightStickMode == StickMode::SCROLL_WHEEL)
			{
				TableNextRow();
				TableSetColumnIndex(0);
				drawAnyFloat(SettingID::SCROLL_SENS);
				TableSetColumnIndex(1);
				drawLabel(SettingID::SCROLL_SENS);

				TableNextRow();
				TableSetColumnIndex(0);
				drawButton(ButtonID::RLEFT);
				TableSetColumnIndex(1);
				drawLabel(ButtonID::RLEFT);

				TableNextRow();
				TableSetColumnIndex(0);
				drawButton(ButtonID::RRIGHT);
				TableSetColumnIndex(1);
				drawLabel(ButtonID::RRIGHT);
			}
			// Handle Controller ones
			else
			{
				Text("%s, Lorem ipsum dolor sit amet, consectetur adipiscing elit...", enum_name(rightStickMode).data());
			}
			EndTable();
		}
		EndChild();

		if (_showPopup != ButtonID::INVALID)
		{
			_inputSelector.show(_showPopup);
			_showPopup = ButtonID::INVALID;
		}
		_inputSelector.draw();
		ImGui::EndTabItem();
	}
}