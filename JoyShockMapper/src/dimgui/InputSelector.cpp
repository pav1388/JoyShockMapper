#include "dimgui/InputSelector.h"
#include "imgui.h"
#include "JSMVariable.hpp"
#include "SettingsManager.h"
#include <regex>
#include <span>
#include <ranges>

using namespace ImGui;
using namespace magic_enum;

enum InputSelector::MappingTabItem::Header InputSelector::MappingTabItem::_activeHeader = InputSelector::MappingTabItem::Header::MODIFIERS;

static array<const char*, enum_integer(ControllerScheme::INVALID)> ctrlrSchemeNames;
static constexpr array<const char*, enum_integer(ControllerScheme::INVALID)>& controllerSchemeNames()
{
	ranges::for_each_n(enum_entries<ControllerScheme>().begin(), ctrlrSchemeNames.size(),
	  [i = 0](auto& pair) mutable
	  {
		  ctrlrSchemeNames[i++] = pair.second.data();
	  });
	return ctrlrSchemeNames;
};

static array<const char*, enum_integer(Mapping::EventModifier::INVALID)> evtModNames;
static constexpr array<const char*, enum_integer(Mapping::EventModifier::INVALID)>& eventModifierNames()
{
	ranges::for_each_n(enum_entries<Mapping::EventModifier>().begin(), evtModNames.size(),
	  [i = 0](auto& pair) mutable
	  {
		  evtModNames[i++] = pair.second.data();
	  });
	return evtModNames;
};

static array<const char*, enum_integer(Mapping::ActionModifier::INVALID)> actModNames;
static constexpr array<const char*, enum_integer(Mapping::ActionModifier::INVALID)>& actModifierNames()
{
	ranges::for_each_n(enum_entries<Mapping::ActionModifier>().begin(), actModNames.size(),
	  [i = 0](auto& pair) mutable
	  {
		  actModNames[i++] = pair.second.data();
	  });
	return actModNames;
}

template<typename T>
void drawCombo(string_view name, T& setting)
{
	if (BeginCombo(name.data(), enum_name(setting).data()))
	{
		for (auto [val, str] : enum_entries<T>())
		{
			if (val == T::INVALID)
				continue;
			if (Selectable(str.data(), val == setting))
			{
				setting = val; // set global variable
			}

			// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
			if (val == setting)
				SetItemDefaultFocus();
		}
		EndCombo();
	}
}

void InputSelector::show(JSMVariable<Mapping> *variable, string_view name)
{
	_variable = variable;
	_name = name;
	stringstream ss;
	ss << "Pick an input for " << name << "##"
	   << "InputSelector";
	OpenPopup(ss.str().c_str(), ImGuiPopupFlags_AnyPopup);
	tabList.clear();
	auto currentLabel = variable->label();
	if (!currentLabel.empty())
	{
		strncpy(label, currentLabel.data(), std::min(IM_ARRAYSIZE(label), int(currentLabel.size())));
	}
	else
	{
		label[0] = '\0';
	}
	static constexpr string_view rgx = R"(\s*([!\^]?)((\".*?\")|\w*[0-9A-Z]|\W)([\\\/+'_]?)\s*(.*))";
	smatch results;
	string command(variable->value().command());
	while (regex_match(command, results, regex(rgx.data())) && !results[0].str().empty())
	{
		Mapping::ActionModifier actMod =
		  results[1].str().empty()   ? Mapping::ActionModifier::None :
		  results[1].str()[0] == '!' ? Mapping::ActionModifier::Instant :
		  results[1].str()[0] == '^' ? Mapping::ActionModifier::Toggle :
		                               Mapping::ActionModifier::INVALID;

		KeyCode key(results[2].str());

		Mapping::EventModifier evtMod =
		  results[4].str().empty()    ? Mapping::EventModifier::Auto :
		  results[4].str()[0] == '\\' ? Mapping::EventModifier::StartPress :
		  results[4].str()[0] == '+'  ? Mapping::EventModifier::TurboPress :
		  results[4].str()[0] == '/'  ? Mapping::EventModifier::ReleasePress :
		  results[4].str()[0] == '\'' ? Mapping::EventModifier::TapPress :
		  results[4].str()[0] == '_'  ? Mapping::EventModifier::HoldPress :
		                                Mapping::EventModifier::INVALID;

		tabList.emplace_back(key, evtMod, actMod);

		command = results[5];
	}
}

