// Input mapping part of the bridge. The capture logic is the one of the desktop input panel
// (wxgui/input/panels/InputPanel.cpp), used here from a touch interface.
#include "MobileBackend.h"

#include "input/InputManager.h"
#include "input/api/Controller.h"
#include "input/emulated/ClassicController.h"
#include "input/emulated/ProController.h"
#include "input/emulated/VPADController.h"
#include "input/emulated/WiimoteController.h"

#include <cmath>

namespace MobileBackend
{
	namespace
	{
		constexpr int kPlayerCount = 1;
		bool s_captureArmed = false; // true once every button was released after the capture started

		std::string ButtonName(EmulatedController::Type type, int id)
		{
			std::string_view name;
			switch (type)
			{
			case EmulatedController::VPAD: name = VPADController::get_button_name((VPADController::ButtonId)id); break;
			case EmulatedController::Pro: name = ProController::get_button_name((ProController::ButtonId)id); break;
			case EmulatedController::Classic: name = ClassicController::get_button_name((ClassicController::ButtonId)id); break;
			case EmulatedController::Wiimote: name = WiimoteController::get_button_name((WiimoteController::ButtonId)id); break;
			default: break;
			}
			return name.empty() ? fmt::format("#{}", id) : std::string(name);
		}

		// when two directions of the same stick are reported, keep the one that is tilted more
		bool IsWeakerAxisDirection(uint32 id, const ControllerState& state)
		{
			auto other = [&](uint32 a, uint32 b) { return state.buttons.GetButtonState(a) || state.buttons.GetButtonState(b); };
			if ((id == kAxisXP || id == kAxisXN) && other(kAxisYP, kAxisYN))
				return std::abs(state.axis.y) > std::abs(state.axis.x);
			if ((id == kAxisYP || id == kAxisYN) && other(kAxisXP, kAxisXN))
				return std::abs(state.axis.x) > std::abs(state.axis.y);
			if ((id == kRotationXP || id == kRotationXN) && other(kRotationYP, kRotationYN))
				return std::abs(state.rotation.y) > std::abs(state.rotation.x);
			if ((id == kRotationYP || id == kRotationYN) && other(kRotationXP, kRotationXN))
				return std::abs(state.rotation.x) > std::abs(state.rotation.y);
			if ((id == kTriggerXP || id == kTriggerXN) && other(kTriggerYP, kTriggerYN))
				return std::abs(state.trigger.y) > std::abs(state.trigger.x);
			if ((id == kTriggerYP || id == kTriggerYN) && other(kTriggerXP, kTriggerXN))
				return std::abs(state.trigger.x) > std::abs(state.trigger.y);
			return false;
		}
	} // namespace

	std::vector<InputMapping> GetInputMappings(int player)
	{
		std::vector<InputMapping> result;
		if (player < 0 || player >= kPlayerCount)
			return result;
		auto controller = InputManager::instance().get_controller((size_t)player);
		if (!controller)
			return result;
		const size_t maxId = controller->get_highest_mapping_id();
		for (size_t id = 1; id < maxId; id++)
		{
			InputMapping m;
			m.id = (int)id;
			m.emulatedName = ButtonName(controller->type(), (int)id);
			m.deviceName = controller->get_mapping_name(id);
			result.push_back(std::move(m));
		}
		return result;
	}

	void StartInputCapture()
	{
		s_captureArmed = false;
	}

	bool PollInputCapture(int player, int mappingId)
	{
		if (player < 0 || player >= kPlayerCount || mappingId <= 0)
			return false;
		auto emulated = InputManager::instance().get_controller((size_t)player);
		if (!emulated)
			return false;

		// physical controllers only, the keyboard device is the on-screen gamepad
		std::vector<std::shared_ptr<ControllerBase>> devices;
		for (const auto& c : emulated->get_controllers())
		{
			if (c->api() != InputAPI::Keyboard)
			{
				c->connect();
				devices.push_back(c);
			}
		}
		bool anyPressed = false;
		for (const auto& device : devices)
		{
			const auto& state = device->update_state();
			if (state.buttons.IsIdle())
				continue;
			anyPressed = true;
			if (!s_captureArmed)
				continue; // the button used to start the capture is still held
			for (const uint32 id : state.buttons.GetButtonList())
			{
				if (device->has_axis())
				{
					if (IsWeakerAxisDirection(id, state))
						continue;
					if (id >= kButtonAxisStart && device->get_axis_value(id) < 0.33f)
						continue; // too weak to be on purpose
				}
				emulated->set_mapping((uint64)mappingId, device, id);
				InputManager::instance().save((size_t)player);
				s_captureArmed = false;
				return true;
			}
		}
		if (!anyPressed)
			s_captureArmed = true;
		return false;
	}

	void ClearInputMapping(int player, int mappingId)
	{
		if (player < 0 || player >= kPlayerCount)
			return;
		auto controller = InputManager::instance().get_controller((size_t)player);
		if (!controller)
			return;
		controller->delete_mapping((uint64)mappingId);
		InputManager::instance().save((size_t)player);
	}
} // namespace MobileBackend
