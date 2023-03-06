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

ButtonID operator+(ButtonID lhs, int rhs)
{
	auto newEnum = enum_cast<ButtonID>(enum_integer(lhs) + rhs);
	return newEnum ? *newEnum : ButtonID::INVALID;
}

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
	_tabs.emplace_back("Base Layer", _cmds);
}

std::string_view getButtonLabel(ButtonID id)
{
	return mappings[enum_integer(id)].label();
}

template<typename T>
void drawCombo(JSMVariable<T>& variable, ImGuiComboFlags flags = 0, string_view label = "")
{
	stringstream name;
	if (!label.empty())
		name << label;
	name << "##" << &variable; // id
	if (BeginCombo(name.str().c_str(), enum_name(variable.value()).data(), flags))
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
				auto setting = dynamic_cast<JSMSetting<StickMode>*>(&variable);
				if (setting && setting->_id != SettingID::MOTION_STICK_MODE &&
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
			if constexpr (is_same_v<T, GyroOutput>)
			{
				auto virtual_controller = SettingsManager::getV<ControllerScheme>(SettingID::VIRTUAL_CONTROLLER)->value();
				if (enumVal == GyroOutput::PS_MOTION)
				{
					disabled = virtual_controller == ControllerScheme::DS4;
				}
			}
			if (disabled)
				BeginDisabled();
			if (Selectable(enumStr.data(), enumVal == variable.value()))
			{
				variable.set(enumVal); // set global variable
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (enumVal == variable.value())
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
		if (MenuItem("Set Double Press", "", false, false))
		{
			auto simMap = mappings[enum_integer(btn)].atSimPress(btn);
			// TODO: set simMap to some value using the input selector and create a display for it somewhere in the UI
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
						// TODO: set simMap to some value using the input selector and create a display for it somewhere in the UI
					}
				}
			}
			EndMenu();
		}
		drawAnyFloat(SettingID::HOLD_PRESS_TIME, true);
		if (IsItemHovered())
		{
			SetTooltip(_cmds.GetHelp(enum_name(SettingID::HOLD_PRESS_TIME)).data());
		}
		drawAnyFloat(SettingID::TURBO_PERIOD, true);
		if (IsItemHovered())
		{
			SetTooltip(_cmds.GetHelp(enum_name(SettingID::TURBO_PERIOD)).data());
		}
		drawAnyFloat(SettingID::DBL_PRESS_WINDOW, true);
		if (IsItemHovered())
		{
			SetTooltip(_cmds.GetHelp(enum_name(SettingID::DBL_PRESS_WINDOW)).data());
		}
		auto setting = SettingsManager::getV<float>(SettingID::SIM_PRESS_WINDOW);
		auto value = setting->value();
		if (InputFloat(enum_name(SettingID::SIM_PRESS_WINDOW).data(), &value, 0.f, 0.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
		{
			setting->set(value);
		}
		if (IsItemHovered())
		{
			SetTooltip(_cmds.GetHelp(enum_name(SettingID::SIM_PRESS_WINDOW)).data());
		}

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

	// int (SDLCALL * SDL_EventFilter) (void *userdata, SDL_Event * event);
	SDL_SetEventFilter([](void* userdata, SDL_Event* evt) -> int
	  { return evt->type >= SDL_JOYAXISMOTION && evt->type <= SDL_CONTROLLERSENSORUPDATE ? FALSE : TRUE; },
	  nullptr);
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
				if (MenuItem("On Startup"))
				{
					WriteToConsole("OnStartup.txt");
				}
				if (MenuItem("On Reset"))
				{
					WriteToConsole("OnReset.txt");
				}
				if (BeginMenu("Templates"))
				{
					string gyroConfigsFolder{ GYRO_CONFIGS_FOLDER() };
					for (auto file : ListDirectory(gyroConfigsFolder.c_str()))
					{
						string fullPathName = ".\\GyroConfigs\\" + file;
						auto noext = file.substr(0, file.find_last_of('.'));
						if (MenuItem(noext.c_str()))
						{
							WriteToConsole(string(fullPathName.begin(), fullPathName.end()));
							SettingsManager::getV<Switch>(SettingID::AUTOLOAD)->set(Switch::OFF);
						}
					}
					EndMenu();
				}
				if (BeginMenu("AutoLoad"))
				{
					string autoloadFolder{ AUTOLOAD_FOLDER() };
					for (auto file : ListDirectory(autoloadFolder.c_str()))
					{
						string fullPathName = ".\\AutoLoad\\" + file;
						auto noext = file.substr(0, file.find_last_of('.'));
						if (MenuItem(noext.c_str()))
						{
							WriteToConsole(string(fullPathName.begin(), fullPathName.end()));
							SettingsManager::getV<Switch>(SettingID::AUTOLOAD)->set(Switch::OFF);
						}
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
				static bool mergeJoycons = true;
				if (MenuItem("Reconnect Controllers"))
				{
					if (mergeJoycons)
						WriteToConsole("RECONNECT_CONTROLLERS MERGE");
					else
						WriteToConsole("RECONNECT_CONTROLLERS SPLIT");
				}
				HelpMarker(_cmds.GetHelp("RECONNECT_CONTROLLERS"));

				Checkbox("Merge Joycons", &mergeJoycons);
				HelpMarker("If checked, complimentary joycons will be consider two parts of a single controller");

				if (MenuItem("Reset Mappings"))
				{
					WriteToConsole("RESET_MAPPINGS");
				}
				HelpMarker(_cmds.GetHelp("RESET_MAPPINGS"));

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

				if (MenuItem("Calculate Real World Calibration"))
				{
					WriteToConsole("CALCULATE_REAL_WORLD_CALIBRATION");
				}
				HelpMarker(_cmds.GetHelp("CALCULATE_REAL_WORLD_CALIBRATION"));

				if (MenuItem("Set Motion Stick Center"))
				{
					WriteToConsole("SET_MOTION_STICK_NEUTRAL");
				}
				HelpMarker(_cmds.GetHelp("SET_MOTION_STICK_NEUTRAL"));

				if (MenuItem("Calibrate adaptive Triggers"))
				{
					WriteToConsole("CALIBRATE_TRIGGERS");
					ShowConsole();
				}
				HelpMarker(_cmds.GetHelp("CALIBRATE_TRIGGERS"));

				Separator();

				static bool whitelistAdd = false;
				if (Checkbox("Add to whitelister application", &whitelistAdd))
				{
					if (whitelistAdd)
						WriteToConsole("WHITELIST_ADD");
					else
						WriteToConsole("WHITELIST_REMOVE");
				}
				HelpMarker(_cmds.GetHelp("WHITELIST_ADD"));

				if (MenuItem("Show whitelister"))
				{
					WriteToConsole("WHITELIST_SHOW");
				}
				HelpMarker(_cmds.GetHelp("WHITELIST_SHOW"));
				EndMenu();
			}
			if (BeginMenu("Settings"))
			{
				auto tickTime = SettingsManager::getV<float>(SettingID::TICK_TIME);
				float tt = tickTime->value();
				if (InputFloat(enum_name(SettingID::TICK_TIME).data(), &tt, 0.f, 0.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
				{
					tickTime->set(tt);
				}
				HelpMarker(_cmds.GetHelp(enum_name(SettingID::TICK_TIME).data()));

				string dir = SettingsManager::getV<PathString>(SettingID::JSM_DIRECTORY)->value();
				dir.resize(256, '\0');
				if (InputText("JSM_DIRECTORY", dir.data(), dir.size(), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					dir.resize(strlen(dir.c_str()));
					SettingsManager::getV<PathString>(SettingID::JSM_DIRECTORY)->set(dir);
				}
				HelpMarker(_cmds.GetHelp("JSM_DIRECTORY"));

				drawCombo(*SettingsManager::getV<ControllerScheme>(SettingID::VIRTUAL_CONTROLLER), 0, "VIRTUAL_CONTROLLER");
				HelpMarker(_cmds.GetHelp("VIRTUAL_CONTROLLER"));

				auto autoload = SettingsManager::getV<Switch>(SettingID::AUTOLOAD);
				bool al = autoload->value() == Switch::ON;
				if (Checkbox(enum_name(SettingID::AUTOLOAD).data(), &al))
				{
					autoload->set(al ? Switch::ON : Switch::OFF);
				}
				HelpMarker(_cmds.GetHelp(enum_name(SettingID::AUTOLOAD).data()));

				float rwc = *SettingsManager::get<float>(SettingID::REAL_WORLD_CALIBRATION);
				if (InputFloat("REAL_WORLD_CALIBRATION", &rwc, 0.f, 0.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
				{
					SettingsManager::get<float>(SettingID::REAL_WORLD_CALIBRATION)->set(rwc);
				}
				HelpMarker(_cmds.GetHelp(enum_name(SettingID::REAL_WORLD_CALIBRATION).data()));

				float igs = *SettingsManager::get<float>(SettingID::IN_GAME_SENS);
				if (InputFloat("IN_GAME_SENS", &rwc, 0.f, 0.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
				{
					SettingsManager::get<float>(SettingID::IN_GAME_SENS)->set(igs);
				}
				HelpMarker(_cmds.GetHelp(enum_name(SettingID::IN_GAME_SENS).data()));

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
		for (auto& bindingTab : _tabs)
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

void Application::BindingTab::drawAnyFloat(SettingID stg, bool labeled)
{
	auto setting = SettingsManager::get<float>(stg);
	auto value = setting->value();
	stringstream ss;
	if (!labeled)
		ss << "##";
	ss << enum_name(stg);
	if (InputFloat(ss.str().data(), &value, 0.f, 0.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
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
			TableNextColumn();
			drawLabel(ButtonID::ZLF);
			TableNextColumn();
			bool disabled = SettingsManager::get<TriggerMode>(SettingID::ZL_MODE)->value() == TriggerMode::NO_FULL;
			if (disabled)
				BeginDisabled();
			drawButton(ButtonID::ZLF);
			if (disabled)
				EndDisabled();

			TableNextRow();
			TableNextColumn();
			drawLabel(SettingID::ZL_MODE);
			TableNextColumn();
			drawCombo(*SettingsManager::get<TriggerMode>(SettingID::ZL_MODE));

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::ZL);
			TableNextColumn();
			drawButton(ButtonID::ZL);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::L);
			TableNextColumn();
			drawButton(ButtonID::L);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::UP);
			TableNextColumn();
			drawButton(ButtonID::UP);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::LEFT);
			TableNextColumn();
			drawButton(ButtonID::LEFT);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::RIGHT);
			TableNextColumn();
			drawButton(ButtonID::RIGHT);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::DOWN);
			TableNextColumn();
			drawButton(ButtonID::DOWN);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::LSL);
			TableNextColumn();
			drawButton(ButtonID::LSL);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::LSR);
			TableNextColumn();
			drawButton(ButtonID::LSR);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::L3);
			TableNextColumn();
			drawButton(ButtonID::L3);

			TableNextRow();
			TableNextColumn();
			drawLabel(ButtonID::LRING);
			SameLine();
			drawCombo(*SettingsManager::get<RingMode>(SettingID::LEFT_RING_MODE), ImGuiComboFlags_NoPreview);
			if (IsItemHovered())
				SetTooltip(_cmds.GetHelp(enum_name(SettingID::LEFT_RING_MODE)).data());
			TableNextColumn();
			drawButton(ButtonID::LRING);

			TableNextRow();
			TableNextColumn();
			drawLabel(SettingID::LEFT_STICK_MODE);
			TableNextColumn();
			drawCombo(*SettingsManager::get<StickMode>(SettingID::LEFT_STICK_MODE));
			SameLine();
			if (Button("..."))
				_stickConfigPopup = SettingID::LEFT_STICK_MODE;
			if (IsItemHovered())
				SetTooltip("More left stick settings");

			StickMode leftStickMode = SettingsManager::get<StickMode>(SettingID::LEFT_STICK_MODE)->value();
			if (leftStickMode == StickMode::NO_MOUSE || leftStickMode == StickMode::OUTER_RING || leftStickMode == StickMode::INNER_RING)
			{
				TableNextRow();
				TableNextColumn();
				drawLabel(ButtonID::LUP);
				TableNextColumn();
				drawButton(ButtonID::LUP);

				TableNextRow();
				TableNextColumn();
				drawLabel(ButtonID::LLEFT);
				TableNextColumn();
				drawButton(ButtonID::LLEFT);

				TableNextRow();
				TableNextColumn();
				drawLabel(ButtonID::LRIGHT);
				TableNextColumn();
				drawButton(ButtonID::LRIGHT);

				TableNextRow();
				TableNextColumn();
				drawLabel(ButtonID::LDOWN);
				TableNextColumn();
				drawButton(ButtonID::LDOWN);
			}
			else if (leftStickMode == StickMode::AIM)
			{
				TableNextRow();
				TableNextColumn();
				drawLabel(SettingID::STICK_SENS);
				TableNextColumn();
				drawAny2Floats(SettingID::STICK_SENS);
			}
			else if (leftStickMode == StickMode::FLICK || leftStickMode == StickMode::FLICK_ONLY || leftStickMode == StickMode::ROTATE_ONLY)
			{
				TableNextRow();
				TableNextColumn();
				drawLabel(SettingID::FLICK_STICK_OUTPUT);
				TableNextColumn();
				drawCombo(*SettingsManager::get<GyroOutput>(SettingID::FLICK_STICK_OUTPUT));
			}
			else if (leftStickMode == StickMode::MOUSE_AREA || leftStickMode == StickMode::MOUSE_RING)
			{
				TableNextRow();
				TableNextColumn();
				drawLabel(SettingID::MOUSE_RING_RADIUS);
				TableNextColumn();
				drawAnyFloat(SettingID::MOUSE_RING_RADIUS);
			}
			else if (leftStickMode == StickMode::SCROLL_WHEEL)
			{
				TableNextRow();
				TableNextColumn();
				drawLabel(ButtonID::LLEFT);
				TableNextColumn();
				drawButton(ButtonID::LLEFT);

				TableNextRow();
				TableNextColumn();
				drawLabel(ButtonID::LRIGHT);
				TableNextColumn();
				drawButton(ButtonID::LRIGHT);
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
			TableNextColumn();
			drawLabel("-");
			TableNextColumn();
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
			TableNextColumn();
			drawButton(ButtonID::MINUS);
			TableNextColumn();
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
			TableNextColumn();
			bool disabled = SettingsManager::get<TriggerMode>(SettingID::ZL_MODE)->value() == TriggerMode::NO_FULL;
			if (disabled)
				BeginDisabled();
			drawButton(ButtonID::ZRF);
			if (disabled)
				EndDisabled();
			TableNextColumn();
			drawLabel(ButtonID::ZRF);

			TableNextRow();
			TableNextColumn();
			drawCombo(*SettingsManager::get<TriggerMode>(SettingID::ZR_MODE));
			TableNextColumn();
			drawLabel(SettingID::ZR_MODE);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::ZR);
			TableNextColumn();
			drawLabel(ButtonID::ZR);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::R);
			TableNextColumn();
			drawLabel(ButtonID::R);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::N);
			TableNextColumn();
			drawLabel(ButtonID::N);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::E);
			TableNextColumn();
			drawLabel(ButtonID::E);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::W);
			TableNextColumn();
			drawLabel(ButtonID::W);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::S);
			TableNextColumn();
			drawLabel(ButtonID::S);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::RSR);
			TableNextColumn();
			drawLabel(ButtonID::RSR);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::RSL);
			TableNextColumn();
			drawLabel(ButtonID::RSL);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::R3);
			TableNextColumn();
			drawLabel(ButtonID::R3);

			TableNextRow();
			TableNextColumn();
			drawButton(ButtonID::RRING);
			TableNextColumn();
			drawLabel(ButtonID::RRING);
			SameLine();
			drawCombo(*SettingsManager::get<RingMode>(SettingID::RIGHT_RING_MODE), ImGuiComboFlags_NoPreview);
			if (IsItemHovered())
				SetTooltip(_cmds.GetHelp(enum_name(SettingID::RIGHT_RING_MODE)).data());

			TableNextRow();
			TableNextColumn();
			drawCombo(*SettingsManager::get<StickMode>(SettingID::RIGHT_STICK_MODE));
			SameLine();
			if (Button("..."))
				_stickConfigPopup = SettingID::RIGHT_STICK_MODE;
			if (IsItemHovered())
				SetTooltip("More right stick settings");
			TableNextColumn();
			drawLabel(SettingID::RIGHT_RING_MODE);

			StickMode rightStickMode = SettingsManager::get<StickMode>(SettingID::RIGHT_STICK_MODE)->value();
			if (rightStickMode == StickMode::NO_MOUSE || rightStickMode == StickMode::OUTER_RING || rightStickMode == StickMode::INNER_RING)
			{
				TableNextRow();
				TableNextColumn();
				drawButton(ButtonID::RUP);
				TableNextColumn();
				drawLabel(ButtonID::RUP);

				TableNextRow();
				TableNextColumn();
				drawButton(ButtonID::RLEFT);
				TableNextColumn();
				drawLabel(ButtonID::RLEFT);

				TableNextRow();
				TableNextColumn();
				drawButton(ButtonID::RRIGHT);
				TableNextColumn();
				drawLabel(ButtonID::RRIGHT);

				TableNextRow();
				TableNextColumn();
				drawButton(ButtonID::RDOWN);
				TableNextColumn();
				drawLabel(ButtonID::RDOWN);
			}
			else if (rightStickMode == StickMode::AIM)
			{
				TableNextRow();
				TableNextColumn();
				drawAny2Floats(SettingID::STICK_SENS);
				TableNextColumn();
				drawLabel(SettingID::STICK_SENS);
			}
			else if (rightStickMode == StickMode::FLICK || rightStickMode == StickMode::FLICK_ONLY || rightStickMode == StickMode::ROTATE_ONLY)
			{
				TableNextRow();
				TableNextColumn();
				drawCombo(*SettingsManager::get<GyroOutput>(SettingID::FLICK_STICK_OUTPUT));
				TableNextColumn();
				drawLabel(SettingID::FLICK_STICK_OUTPUT);
			}
			else if (rightStickMode == StickMode::MOUSE_AREA || rightStickMode == StickMode::MOUSE_RING)
			{
				TableNextRow();
				TableNextColumn();
				drawAnyFloat(SettingID::MOUSE_RING_RADIUS);
				TableNextColumn();
				drawLabel(SettingID::MOUSE_RING_RADIUS);
			}
			else if (rightStickMode == StickMode::SCROLL_WHEEL)
			{
				TableNextRow();
				TableNextColumn();
				drawButton(ButtonID::RLEFT);
				TableNextColumn();
				drawLabel(ButtonID::RLEFT);

				TableNextRow();
				TableNextColumn();
				drawButton(ButtonID::RRIGHT);
				TableNextColumn();
				drawLabel(ButtonID::RRIGHT);
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
		if (ImGui::BeginPopup("StickConfig"))
		{
			if (_stickConfigPopup == SettingID::RIGHT_STICK_MODE)
			{
				drawLabel(SettingID::RIGHT_STICK_DEADZONE_INNER);
				SameLine();
				drawPercentFloat(SettingID::RIGHT_STICK_DEADZONE_INNER);

				drawLabel(SettingID::RIGHT_STICK_DEADZONE_OUTER);
				SameLine();
				drawPercentFloat(SettingID::RIGHT_STICK_DEADZONE_OUTER);
			}
			else if (_stickConfigPopup == SettingID::LEFT_STICK_MODE)
			{
				drawLabel(SettingID::LEFT_STICK_DEADZONE_INNER);
				SameLine();
				drawPercentFloat(SettingID::LEFT_STICK_DEADZONE_INNER);

				drawLabel(SettingID::LEFT_STICK_DEADZONE_OUTER);
				SameLine();
				drawPercentFloat(SettingID::LEFT_STICK_DEADZONE_OUTER);
			}

			auto stickMode = SettingsManager::get<StickMode>(_stickConfigPopup)->value();
			if (stickMode == StickMode::FLICK || stickMode == StickMode::FLICK_ONLY || stickMode == StickMode::ROTATE_ONLY)
			{
				drawLabel(SettingID::FLICK_SNAP_MODE);
				SameLine();
				drawCombo(*SettingsManager::get<FlickSnapMode>(SettingID::FLICK_SNAP_MODE));

				drawLabel(SettingID::FLICK_SNAP_STRENGTH);
				SameLine();
				drawPercentFloat(SettingID::FLICK_SNAP_STRENGTH);

				drawLabel(SettingID::FLICK_DEADZONE_ANGLE);
				SameLine();
				drawAnyFloat(SettingID::FLICK_DEADZONE_ANGLE);

				auto& fsOut = *SettingsManager::get<GyroOutput>(SettingID::FLICK_STICK_OUTPUT);
				if (fsOut == GyroOutput::MOUSE)
				{
					drawLabel(SettingID::FLICK_TIME);
					SameLine();
					drawAnyFloat(SettingID::FLICK_TIME);

					drawLabel(SettingID::FLICK_TIME_EXPONENT);
					SameLine();
					drawAnyFloat(SettingID::FLICK_TIME_EXPONENT);
				}
				else // LEFT_STICK, RIGHT_STICK, PS_MOTION
				{
					drawLabel(SettingID::VIRTUAL_STICK_CALIBRATION);
					SameLine();
					drawAnyFloat(SettingID::VIRTUAL_STICK_CALIBRATION);
				}
			}
			else if (stickMode == StickMode::AIM)
			{
				drawLabel(SettingID::STICK_POWER);
				SameLine();
				drawAnyFloat(SettingID::STICK_POWER);

				drawLabel(SettingID::STICK_ACCELERATION_RATE);
				SameLine();
				drawAnyFloat(SettingID::STICK_ACCELERATION_RATE);

				drawLabel(SettingID::STICK_ACCELERATION_CAP);
				SameLine();
				drawAnyFloat(SettingID::STICK_ACCELERATION_CAP);
			}

			else if (stickMode == StickMode::MOUSE_RING)
			{
				drawLabel(SettingID::SCREEN_RESOLUTION_X);
				SameLine();
				drawAnyFloat(SettingID::SCREEN_RESOLUTION_X);

				drawLabel(SettingID::SCREEN_RESOLUTION_Y);
				SameLine();
				drawAnyFloat(SettingID::SCREEN_RESOLUTION_Y);
			}
			else if (stickMode == StickMode::SCROLL_WHEEL)
			{
				drawLabel(SettingID::SCROLL_SENS);
				SameLine();
				drawAny2Floats(SettingID::SCROLL_SENS);
			}
			else if (stickMode == StickMode::LEFT_STICK)
			{
				drawLabel(SettingID::LEFT_STICK_UNDEADZONE_INNER);
				SameLine();
				drawAnyFloat(SettingID::LEFT_STICK_UNDEADZONE_INNER);

				drawLabel(SettingID::LEFT_STICK_UNDEADZONE_OUTER);
				SameLine();
				drawAnyFloat(SettingID::LEFT_STICK_UNDEADZONE_OUTER);

				drawLabel(SettingID::VIRTUAL_STICK_CALIBRATION);
				SameLine();
				drawAnyFloat(SettingID::VIRTUAL_STICK_CALIBRATION);
			}
			else if (stickMode == StickMode::RIGHT_STICK)
			{
				drawLabel(SettingID::RIGHT_STICK_UNDEADZONE_INNER);
				SameLine();
				drawAnyFloat(SettingID::RIGHT_STICK_UNDEADZONE_INNER);

				drawLabel(SettingID::RIGHT_STICK_UNDEADZONE_OUTER);
				SameLine();
				drawAnyFloat(SettingID::RIGHT_STICK_UNDEADZONE_OUTER);

				drawLabel(SettingID::VIRTUAL_STICK_CALIBRATION);
				SameLine();
				drawAnyFloat(SettingID::VIRTUAL_STICK_CALIBRATION);
			}
			else if (stickMode >= StickMode::LEFT_ANGLE_TO_X && stickMode <= StickMode::RIGHT_ANGLE_TO_Y)
			{
				drawLabel(SettingID::ANGLE_TO_AXIS_DEADZONE_INNER);
				SameLine();
				drawAnyFloat(SettingID::ANGLE_TO_AXIS_DEADZONE_INNER);

				drawLabel(SettingID::ANGLE_TO_AXIS_DEADZONE_OUTER);
				SameLine();
				drawAnyFloat(SettingID::ANGLE_TO_AXIS_DEADZONE_OUTER);

				if (stickMode == StickMode::LEFT_ANGLE_TO_X || stickMode == StickMode::LEFT_ANGLE_TO_Y) // isLeft
				{
					drawLabel(SettingID::LEFT_STICK_UNDEADZONE_INNER);
					SameLine();
					drawAnyFloat(SettingID::LEFT_STICK_UNDEADZONE_INNER);

					drawLabel(SettingID::LEFT_STICK_UNDEADZONE_OUTER);
					SameLine();
					drawAnyFloat(SettingID::LEFT_STICK_UNDEADZONE_OUTER);

					drawLabel(SettingID::LEFT_STICK_UNPOWER);
					SameLine();
					drawAnyFloat(SettingID::LEFT_STICK_UNPOWER);
				}
				else // isRight!
				{
					drawLabel(SettingID::RIGHT_STICK_UNDEADZONE_INNER);
					SameLine();
					drawAnyFloat(SettingID::RIGHT_STICK_UNDEADZONE_INNER);

					drawLabel(SettingID::RIGHT_STICK_UNDEADZONE_OUTER);
					SameLine();
					drawAnyFloat(SettingID::RIGHT_STICK_UNDEADZONE_OUTER);

					drawLabel(SettingID::RIGHT_STICK_UNPOWER);
					SameLine();
					drawAnyFloat(SettingID::RIGHT_STICK_UNPOWER);
				}
			}
			else if (stickMode == StickMode::LEFT_WIND_X || stickMode == StickMode::RIGHT_WIND_X)
			{
				drawLabel(SettingID::WIND_STICK_RANGE);
				SameLine();
				drawAnyFloat(SettingID::WIND_STICK_RANGE);

				drawLabel(SettingID::WIND_STICK_POWER);
				SameLine();
				drawAnyFloat(SettingID::WIND_STICK_POWER);

				drawLabel(SettingID::UNWIND_RATE);
				SameLine();
				drawAnyFloat(SettingID::UNWIND_RATE);

				if (stickMode == StickMode::LEFT_WIND_X) // isLeft
				{
					drawLabel(SettingID::LEFT_STICK_UNDEADZONE_INNER);
					SameLine();
					drawAnyFloat(SettingID::LEFT_STICK_UNDEADZONE_INNER);

					drawLabel(SettingID::LEFT_STICK_UNDEADZONE_OUTER);
					SameLine();
					drawAnyFloat(SettingID::LEFT_STICK_UNDEADZONE_OUTER);

					drawLabel(SettingID::LEFT_STICK_UNPOWER);
					SameLine();
					drawAnyFloat(SettingID::LEFT_STICK_UNPOWER);
				}
				else // isRight!
				{
					drawLabel(SettingID::RIGHT_STICK_UNDEADZONE_INNER);
					SameLine();
					drawAnyFloat(SettingID::RIGHT_STICK_UNDEADZONE_INNER);

					drawLabel(SettingID::RIGHT_STICK_UNDEADZONE_OUTER);
					SameLine();
					drawAnyFloat(SettingID::RIGHT_STICK_UNDEADZONE_OUTER);

					drawLabel(SettingID::RIGHT_STICK_UNPOWER);
					SameLine();
					drawAnyFloat(SettingID::RIGHT_STICK_UNPOWER);
				}
			}
			EndPopup();
		}

		if (!IsPopupOpen("StickConfig") && _stickConfigPopup != SettingID::INVALID)
		{
			static bool openOrClear = true;
			if (openOrClear)
				OpenPopup("StickConfig");
			else
				_stickConfigPopup = SettingID::INVALID;
			openOrClear = !openOrClear;
		}
		ImGui::EndTabItem();
	}
}