void InputSelector::draw()
{
	ImVec2 center = GetMainViewport()->GetCenter();
	center.x /= 3.f;
	center.y /= 3.f;
	SetNextWindowPos(center, ImGuiCond_Appearing);
	SetNextWindowBgAlpha(1.f);
	stringstream ss;
	ss << "Pick an input for " << _name << "##"
	   << "InputSelector";
	if (BeginPopupModal(ss.str().c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		Mapping currentMapping;

		size_t count = 0;
		if (tabList.empty())
		{
			currentMapping = Mapping::NO_MAPPING;
		}
		else
		{
			for (auto const& map : tabList)
			{
				Mapping::EventModifier actualEvt = map.evt();
				if (actualEvt == Mapping::EventModifier::Auto)
				{
					actualEvt = count == 0 ? (tabList.size() == 1 ? Mapping::EventModifier::StartPress : Mapping::EventModifier::TapPress) :
											 (count == 1 ? Mapping::EventModifier::HoldPress : Mapping::EventModifier::Auto);
				}

				currentMapping.AddMapping(map.keyCode(), actualEvt, map.act());
				currentMapping.AppendToCommand(map.keyCode(), map.evt(), map.act());
				++count;
			}
		}

		Text(currentMapping.description().data());
		InputTextWithHint("label", "What action does this mapping perform?", label, IM_ARRAYSIZE(label));
		if (BeginTabBar("MyTabBar", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_NoTooltip | ImGuiTabBarFlags_FittingPolicyScroll))
		{
			// Demo Trailing Tabs: click the "+" button to add a new tab (in your app you may want to use a font icon instead of the "+")
			// Note that we submit it before the regular tabs, but because of the ImGuiTabItemFlags_Trailing flag it will always appear at the end.
			if (TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
			{
				Mapping::EventModifier defEvent = tabList.size() >= 3 ? Mapping::EventModifier::StartPress : Mapping::EventModifier::Auto;
				tabList.emplace_back(KeyCode("NONE"), defEvent, Mapping::ActionModifier::None); // Add new tab
			}

			// Submit our regular tabs
			size_t i = 0;
			for (auto tabItem = tabList.begin(); tabItem != tabList.end(); ++i)
			{
				bool open = true;
				stringstream tabtitle;
				if (tabItem->keyCode().code == COMMAND_ACTION)
					tabtitle << '\"' << tabItem->keyCode().name << '\"';
				else
					tabtitle << tabItem->keyCode().name;
				tabtitle << "###"
				         << "MappingTabItem_" << i;
				if (BeginTabItem(tabtitle.str().c_str(), &open, ImGuiTabItemFlags_None))
				{
					tabItem->draw();
					EndTabItem();
				}

				if (!open)
					tabItem = tabList.erase(tabItem);
				else
					tabItem++;
			}

			EndTabBar();
		}
		if (BeginTable("CloseButtons", 3, ImGuiTableFlags_SizingStretchSame))
		{
			TableNextRow();
			TableSetColumnIndex(0);
			if (Button("Save", { -FLT_MIN, 0.f }))
			{
				_variable->set(currentMapping);
				_variable->updateLabel(label);
				_variable = nullptr;
				CloseCurrentPopup();
			}

			TableSetColumnIndex(1);
			if (Button("Clear", { -FLT_MIN, 0.f }))
			{
				_variable->set(Mapping::NO_MAPPING);
				_variable->updateLabel("");
				_variable = nullptr;
				CloseCurrentPopup();
			}
			else if (IsItemHovered())
			{
				SetTooltip(_variable->defaultValue().description().data());
			}
			TableSetColumnIndex(2);

			if (Button("Cancel", { -FLT_MIN, 0.f }))
			{
				_variable = nullptr;
				CloseCurrentPopup();
			}
			else if (IsItemHovered())
			{
				SetTooltip(_variable->value().description().data());
			}
			EndTable();
		}
		EndPopup();
	}
}

template<size_t rows, size_t cols>
void InputSelector::MappingTabItem::drawSelectableGrid(const string_view labelGrid[rows][cols], std::function<ImVec2(string_view)> getSize)
{
	for (auto row : span(labelGrid, rows))
	{
		for (int x = 0; auto k : span(row, cols))
		{
			if (k.empty())
				break;
			if (x > 0)
			{
				SameLine();
			}
			auto size = getSize(k);
			bool disable = k == " ";
			if(disable)
				BeginDisabled();
			if (Selectable(k.data(), _keyCode.name == k, ImGuiSelectableFlags_DontClosePopups, size))
			{
				_keyCode = KeyCode(k);
			}
			if (disable)
				EndDisabled();
			++x;
		}
	}
}

InputSelector::MappingTabItem::MappingTabItem(KeyCode keyCode, Mapping::EventModifier evt, Mapping::ActionModifier act)
  : _keyCode(keyCode)
	, _evt(evt)
	, _act(act)
{

}

void InputSelector::MappingTabItem::draw()
{
	SetNextItemOpen(_activeHeader == MODIFIERS, ImGuiCond_Always);
	if (CollapsingHeader("Modifiers"))
	{
		_activeHeader = MODIFIERS;
		int evt = 0, act = 0;
		if (ListBox("Event Modifiers", &evt, eventModifierNames().data(), eventModifierNames().size(), eventModifierNames().size()))
		{
			_evt = *enum_cast<Mapping::EventModifier>(evt);
		}
		if (ListBox("Action Modifiers", &act, actModifierNames().data(), actModifierNames().size(), actModifierNames().size()))
		{
			_act = *enum_cast<Mapping::ActionModifier>(act);
		}
	}
	SetNextItemOpen(_activeHeader == MOUSE, ImGuiCond_Always);
	if (CollapsingHeader("Mouse"))
	{
		_activeHeader = MOUSE;
		static constexpr size_t MS_ROWS = 3;
		static constexpr size_t MS_COLS = 3;
		static constexpr string_view mouse[MS_ROWS][MS_COLS] = {
			{ "LMOUSE", "MMOUSE", "RMOUSE" },
			{ "SCROLLUP", "SCROLLDOWN", "" },
			{ "FMOUSE", "BMOUSE", "" },
		};
		auto sizing = [](string_view k)
		{
			ImVec2 size(60, 40);
			if (k == "SCROLLDOWN")
			{
				size.x = 80;
			}
			return size;
		};
		drawSelectableGrid<MS_ROWS, MS_COLS>(mouse, sizing);
	}
	SetNextItemOpen(_activeHeader == KEYBOARD, ImGuiCond_Always);
	if (CollapsingHeader("Keyboard"))
	{
		_activeHeader = KEYBOARD;
		static constexpr size_t KB_ROWS = 6;
		static constexpr size_t KB_COLS = 14;
		static constexpr string_view keyboard[KB_ROWS][KB_COLS] = {
			{ "ESC", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "SCROLL_LOCK" },
			{ "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "_", "+", "BACKSPACE" },
			{ "TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\" },
			{ "CAPS_LOCK", "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "'", "ENTER", "" },
			{ "LSHIFT", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "RSHIFT", "UP", "" },
			{ "LCONTROL", "LWINDOWS", "LALT", "SPACE", "RALT", "RWINDOWS", "CONTEXT", "RCONTROL", "LEFT", "DOWN", "RIGHT", "" },
		};

		auto sizing = [](string_view k)
		{
			ImVec2 size(40, 40);
			if (k == "SCROLL_LOCK" || k == "BACKSPACE" || k == "\\" || k == "ENTER" || k == "UP" || k == "RIGHT")
			{
				size.x = 0; // Use remaining space for the last item
			}
			else if (k == "LSHIFT")
			{
				size.x *= 1.8f;
			}
			else if (k.size() >= 6)
			{
				size.x *= 1 + 0.16f * (k.size() - 5);
			}
			else if (k == "SPACE")
			{
				size.x *= 3.33f;
			}
			return size;
		};

		drawSelectableGrid<KB_ROWS, KB_COLS>(keyboard, sizing);
	}
	SetNextItemOpen(_activeHeader == MORE_KEYBOARD, ImGuiCond_Always);
	if (CollapsingHeader("More keyboard"))
	{
		_activeHeader = MORE_KEYBOARD;
		static constexpr size_t XKB_ROWS = 5;
		static constexpr size_t XKB_COLS = 7;
		static constexpr string_view extra_keyboard[XKB_ROWS][XKB_COLS] = {
			{ "INSERT", "HOME", "PAGEUP", "NUM_LOCK", "DIVIDE", "MULTIPLY", "SUBSTRACT" },
			{ "DELETE", "END", "PAGEDOWN", "N7", "N8", "N9", "ADD" },
			{ "MUTE", "PREV_TRACK", "NEXT_TRACK", "N4", "N5", "N6", "" },
			{
			  "VOLUME_UP",
			  "STOP_TRACK",
			  "PLAY_PAUSE",
			  "N1",
			  "N2",
			  "N3",
			  "ENTER",
			},
			{ "VOLUME_DOWN", "SCREENSHOT", "N0", "DECIMAL", "" },
		};

		// static float vdsize = 1.0f, n0size = 1.0f;
		// SliderFloat("vdsize", &vdsize, 0.1f, 3.0f);
		// SliderFloat("sssize", &n0size, 0.1f, 3.0f);

		auto sizing = [](string_view k)
		{
			ImVec2 size(70, 40);
			if (k == "VOLUME_DOWN" || k == "SCREENSHOT")
			{
				size.x *= 1.56f;
			}
			else if (k == "N0")
			{
				size.x *= 2.11f;
			}
			return size;
		};

		drawSelectableGrid<XKB_ROWS, XKB_COLS>(extra_keyboard, sizing);
	}
	SetNextItemOpen(_activeHeader == CONSOLE, ImGuiCond_Always);
	if (CollapsingHeader("Console Command"))
	{
		_activeHeader = CONSOLE;
		static string command(256, '\0');
		InputText("Command", command.data(), command.size(), ImGuiInputTextFlags_None);
		if (IsItemDeactivatedAfterEdit())
		{
			stringstream ss;
			ss << '\"' << command.c_str() << '\"';
			KeyCode commandAction = KeyCode(ss.str());
			Mapping testCommand;
			if (testCommand.AddMapping(commandAction, Mapping::EventModifier::StartPress))
			{
				_keyCode = commandAction; 
			}
		}
	}		
	SetNextItemOpen(_activeHeader == GYRO, ImGuiCond_Always);
	if (CollapsingHeader("Gyro management"))
	{
		_activeHeader = GYRO;
		static constexpr size_t GYRO_ROWS = 3;
		static constexpr size_t GYRO_COLS = 3;
		static constexpr string_view gyro[GYRO_ROWS][GYRO_COLS] = {
			{ "GYRO_ON", "GYRO_OFF", "CALIBRATE" },
			{ "GYRO_INVERT", "GYRO_INV_X", "GYRO_INV_Y" },
			{ "GYRO_TRACKBALL", "GYRO_TRACK_X", "GYRO_TRACK_Y" },
		};
		/*static float scale =  100.f;
		SliderFloat("scale", &scale, 50.f, 500.f);*/
		auto sizing = [](string_view k)
		{
			ImVec2 size(100, 40);
			return size;
		};
		drawSelectableGrid<GYRO_ROWS, GYRO_COLS>(gyro, sizing);
	}
	SetNextItemOpen(_activeHeader == CONTROLLER, ImGuiCond_Always);
	if (CollapsingHeader("Virtual Controller"))
	{
		_activeHeader = CONTROLLER;
		ControllerScheme vc = SettingsManager::getV<ControllerScheme>(SettingID::VIRTUAL_CONTROLLER)->value();
		int currentSelection = *enum_index(vc);
		if (vc == ControllerScheme::NONE)
		{
			if (ListBox("VIRTUAL_CONTROLLER", &currentSelection, controllerSchemeNames().data(), controllerSchemeNames().size(), controllerSchemeNames().size()))
			{
				SettingsManager::getV<ControllerScheme>(SettingID::VIRTUAL_CONTROLLER)->set(enum_value<ControllerScheme>(currentSelection));
			}
		}
		else
		{
			if (Combo("VIRTUAL_CONTROLLER", &currentSelection, controllerSchemeNames().data(), controllerSchemeNames().size()))
			{
				SettingsManager::getV<ControllerScheme>(SettingID::VIRTUAL_CONTROLLER)->set(enum_value<ControllerScheme>(currentSelection));
			}
			if (vc == ControllerScheme::XBOX)
			{
				static constexpr size_t XBOX_ROWS = 5;
				static constexpr size_t XBOX_COLS = 6;
				static constexpr string_view xbox[XBOX_ROWS][XBOX_COLS] = {
					{ "X_LT", "X_LB", "X_GUIDE", " ", "X_RB", "X_RT" },
					{ "X_LS", " ", "X_BACK", "X_START", "X_Y", "" },
					{ " ", "X_UP", " ", "X_X", " ", "X_B" },
					{ "X_LEFT", " ", "X_RIGHT", " ", "X_A", "" },
					{ " ", "X_DOWN", " ", "X_RS", " ", "" },
				};
				//static float scale =  100.f;
				//SliderFloat("scale", &scale, 10.f, 200.f);
				auto sizing = [](string_view k)
				{
					ImVec2 size(75, 40);
					return size;
				};
				drawSelectableGrid<XBOX_ROWS, XBOX_COLS>(xbox, sizing);
			}
			if (vc == ControllerScheme::DS4)
			{
				static constexpr size_t DS4_ROWS = 5;
				static constexpr size_t DS4_COLS = 6;
				static constexpr string_view ds4[DS4_ROWS][DS4_COLS] = {
					{ "PS_L2", "PS_L1", "PS_HOME", "PS_PAD_CLICK", "PS_R1", "PS_R2" },
					{ " ", "PS_UP", "PS_SHARE", "PS_OPTIONS", "PS_TRIANGLE", "" },
					{ "PS_LEFT", " ", "PS_RIGHT", "PS_SQUARE", " ", "PS_CIRCLE" },
					{ " ", "PS_DOWN", " ", " ", "PS_CROSS", "" },
					{ " ", " ", "PS_L3", "PS_R3", " ", "" },
				};
				// static float scale =  100.f;
				// SliderFloat("scale", &scale, 10.f, 200.f);
				auto sizing = [](string_view k)
				{
					ImVec2 size(85, 40);
					return size;
				};
				drawSelectableGrid<DS4_ROWS, DS4_COLS>(ds4, sizing);
			}
		}
	}
}
