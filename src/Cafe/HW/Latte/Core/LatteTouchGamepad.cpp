#include "Cafe/HW/Latte/Core/LatteTouchGamepad.h"

#include "WindowSystem.h"
#include "imgui/imgui_extension.h"
#include "input/emulated/VPADController.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>

namespace LatteTouchGamepad
{
	namespace
	{
		constexpr uint32 kVirtualKeyBase = 0x7F000000;

		enum class Shape
		{
			kCircle,
			kRect
		};

		struct Button
		{
			uint32 vpad;
			const char* label;
			Shape shape;
			float cx, cy, r; // circle: center and radius. rect: center and half size (r = half height, width = 2.6 r)
		};

		struct Stick
		{
			float cx, cy, r;
			uint32 up, down, left, right;
		};

		struct Layout
		{
			std::vector<Button> buttons;
			Stick left{}, right{};
		};

		using B = VPADController;

		Layout MakeLayout(int w, int h)
		{
			const float u = (float)h; // everything scales with the height of the window
			Layout l;
			// ABXY, Wii U layout
			const float abxyX = w - 0.30f * u, abxyY = 0.28f * u, off = 0.15f * u, br = 0.075f * u;
			l.buttons.push_back({B::kButtonId_A, "A", Shape::kCircle, abxyX + off, abxyY, br});
			l.buttons.push_back({B::kButtonId_B, "B", Shape::kCircle, abxyX, abxyY + off, br});
			l.buttons.push_back({B::kButtonId_X, "X", Shape::kCircle, abxyX, abxyY - off, br});
			l.buttons.push_back({B::kButtonId_Y, "Y", Shape::kCircle, abxyX - off, abxyY, br});
			// D-pad
			const float dpadX = 0.30f * u, dpadY = 0.28f * u;
			l.buttons.push_back({B::kButtonId_Up, "^", Shape::kCircle, dpadX, dpadY - off, br});
			l.buttons.push_back({B::kButtonId_Down, "v", Shape::kCircle, dpadX, dpadY + off, br});
			l.buttons.push_back({B::kButtonId_Left, "<", Shape::kCircle, dpadX - off, dpadY, br});
			l.buttons.push_back({B::kButtonId_Right, ">", Shape::kCircle, dpadX + off, dpadY, br});
			// shoulders
			const float sr = 0.04f * u, sy = 0.05f * u;
			l.buttons.push_back({B::kButtonId_ZL, "ZL", Shape::kRect, 0.14f * u, sy, sr});
			l.buttons.push_back({B::kButtonId_L, "L", Shape::kRect, 0.36f * u, sy, sr});
			l.buttons.push_back({B::kButtonId_R, "R", Shape::kRect, w - 0.36f * u, sy, sr});
			l.buttons.push_back({B::kButtonId_ZR, "ZR", Shape::kRect, w - 0.14f * u, sy, sr});
			// menu buttons
			const float mr = 0.035f * u, my = 0.94f * u;
			l.buttons.push_back({B::kButtonId_Minus, "-", Shape::kRect, w / 2.0f - 0.2f * u, my, mr});
			l.buttons.push_back({B::kButtonId_Home, "H", Shape::kRect, w / 2.0f, my, mr});
			l.buttons.push_back({B::kButtonId_Plus, "+", Shape::kRect, w / 2.0f + 0.2f * u, my, mr});
			// sticks
			l.left = {0.30f * u, 0.74f * u, 0.15f * u, B::kButtonId_StickL_Up, B::kButtonId_StickL_Down, B::kButtonId_StickL_Left, B::kButtonId_StickL_Right};
			l.right = {w - 0.30f * u, 0.74f * u, 0.15f * u, B::kButtonId_StickR_Up, B::kButtonId_StickR_Down, B::kButtonId_StickR_Left, B::kButtonId_StickR_Right};
			return l;
		}

		bool Hit(const Button& b, float x, float y)
		{
			if (b.shape == Shape::kCircle)
			{
				const float dx = x - b.cx, dy = y - b.cy;
				return dx * dx + dy * dy <= (b.r * 1.25f) * (b.r * 1.25f);
			}
			return std::fabs(x - b.cx) <= b.r * 1.3f * 1.25f && std::fabs(y - b.cy) <= b.r * 1.25f;
		}

		// what a touch point controls: a button index, a stick (-1 left, -2 right) or nothing
		enum : int
		{
			kOwnerScreen = -3,
			kOwnerStickRight = -2,
			kOwnerStickLeft = -1
		};

