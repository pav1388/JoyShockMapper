#pragma once
#include "JoyShockMapper.h"
#include "Mapping.h"
#include "imgui.h"
#include <list>

template<class T>
class JSMVariable;

class InputSelector
{
public:
	InputSelector() = default;
	void show(JSMVariable<Mapping> *variable, string_view name);
	void draw();
private:
	struct MappingTabItem
	{
		MappingTabItem(KeyCode keyCode, Mapping::EventModifier evt, Mapping::ActionModifier act);
		void draw();
		Mapping::ActionModifier act() const
		{
			return _act;
		}
		Mapping::EventModifier evt() const
		{
			return _evt;
		}
		const KeyCode &keyCode() const
		{
			return _keyCode;
		}
	private:
		template<size_t rows, size_t cols>
		void drawSelectableGrid(const std::string_view labelGrid[rows][cols], std::function<ImVec2(std::string_view)> getSize);

		Mapping::ActionModifier _act = Mapping::ActionModifier::None;
		Mapping::EventModifier _evt = Mapping::EventModifier::Auto;
		KeyCode _keyCode = KeyCode("NONE");
		static enum Header {
			MODIFIERS,
			MOUSE,
			KEYBOARD,
			MORE_KEYBOARD,
			CONSOLE,
			GYRO,
			CONTROLLER,
		} _activeHeader;
	};

	JSMVariable<Mapping> *_variable;
	std::string _name;
	std::list<MappingTabItem> tabList;
	size_t activeTab = 0;
	char label[64] = "";
};