#pragma once

#include "imgui.h"
#include "SDL.h"
#include "SDL_syswm.h"
#include <functional>
#include <future>
#include <atomic>
#include <map>
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

	void draw(SDL_GameController *controller);

protected:
	void createChord(ButtonID chord) override;


private:
	static void HelpMarker(string_view cmd);

	template<typename T>
	static void drawCombo(SettingID stg, ButtonID chord, ImGuiComboFlags flags = ImGuiComboFlags_NoArrowButton, bool labeled = false);

	struct BindingTab
	{
		BindingTab(string_view name, JslWrapper *jsl, ButtonID chord = ButtonID::NONE);
		bool draw(ImVec2 &renderingAreaPos, ImVec2 &renderingAreaSize, bool setFocus = false);

	private:
		void drawButton(ButtonID btn, ImVec2 size = ImVec2{ 0, 0 });
		void drawLabel(ButtonID btn);
		void drawLabel(SettingID stg);
		void drawLabel(string_view cmd);
		void drawAnyFloat(SettingID stg, bool labeled = false);
		void drawPercentFloat(SettingID stg, bool labeled = false);
		void drawAny2Floats(SettingID stg, bool labeled = false);
		template<typename T>
		T getSettingValue(SettingID setting, JSMVariable<T> **outVariable = nullptr);
		
		ButtonID _chord;
	public:
		static InputSelector _inputSelector;
		static AppIf * _app;

		string _name;
		ButtonID _showPopup = ButtonID::INVALID;
		SettingID _stickConfigPopup = SettingID::INVALID;
		JslWrapper *_jsl;

		bool operator<(const BindingTab& rhs)
		{
			return _chord < rhs._chord;
		}
	};
	ButtonID _newTab = ButtonID::NONE;
	map<ButtonID, BindingTab> _tabs;
	bool show_demo_window = false;
	bool show_plot_demo_window = false;
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	future<bool> threadDone;
	JslWrapper *_jsl;
};