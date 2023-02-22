#pragma once

#include "imgui.h"
#include "SDL.h"
#include "SDL_syswm.h"
#include <functional>
#include <future>
#include <atomic>
#include <optional>
#include "Mapping.h"
#include "InputSelector.h"

enum class ButtonID;
class CmdRegistry;

class Application
{
public:
	Application(const CmdRegistry &cmds);

	~Application() = default;

	void run();

	void StopLoop();

	bool DrawLoop();


private:

	struct BindingTab
	{
		BindingTab(string_view name, const CmdRegistry &cmds, ButtonID chord = ButtonID::NONE);
		void draw(ImVec2 &renderingAreaPos, ImVec2 &renderingAreaSize);
		void drawButton(ButtonID btn, ImVec2 size = ImVec2{ 0, 0 });
		const string_view _name;
		const ButtonID _chord;
		static InputSelector _inputSelector;
		const CmdRegistry &_cmds;
		ButtonID _showPopup = ButtonID::INVALID;
	};
	vector<BindingTab> _tabs;
	const CmdRegistry &_cmds;
	atomic_bool done = false;
	bool show_demo_window = false;
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	future<bool> threadDone;
};