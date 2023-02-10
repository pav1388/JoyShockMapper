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

	void drawButton(ButtonID btn, ImVec2 size = ImVec2{ 0, 0 }, bool enabled = true);

private:
	const CmdRegistry &_cmds;
	InputSelector _inputSelector;
	std::atomic_bool done = false;
	bool show_demo_window = false;
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::future<bool> threadDone;

};