		std::mutex s_mutex;
		bool s_enabled = false;
		float s_opacity = 0.5f;
		std::map<int, int> s_owners; // touch id -> owner
		std::map<uint32, bool> s_pressed; // vpad button id -> pressed
		float s_leftOffset[2] = {0, 0}, s_rightOffset[2] = {0, 0}; // stick knob offsets in pixels
		int s_layoutW = 0, s_layoutH = 0;
		Layout s_layout;

		void SetPressed(uint32 vpad, bool pressed)
		{
			auto& state = s_pressed[vpad];
			if (state == pressed)
				return;
			state = pressed;
			WindowSystem::GetWindowInfo().set_keystate(kVirtualKeyBase + vpad, pressed);
		}

		void ReleaseAll()
		{
			for (auto& [vpad, state] : s_pressed)
			{
				if (state)
					WindowSystem::GetWindowInfo().set_keystate(kVirtualKeyBase + vpad, false);
				state = false;
			}
			s_owners.clear();
			s_leftOffset[0] = s_leftOffset[1] = s_rightOffset[0] = s_rightOffset[1] = 0;
		}

		void ApplyStick(const Stick& s, float x, float y, float* offset)
		{
			float dx = x - s.cx, dy = y - s.cy;
			const float len = std::sqrt(dx * dx + dy * dy);
			if (len > s.r)
			{
				dx *= s.r / len;
				dy *= s.r / len;
			}
			offset[0] = dx;
			offset[1] = dy;
			const float t = 0.35f * s.r;
			SetPressed(s.up, dy < -t);
			SetPressed(s.down, dy > t);
			SetPressed(s.left, dx < -t);
			SetPressed(s.right, dx > t);
		}

		void ReleaseStick(const Stick& s, float* offset)
		{
			offset[0] = offset[1] = 0;
			SetPressed(s.up, false);
			SetPressed(s.down, false);
			SetPressed(s.left, false);
			SetPressed(s.right, false);
		}
	} // namespace

	void SetEnabled(bool enabled)
	{
		std::lock_guard lock(s_mutex);
		if (s_enabled && !enabled)
			ReleaseAll();
		s_enabled = enabled;
	}

	bool IsEnabled()
	{
		std::lock_guard lock(s_mutex);
		return s_enabled;
	}

	void SetOpacity(float opacity)
	{
		std::lock_guard lock(s_mutex);
		s_opacity = std::clamp(opacity, 0.1f, 1.0f);
	}

	const std::vector<KeyMapping>& GetKeyMappings()
	{
		static const std::vector<KeyMapping> mappings = [] {
			std::vector<KeyMapping> m;
			for (uint32 id : {B::kButtonId_A, B::kButtonId_B, B::kButtonId_X, B::kButtonId_Y, B::kButtonId_L, B::kButtonId_R, B::kButtonId_ZL,
							  B::kButtonId_ZR, B::kButtonId_Plus, B::kButtonId_Minus, B::kButtonId_Up, B::kButtonId_Down, B::kButtonId_Left,
							  B::kButtonId_Right, B::kButtonId_Home, B::kButtonId_StickL_Up, B::kButtonId_StickL_Down, B::kButtonId_StickL_Left,
							  B::kButtonId_StickL_Right, B::kButtonId_StickR_Up, B::kButtonId_StickR_Down, B::kButtonId_StickR_Left,
							  B::kButtonId_StickR_Right})
				m.push_back({id, kVirtualKeyBase + id});
			return m;
		}();
		return mappings;
	}

