// On-screen gamepad part of the bridge. The controls, their drawing and the multi-touch handling are in
// Cafe/HW/Latte/Core/LatteTouchGamepad.cpp
#include "MobileBackend.h"

#include "Cafe/HW/Latte/Core/LatteTouchGamepad.h"
#include "WindowSystem.h"
#include "config/ActiveSettings.h"
#include "input/ControllerFactory.h"
#include "input/InputManager.h"
#include "input/emulated/VPADController.h"

namespace MobileBackend
{
	namespace
	{
		constexpr int kPlayerCount = 1; // the mobile interface only configures player 1

		std::shared_ptr<ControllerBase> FindKeyboard(const std::shared_ptr<EmulatedController>& controller)
		{
			for (const auto& c : controller->get_controllers())
			{
				if (c->api() == InputAPI::Keyboard)
					return c;
			}
			return nullptr;
		}

		bool s_screenTouchDown = false;
		int s_screenX = 0, s_screenY = 0;
	} // namespace

	bool IsTouchGamepadEnabled(int player)
	{
		if (player < 0 || player >= kPlayerCount)
			return false;
		auto controller = InputManager::instance().get_controller((size_t)player);
		return controller && FindKeyboard(controller) != nullptr;
	}

	bool SetTouchGamepadEnabled(int player, bool enabled, std::string& errorOut)
	{
		if (player < 0 || player >= kPlayerCount)
		{
			errorOut = "invalid player";
			return false;
		}
		auto& manager = InputManager::instance();
		auto controller = manager.get_controller((size_t)player);
		if (!enabled)
		{
			if (controller)
			{
				if (auto keyboard = FindKeyboard(controller))
				{
					controller->remove_controller(keyboard);
					manager.save((size_t)player);
				}
			}
			LatteTouchGamepad::SetEnabled(false);
			return true;
		}
		if (!controller)
		{
			controller = manager.set_controller((size_t)player, EmulatedController::VPAD);
			if (!controller)
			{
				errorOut = "can't create the emulated controller";
				return false;
			}
		}
		if (controller->type() != EmulatedController::VPAD)
		{
			errorOut = "the on-screen gamepad is only available for the Wii U GamePad controller type";
			return false;
		}
		if (!FindKeyboard(controller))
		{
			auto keyboard = ControllerFactory::CreateController(InputAPI::Keyboard, "keyboard", "Keyboard");
			if (!keyboard)
			{
				errorOut = "keyboard device not available";
				return false;
			}
			controller->add_controller(keyboard);
			keyboard->connect();
			for (const auto& m : LatteTouchGamepad::GetKeyMappings())
				controller->set_mapping(m.vpadButton, keyboard, m.virtualKey);
			manager.save((size_t)player);
		}
		LatteTouchGamepad::SetEnabled(true);
		return true;
	}

	namespace
	{
		std::atomic_bool s_editOnLaunch{false};
	}

	void SetTouchGamepadEditOnLaunch(bool edit)
	{
		s_editOnLaunch = edit;
	}

	bool IsTouchGamepadEditOnLaunch()
	{
		return s_editOnLaunch;
	}

	void SyncTouchGamepad()
	{
		LatteTouchGamepad::SetLayoutPath(_pathToUtf8(ActiveSettings::GetConfigPath("touch_gamepad_layout.txt")));
		const bool enabled = IsTouchGamepadEnabled(0);
		LatteTouchGamepad::SetEnabled(enabled);
		// "edit the layout" was requested from the controllers page: start the next game in the editor
		LatteTouchGamepad::SetEditing(enabled && s_editOnLaunch.exchange(false));
	}

	void SetTouchPoints(const std::vector<TouchPoint>& points)
	{
		std::vector<LatteTouchGamepad::TouchPoint> converted;
		converted.reserve(points.size());
		for (const auto& p : points)
		{
			int x = p.x, y = p.y;
			PhysicalToLogical(x, y);
			converted.push_back({p.id, (float)x, (float)y, p.down});
		}
		sint32 w = 0, h = 0;
		WindowSystem::GetWindowPhysSize(w, h);
		bool screenDown = false;
		LatteTouchGamepad::TouchPoint screen;
		LatteTouchGamepad::UpdateTouches(converted, w, h, screenDown, screen);
		if (screenDown)
		{
			s_screenX = (int)screen.x;
			s_screenY = (int)screen.y;
			SetTouch(s_screenX, s_screenY, true);
			s_screenTouchDown = true;
		}
		else if (s_screenTouchDown)
		{
			SetTouch(s_screenX, s_screenY, false);
			s_screenTouchDown = false;
		}
	}
} // namespace MobileBackend
