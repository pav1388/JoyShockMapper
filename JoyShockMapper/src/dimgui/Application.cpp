#include "dimgui/Application.h"
#include "InputHelpers.h"
#include "JSMVariable.hpp"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer.h"

#define ImTextureID SDL_Texture

extern vector<JSMButton> mappings;
extern JSMSetting<TriggerMode> zlMode;
extern JSMSetting<TriggerMode> zrMode;
extern JSMSetting<TriggerMode> touch_ds_mode;
extern JSMSetting<StickMode> left_stick_mode;
extern JSMSetting<StickMode> right_stick_mode;
extern JSMSetting<StickMode> motion_stick_mode;

static constexpr std::string_view popup = "Pick an input";

Application::Application()
{
}

std::string getButtonDescription(ButtonID id)
{
	std::stringstream ss;
	ss << mappings[magic_enum::enum_integer(id)].get().value().description();
	ss << "###" << magic_enum::enum_name(id);
	return ss.str();
}

std::string_view getButtonLabel(ButtonID id)
{
	return mappings[magic_enum::enum_integer(id)].label();
}

template<typename T>
void drawCombo(JSMSetting<T>& setting)
{
	using namespace magic_enum;
	T selected = *setting.get();
	if (ImGui::BeginCombo(enum_name(setting._id).data(), enum_name(selected).data()))
	{
		for (int n = 0; n < enum_count<T>(); ++n)
		{
			const bool is_selected = (enum_cast<T>(n) == selected);
			if (ImGui::Selectable(enum_names<T>()[n].data(), is_selected))
			{
				setting = *enum_cast<T>(n); // set global variable
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

template<typename T>
void drawCombo(string_view name, T& setting)
{
	using namespace magic_enum;
	if (ImGui::BeginCombo(name.data(), enum_name(setting).data()))
	{
		for (int n = 0; n < enum_count<T>(); ++n)
		{
			auto val = enum_cast<T>(n);
			if (!val || *val == T::INVALID)
				continue;
			if constexpr (std::is_same<T, Mapping::EventModifier>::value)
			{
				if (*val == Mapping::EventModifier::None)
				{
					continue;
				}
			}
			const bool is_selected = (*val == setting);
			if (ImGui::Selectable(enum_names<T>()[n].data(), is_selected))
			{
				setting = *enum_cast<T>(n); // set global variable
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

void Application::drawButton(ButtonID btn)
{
	using namespace ImGui;
	if (Button(getButtonDescription(btn).data()))
	{
		speedpopup = btn;
		OpenPopup(popup.data(), ImGuiPopupFlags_AnyPopup);
	}

	string_view label = getButtonLabel(btn).data();
	if (IsItemHovered() && !label.empty())
	{
		SetTooltip(label.data());
	}
}

void Application::ShowModalInputSelector()
{
	using namespace ImGui;
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(popup.data(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static Mapping::ActionModifier act = Mapping::ActionModifier::None;
		static Mapping::EventModifier evt = Mapping::EventModifier::StartPress;
		static KeyCode keyCode;
		Mapping currentMapping;
		currentMapping.AddMapping(keyCode, evt, act);
		Text(currentMapping.description().data());
		if (ImGui::CollapsingHeader("Modifiers", ImGuiTreeNodeFlags_None))
		{
			drawCombo("Action Modifiers", act);
			drawCombo("Event Modifiers", evt);
		}
		if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_None))
		{
			if (Button("LMOUSE"))
			{
				keyCode = KeyCode("LMOUSE");
			}
			if (Button("RMOUSE"))
			{
				keyCode = KeyCode("RMOUSE");
			}
			if (Button("MMOUSE"))
			{
				keyCode = KeyCode("MMOUSE");
			}
			if (Button("BMOUSE"))
			{
				keyCode = KeyCode("BMOUSE");
			}
			if (Button("MMOUSE"))
			{
				keyCode = KeyCode("MMOUSE");
			}
		}
		if (ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_None))
		{
			ImGui::Text("IsItemHovered: %d", ImGui::IsItemHovered());
			for (int i = 0; i < 5; i++)
				ImGui::Text("Some content %d", i);
		}
		if (ImGui::CollapsingHeader("More keyboard", ImGuiTreeNodeFlags_None))
		{
			ImGui::Text("IsItemHovered: %d", ImGui::IsItemHovered());
			for (int i = 0; i < 5; i++)
				ImGui::Text("Some content %d", i);
		}
		if (ImGui::CollapsingHeader("Gyro management", ImGuiTreeNodeFlags_None))
		{
			ImGui::Text("IsItemHovered: %d", ImGui::IsItemHovered());
			for (int i = 0; i < 5; i++)
				ImGui::Text("Some content %d", i);
		}
		if (ImGui::CollapsingHeader("Virtual Controller", ImGuiTreeNodeFlags_None))
		{
			ImGui::Text("IsItemHovered: %d", ImGui::IsItemHovered());
			for (int i = 0; i < 5; i++)
				ImGui::Text("Some content %d", i);
		}
		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			mappings[magic_enum::enum_integer(*speedpopup)] = currentMapping;
			speedpopup = std::nullopt;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void Application::drawButton(ButtonID btn, ImVec2 size)
{
	using namespace ImGui;
	if (Button(getButtonDescription(btn).data(), size))
	{
		// do something useful
		speedpopup = btn;
		OpenPopup(popup.data(), ImGuiPopupFlags_AnyPopup);
	}

	string_view label = getButtonLabel(btn).data();
	if (ImGui::IsItemHovered() && !label.empty())
	{
		ImGui::SetTooltip(label.data());
	}
}

void Application::Runloop()
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
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer_Init(renderer);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
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

	ImGui::GetStyle().Alpha = 1.0f;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	SDL_Surface* image = SDL_LoadBMP("ds4.bmp");
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, image);

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

		// Start the Dear ImGui frame
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
		// static float f = 0.0f;
		using namespace ImGui;
		static int counter = 0;

		Begin("ButtonBar");
		Checkbox("Show demo window", &show_demo_window);
		SameLine();
		if (ImGui::Button("RECONNECT_CONTROLLERS"))
		{
			WriteToConsole("RECONNECT_CONTROLLERS");
		}
		SameLine();
		static int durationIndex = 0; // Here we store our selection data as an index.
		if (Button("RESTART_GYRO_CALIBRATION"))
		{
			auto t = std::thread([]()
			  {
					WriteToConsole("RESTART_GYRO_CALIBRATION");
					int32_t ms = (durationIndex + 1) * 1000;
					Sleep(ms); // ms
					WriteToConsole("FINISH_GYRO_CALIBRATION"); });
			t.detach();
		}
		static const char* items[] = { "1 second", "2 second", "3 seconds", "4 seconds", "5 seconds" };
		SameLine();
		if (ImGui::BeginCombo("duration", nullptr, ImGuiComboFlags_NoPreview))
		{
			for (int n = 0; n < IM_ARRAYSIZE(items); n++)
			{
				const bool is_selected = (durationIndex == n);
				if (ImGui::Selectable(items[n], is_selected))
					durationIndex = n;

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		SameLine();
		if (Button("Quit")) // Buttons return true when clicked (most widgets return true when edited/activated)
			done = true;
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		End();

		Begin("Ds4");
		DockSpace(GetID("Ds4"));
		bool isDs4Visible = ImGui::IsItemVisible();
		End();

		Begin("Left Bindings");
		ImVec2 buttonSize{ GetWindowSize().x, 50.f };
		ImVec2 dummySize{ buttonSize.x, buttonSize.y / 2.f };
		drawButton(ButtonID::ZLF, buttonSize);
		drawCombo(zlMode);
		drawButton(ButtonID::ZL, buttonSize);
		drawButton(ButtonID::L, buttonSize);
		Dummy(dummySize);
		drawButton(ButtonID::UP, buttonSize);
		drawButton(ButtonID::LEFT, buttonSize);
		drawButton(ButtonID::RIGHT, buttonSize);
		drawButton(ButtonID::DOWN, buttonSize);
		Dummy(dummySize);
		drawButton(ButtonID::L3, buttonSize);
		drawCombo(left_stick_mode);

		ShowModalInputSelector();
		End();

		ImVec2 rightWindowSize;
		Begin("Right Bindings");
		rightWindowSize = GetWindowSize();
		buttonSize = { GetWindowSize().x, 50.f };
		drawButton(ButtonID::ZRF, buttonSize);
		drawCombo(zrMode);
		drawButton(ButtonID::ZR, buttonSize);
		drawButton(ButtonID::R, buttonSize);
		Dummy(dummySize);
		drawButton(ButtonID::N, buttonSize);
		drawButton(ButtonID::E, buttonSize);
		drawButton(ButtonID::W, buttonSize);
		drawButton(ButtonID::S, buttonSize);
		Dummy(dummySize);
		drawButton(ButtonID::R3, buttonSize);
		drawCombo(right_stick_mode);
		ShowModalInputSelector();
		End();

		ImVec2 topWindowPos, topWindowSize;
		{
			Begin("Top buttons");
			topWindowPos = GetWindowPos();
			topWindowSize = GetWindowSize();
			ImVec2 buttonSize{ GetWindowSize().x / 5.f - 10.f, 50.f };
			drawButton(ButtonID::MINUS, buttonSize);
			SameLine();
			drawButton(ButtonID::TOUCH, buttonSize);
			SameLine();
			static const char* items[] = { "NO_FULL", "NO_SKIP", "MAY_SKIP", "MUST_SKIP", "NO_SKIP_EXCLUSIVE", "MAY_SKIP_R", "MUST_SKIP_R", "X_LT", "X_RT" };
			static int zlModeIndex = 0;
			SetNextWindowSizeConstraints(buttonSize, buttonSize);
			if (ImGui::BeginCombo(" ", items[zlModeIndex], ImGuiComboFlags_NoPreview))
			{
				for (int n = 0; n < IM_ARRAYSIZE(items); n++)
				{
					const bool is_selected = (zlModeIndex == n);
					if (ImGui::Selectable(items[n], is_selected))
						zlModeIndex = n; // set global variable

					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			SameLine();
			drawButton(ButtonID::CAPTURE, buttonSize);
			SameLine();
			drawButton(ButtonID::PLUS, buttonSize);
			ShowModalInputSelector();
			End();
		}

		ImVec2 bottomWindowPos, bottomWindowSize;
		{
			Begin("Bottom Buttons");
			bottomWindowPos = GetWindowPos();
			bottomWindowSize = GetWindowSize();
			ImVec2 buttonSize{ GetWindowSize().x / 6.f - 10.f, 50.f };
			drawButton(ButtonID::LUP, buttonSize);
			SameLine();
			drawButton(ButtonID::LRIGHT, buttonSize);
			SameLine();
			drawButton(ButtonID::MUP, buttonSize);
			SameLine();
			drawButton(ButtonID::MRIGHT, buttonSize);
			SameLine();
			drawButton(ButtonID::RUP, buttonSize);
			SameLine();
			drawButton(ButtonID::RRIGHT, buttonSize);
			drawButton(ButtonID::LLEFT, buttonSize);
			SameLine();
			drawButton(ButtonID::LDOWN, buttonSize);
			SameLine();
			drawButton(ButtonID::MLEFT, buttonSize);
			SameLine();
			drawButton(ButtonID::MDOWN, buttonSize);
			SameLine();
			drawButton(ButtonID::RLEFT, buttonSize);
			SameLine();
			drawButton(ButtonID::RDOWN, buttonSize);
			ShowModalInputSelector();
			End();
		}

		SDL_Rect dstrect{
			topWindowPos.x,
			topWindowPos.y + topWindowSize.y,
			topWindowSize.x,
			rightWindowSize.y - bottomWindowSize.y - topWindowSize.y
		};

		if (isDs4Visible)
		{
			ImVec2 upLeft{ float(dstrect.x), float(dstrect.y) };
			ImVec2 lowRight{ float(dstrect.x + dstrect.w), float(dstrect.y + dstrect.h) };
			GetBackgroundDrawList()->AddImage(texture, upLeft, lowRight);
		}

		// Rendering
		ImGui::Render();
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		SDL_SetTextureBlendMode(texture, SDL_BlendMode::SDL_BLENDMODE_ADD);
		if (isDs4Visible)
			SDL_RenderCopy(renderer, texture, nullptr, &dstrect);
		SDL_RenderPresent(renderer);
	}

	// Cleanup
	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyTexture(texture);
	SDL_FreeSurface(image);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	WriteToConsole("QUIT");

	return done;
}