	void UpdateTouches(const std::vector<TouchPoint>& points, int width, int height, bool& screenDown, TouchPoint& screenPoint)
	{
		std::lock_guard lock(s_mutex);
		screenDown = false;
		if (!s_enabled || width <= 0 || height <= 0)
		{
			// plain touch screen: follow the first point
			for (const auto& p : points)
			{
				if (p.down)
				{
					screenDown = true;
					screenPoint = p;
					break;
				}
			}
			return;
		}
		if (width != s_layoutW || height != s_layoutH)
		{
			s_layout = MakeLayout(width, height);
			s_layoutW = width;
			s_layoutH = height;
		}
		const Layout& layout = s_layout;

		std::map<int, bool> seen;
		bool buttonDown[64] = {};
		bool leftHeld = false, rightHeld = false;
		for (const auto& p : points)
		{
			if (!p.down)
				continue;
			seen[p.id] = true;
			auto it = s_owners.find(p.id);
			if (it == s_owners.end())
			{
				int owner = kOwnerScreen;
				const float dl = std::hypot(p.x - layout.left.cx, p.y - layout.left.cy);
				const float dr = std::hypot(p.x - layout.right.cx, p.y - layout.right.cy);
				if (dl <= layout.left.r * 1.4f)
					owner = kOwnerStickLeft;
				else if (dr <= layout.right.r * 1.4f)
					owner = kOwnerStickRight;
				else
				{
					for (size_t i = 0; i < layout.buttons.size(); i++)
					{
						if (Hit(layout.buttons[i], p.x, p.y))
						{
							owner = (int)i;
							break;
						}
					}
				}
				it = s_owners.emplace(p.id, owner).first;
			}
			const int owner = it->second;
			if (owner == kOwnerStickLeft)
			{
				ApplyStick(layout.left, p.x, p.y, s_leftOffset);
				leftHeld = true;
			}
			else if (owner == kOwnerStickRight)
			{
				ApplyStick(layout.right, p.x, p.y, s_rightOffset);
				rightHeld = true;
			}
			else if (owner >= 0)
			{
				if (owner < 64)
					buttonDown[owner] = true;
			}
			else if (!screenDown)
			{
				screenDown = true;
				screenPoint = p;
			}
		}
		// forget the points that are gone
		for (auto it = s_owners.begin(); it != s_owners.end();)
		{
			if (!seen.count(it->first))
				it = s_owners.erase(it);
			else
				++it;
		}
		for (size_t i = 0; i < layout.buttons.size() && i < 64; i++)
			SetPressed(layout.buttons[i].vpad, buttonDown[i]);
		if (!leftHeld)
			ReleaseStick(layout.left, s_leftOffset);
		if (!rightHeld)
			ReleaseStick(layout.right, s_rightOffset);
	}

	void Render(int width, int height)
	{
		std::lock_guard lock(s_mutex);
		if (!s_enabled || width <= 0 || height <= 0)
			return;
		if (width != s_layoutW || height != s_layoutH)
		{
			s_layout = MakeLayout(width, height);
			s_layoutW = width;
			s_layoutH = height;
		}
		const Layout& layout = s_layout;
		ImDrawList* draw = ImGui::GetForegroundDrawList();
		const int alpha = (int)(255.0f * s_opacity);
		const ImU32 outline = IM_COL32(255, 255, 255, alpha);
		const ImU32 fillIdle = IM_COL32(0, 0, 0, alpha / 2);
		const ImU32 fillDown = IM_COL32(255, 255, 255, alpha / 2);
		const float thickness = std::max(2.0f, 0.006f * height);
		ImFont* font = ImGui_GetFont((float)(int)std::clamp(0.045f * height, 12.0f, 64.0f));

		auto drawLabel = [&](const char* text, float cx, float cy) {
			if (!font)
				return;
			ImGui::PushFont(font);
			const ImVec2 ts = ImGui::CalcTextSize(text);
			draw->AddText(ImVec2(cx - ts.x / 2, cy - ts.y / 2), outline, text);
			ImGui::PopFont();
		};

		for (const auto& b : layout.buttons)
		{
			const bool down = s_pressed.count(b.vpad) && s_pressed.at(b.vpad);
			if (b.shape == Shape::kCircle)
			{
				draw->AddCircleFilled(ImVec2(b.cx, b.cy), b.r, down ? fillDown : fillIdle, 32);
				draw->AddCircle(ImVec2(b.cx, b.cy), b.r, outline, 32, thickness);
			}
			else
			{
				const ImVec2 a(b.cx - b.r * 1.3f, b.cy - b.r), c(b.cx + b.r * 1.3f, b.cy + b.r);
				draw->AddRectFilled(a, c, down ? fillDown : fillIdle, b.r * 0.4f);
				draw->AddRect(a, c, outline, b.r * 0.4f, 0, thickness);
			}
			drawLabel(b.label, b.cx, b.cy);
		}
		auto drawStick = [&](const Stick& s, const float* offset) {
			draw->AddCircleFilled(ImVec2(s.cx, s.cy), s.r, fillIdle, 48);
			draw->AddCircle(ImVec2(s.cx, s.cy), s.r, outline, 48, thickness);
			draw->AddCircleFilled(ImVec2(s.cx + offset[0], s.cy + offset[1]), s.r * 0.45f, fillDown, 32);
			draw->AddCircle(ImVec2(s.cx + offset[0], s.cy + offset[1]), s.r * 0.45f, outline, 32, thickness);
		};
		drawStick(layout.left, s_leftOffset);
		drawStick(layout.right, s_rightOffset);
	}
} // namespace LatteTouchGamepad
