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
template<typename T>
class JSMVariable;

class JslWrapper;

class AppIf
{
public:
	virtual ~AppIf() = default;

	virtual void createChord(ButtonID chord) = 0;
};

class Application : protected AppIf
{
public:
	Application(JslWrapper *jsl);

	~Application() override = default;

	void init();

	void cleanUp();

	void draw();

protected:
	void createChord(ButtonID chord) override;


private:
	static void HelpMarker(string_view cmd);

	template<typename T>
	static void drawCombo(SettingID stg, ImGuiComboFlags flags = ImGuiComboFlags_NoArrowButton, bool labeled = false);

	struct BindingTab
	{
		BindingTab(string_view name, JslWrapper *jsl, ButtonID chord = ButtonID::NONE);
		void draw(ImVec2 &renderingAreaPos, ImVec2 &renderingAreaSize, bool setFocus = false);
		void drawButton(ButtonID btn, ImVec2 size = ImVec2{ 0, 0 });
		void drawLabel(ButtonID btn);
		void drawLabel(SettingID stg);
		void drawLabel(string_view cmd);
		void drawAnyFloat(SettingID stg, bool labeled = false);
		void drawPercentFloat(SettingID stg, bool labeled = false);
		void drawAny2Floats(SettingID stg, bool labeled = false);
		string _name;
		const ButtonID _chord;
		static InputSelector _inputSelector;
		ButtonID _showPopup = ButtonID::INVALID;
		SettingID _stickConfigPopup = SettingID::INVALID;
		JslWrapper *_jsl;
		static AppIf * _app;
	};
	ButtonID newTab = ButtonID::NONE;
	vector<BindingTab> _tabs;
	bool show_demo_window = false;
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	future<bool> threadDone;
	SDL_Texture *texture = nullptr;
	SDL_Surface *image = nullptr;
	JslWrapper *_jsl;
};