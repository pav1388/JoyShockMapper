#pragma once

#include "imgui.h"
#include "SDL.h"
#include "SDL_syswm.h"
#include <functional>
#include <future>
#include <atomic>
#include <optional>

enum class ButtonID;

class Application
{
public:
	Application();

	~Application() = default;

	void Runloop();

	void StopLoop();

	bool DrawLoop();

	void ShowModalInputSelector();

	void drawButton(ButtonID btn);
	void drawButton(ButtonID btn, ImVec2 size);

private:
	// Our state
	std::atomic_bool done = false;
	bool show_demo_window = false;
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::future<bool> threadDone;
	std::optional<ButtonID> speedpopup;
};