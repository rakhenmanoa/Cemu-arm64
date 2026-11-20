#pragma once

#include <vector>

// On-screen gamepad drawn on top of the game (used by the Qt mobile interface, ported from the on-screen
// input overlay of the Android port). The buttons are injected as virtual keyboard keys: the mobile interface
// maps them to the Wii U GamePad buttons on the "Keyboard" device of player 1.
namespace LatteTouchGamepad
{
	struct TouchPoint
	{
		int id = 0;
		float x = 0, y = 0; // physical pixels of the game window
		bool down = false;
	};

	struct KeyMapping
	{
		uint32 vpadButton; // VPADController::ButtonId
		uint32 virtualKey; // key code reported to the keyboard device
	};

	void SetEnabled(bool enabled);
	bool IsEnabled();
	void SetOpacity(float opacity); // 0.1 .. 1

	// buttons and the virtual key that stands for each of them
	const std::vector<KeyMapping>& GetKeyMappings();

	// feeds the current touch points of the window. Controls take the touches that start on them; the first
	// other touch is returned as the Wii U GamePad screen touch (screenDown = false when there is none)
	void UpdateTouches(const std::vector<TouchPoint>& points, int width, int height, bool& screenDown, TouchPoint& screenPoint);

	// draws the controls, the ImGui frame must be active
	void Render(int width, int height);
} // namespace LatteTouchGamepad
