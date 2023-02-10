#pragma once
#include "JoyShockMapper.h"
#include "Mapping.h"
#include <list>

class InputSelector
{
public:
	InputSelector() = default;
	void show(ButtonID btn);
	void draw();
	bool isShowing() const
	{
		return speedpopup.has_value();
	}

private:
	struct MappingTabItem
	{
		Mapping::ActionModifier act = Mapping::ActionModifier::None;
		Mapping::EventModifier evt = Mapping::EventModifier::Auto;
		KeyCode keyCode = KeyCode("NONE");
		void draw();
	};

	std::optional<ButtonID> speedpopup;
	std::list<MappingTabItem> tabList;
	size_t activeTab = 0;
	char label[64] = "";
};