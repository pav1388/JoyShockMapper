#include "dimgui/InputSelector.h" 
#include "imgui.h"
#include "JSMVariable.hpp"
#include <regex>
#include <span>

using namespace ImGui;
using namespace magic_enum;
extern vector<JSMButton> mappings;
static constexpr std::string_view popup = "Pick an input";

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

void InputSelector::show(ButtonID btn)
{
	speedpopup = btn;
	OpenPopup(popup.data(), ImGuiPopupFlags_AnyPopup);
	tabList.clear();
	auto currentLabel = mappings[enum_integer(btn)].label();
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
	string command(mappings[enum_integer(btn)].value().command());
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

		tabList.push_back(MappingTabItem{
		  .act = actMod,
		  .evt = evtMod,
		  .keyCode = key,
		});

		command = results[5];
	}
}

void InputSelector::draw()
{
	ImVec2 center = GetMainViewport()->GetCenter();
	center.x /= 3.f;
	center.y /= 3.f;
	SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImVec2 dims = GetWindowSize();
	dims.x /= 2.f;
	dims.y /= 2.f;
	SetNextWindowSize(dims, ImGuiCond_Appearing);
	SetNextWindowBgAlpha(1.0f);
	if (BeginPopupModal(popup.data(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		Mapping currentMapping;

		size_t count = 0;
		for (auto const& map : tabList)
		{
			Mapping::EventModifier actualEvt = map.evt;
			if (actualEvt == Mapping::EventModifier::Auto)
			{
				actualEvt = count == 0 ? (tabList.size() == 1 ? Mapping::EventModifier::StartPress : Mapping::EventModifier::TapPress) :
				                         (count == 1 ? Mapping::EventModifier::HoldPress : Mapping::EventModifier::Auto);
			}

			currentMapping.AddMapping(map.keyCode, actualEvt, map.act);
			currentMapping.AppendToCommand(map.keyCode, map.evt, map.act);
			++count;
		}

		Text(currentMapping.description().data());
		InputTextWithHint("label", "What action does this mapping perform?", label, IM_ARRAYSIZE(label));
		if (BeginTabBar("MyTabBar", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_NoTooltip | ImGuiTabBarFlags_FittingPolicyScroll))
		{
			// Demo Trailing Tabs: click the "+" button to add a new tab (in your app you may want to use a font icon instead of the "+")
			// Note that we submit it before the regular tabs, but because of the ImGuiTabItemFlags_Trailing flag it will always appear at the end.
			if (TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
			{
				tabList.push_back(MappingTabItem()); // Add new tab
				if (tabList.size() >= 3)
				{
					tabList.back().evt = Mapping::EventModifier::StartPress;
				}
			}

			// Submit our regular tabs
			size_t i = 0;
			for (auto tabItem = tabList.begin(); tabItem != tabList.end(); ++i)
			{
				bool open = true;
				stringstream tabtitle;
				tabtitle << tabItem->keyCode.name << "###"
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
			if (Button("Cancel", { -FLT_MIN , 0.f}))
			{
				speedpopup = std::nullopt;
				CloseCurrentPopup();
			}
			else if (IsItemHovered())
			{
				SetTooltip(mappings[enum_integer(*speedpopup)].value().description().data());
			}

			TableSetColumnIndex(1);
			Dummy({ -FLT_MIN , 0.f});
			TableSetColumnIndex(2);
			if (Button("Save", { -FLT_MIN , 0.f}))
			{
				mappings[enum_integer(*speedpopup)].set(currentMapping);
				mappings[enum_integer(*speedpopup)].updateLabel(label);
				speedpopup = std::nullopt;
				CloseCurrentPopup();
			}
			EndTable();
		}
		EndPopup();
	}
}

template<size_t rows, size_t cols>
void drawSelectableGrid(InputSelector::MappingTabItem *tab, const string_view labelGrid[rows][cols], std::function<ImVec2(string_view)> getSize)
{
	// static float scale = 1.f;
	// SliderFloat("scale", &scale, 0.1f, 3.f);
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

			if (Selectable(k.data(), tab->keyCode.name == k, ImGuiSelectableFlags_DontClosePopups, size))
			{
				tab->keyCode = KeyCode(k);
			}
			++x;
		}
	}
}


void InputSelector::MappingTabItem::draw()
{
	if (CollapsingHeader("Modifiers", ImGuiTreeNodeFlags_None))
	{
		drawCombo("Action Modifiers", act);
		drawCombo("Event Modifiers", evt);
	}
	if (CollapsingHeader("Mouse", ImGuiTreeNodeFlags_None))
	{
		static constexpr size_t MS_ROWS = 3;
		static constexpr size_t MS_COLS = 3;
		static constexpr string_view mouse[MS_ROWS][MS_COLS] = {
			{ "LMOUSE", "SCROLLUP", "RMOUSE" },
			{ "FMOUSE", "MMOUSE", "" },
			{ "BMOUSE", "SCROLLDOWN", "" },
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
		drawSelectableGrid<MS_ROWS, MS_COLS>(this, mouse, sizing);
	}
	if (CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_None))
	{
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

		drawSelectableGrid<KB_ROWS, KB_COLS>(this, keyboard, sizing);
	}
	if (CollapsingHeader("More keyboard", ImGuiTreeNodeFlags_None))
	{
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

		//static float vdsize = 1.0f, n0size = 1.0f;
		//SliderFloat("vdsize", &vdsize, 0.1f, 3.0f);
		//SliderFloat("sssize", &n0size, 0.1f, 3.0f);

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

		drawSelectableGrid<XKB_ROWS, XKB_COLS>(this, extra_keyboard, sizing);
	}
	if (CollapsingHeader("Gyro management", ImGuiTreeNodeFlags_None))
	{
		Text("IsItemHovered: %d", IsItemHovered());
		for (int i = 0; i < 5; i++)
			Text("Some content %d", i);
	}
	if (CollapsingHeader("Virtual Controller", ImGuiTreeNodeFlags_None))
	{
		Text("IsItemHovered: %d", IsItemHovered());
		for (int i = 0; i < 5; i++)
			Text("Some content %d", i);
	}
}
