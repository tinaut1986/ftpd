// ftpd is a server implementation based on the following:
// - RFC  959 (https://tools.ietf.org/html/rfc959)
// - RFC 3659 (https://tools.ietf.org/html/rfc3659)
// - suggested implementation details from https://cr.yp.to/ftp/filesystem.html
// - Deflate transmission mode for FTP
//   (https://tools.ietf.org/html/draft-preston-ftpext-deflate-04)
//
// Copyright (C) 2024 Michael Theall
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef CLASSIC

#include "ui.h"

#include <imgui_internal.h>

#include <cmath>
#include <string>
#include <vector>
#include <cstring>

float ui::scale ()
{
#ifdef __3DS__
	return 1.0f;
#else
	// the 3DS UI is designed around a ~13px font; read the atlas font so this also works
	// before the first frame
	auto const &fonts = ImGui::GetIO ().Fonts->Fonts;
	return (fonts.Size > 0 ? fonts[0]->FontSize : 13.0f) / 13.0f;
#endif
}

void ui::applyTheme (ImGuiStyle &style_)
{
	auto const s = scale ();

	// Modern rounded corners
	style_.WindowRounding    = 5.0f * s;
	style_.ChildRounding     = 5.0f * s;
	style_.FrameRounding     = 4.0f * s;
	style_.PopupRounding     = 4.0f * s;
	style_.ScrollbarRounding = 6.0f * s;
	style_.GrabRounding      = 5.0f * s;
	style_.TabRounding       = 4.0f * s;

	// Thin scrollbars; the touch target is widened by TouchScroller::map()
	style_.ScrollbarSize     = 9.0f * s;
	style_.GrabMinSize       = 24.0f * s;
	style_.TouchExtraPadding = ImVec2 (4.0f * s, 4.0f * s);

	// Refined modern dark theme
	style_.Colors[ImGuiCol_WindowBg]             = ImVec4 (0.10f, 0.12f, 0.16f, 0.85f);
	style_.Colors[ImGuiCol_ChildBg]              = ImVec4 (0.13f, 0.16f, 0.22f, 0.70f);
	style_.Colors[ImGuiCol_PopupBg]              = ImVec4 (0.12f, 0.14f, 0.19f, 0.95f);
	style_.Colors[ImGuiCol_Border]               = ImVec4 (0.22f, 0.28f, 0.38f, 0.50f);
	style_.Colors[ImGuiCol_BorderShadow]         = ImVec4 (0.00f, 0.00f, 0.00f, 0.00f);
	style_.Colors[ImGuiCol_TitleBg]              = ImVec4 (0.12f, 0.15f, 0.20f, 1.00f);
	style_.Colors[ImGuiCol_TitleBgActive]        = ImVec4 (0.15f, 0.19f, 0.26f, 1.00f);
	style_.Colors[ImGuiCol_MenuBarBg]            = ImVec4 (0.11f, 0.14f, 0.19f, 1.00f);
	style_.Colors[ImGuiCol_Button]               = ImVec4 (0.18f, 0.26f, 0.38f, 0.80f);
	style_.Colors[ImGuiCol_ButtonHovered]        = ImVec4 (0.25f, 0.36f, 0.52f, 1.00f);
	style_.Colors[ImGuiCol_ButtonActive]         = ImVec4 (0.30f, 0.44f, 0.62f, 1.00f);
	style_.Colors[ImGuiCol_PlotHistogram]        = ImVec4 (0.18f, 0.68f, 0.90f, 1.00f);
	style_.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4 (0.25f, 0.78f, 1.00f, 1.00f);
	style_.Colors[ImGuiCol_Header]               = ImVec4 (0.18f, 0.26f, 0.38f, 0.65f);
	style_.Colors[ImGuiCol_HeaderHovered]        = ImVec4 (0.24f, 0.35f, 0.50f, 0.80f);
	style_.Colors[ImGuiCol_HeaderActive]         = ImVec4 (0.28f, 0.40f, 0.58f, 1.00f);
	style_.Colors[ImGuiCol_ScrollbarBg]          = ImVec4 (0.07f, 0.09f, 0.13f, 0.60f);
	style_.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4 (0.30f, 0.42f, 0.58f, 0.85f);
	style_.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4 (0.38f, 0.54f, 0.74f, 1.00f);
	style_.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4 (0.45f, 0.65f, 0.90f, 1.00f);
	style_.Colors[ImGuiCol_Separator]            = ImVec4 (0.20f, 0.25f, 0.34f, 0.50f);
	style_.Colors[ImGuiCol_ModalWindowDimBg]     = ImVec4 (0.00f, 0.00f, 0.00f, 0.65f);

	// Tabs: dim when unselected, accent fill and overline when selected
	style_.TabBarBorderSize   = 2.0f * s;
	style_.TabBarOverlineSize = 2.0f * s;
	style_.Colors[ImGuiCol_Tab]                       = ImVec4 (0.14f, 0.19f, 0.27f, 1.00f);
	style_.Colors[ImGuiCol_TabHovered]                = ImVec4 (0.28f, 0.42f, 0.60f, 1.00f);
	style_.Colors[ImGuiCol_TabSelected]               = ImVec4 (0.22f, 0.44f, 0.72f, 1.00f);
	style_.Colors[ImGuiCol_TabSelectedOverline]       = ImVec4 (0.45f, 0.75f, 1.00f, 1.00f);
	style_.Colors[ImGuiCol_TabDimmed]                 = style_.Colors[ImGuiCol_Tab];
	style_.Colors[ImGuiCol_TabDimmedSelected]         = style_.Colors[ImGuiCol_TabSelected];
	style_.Colors[ImGuiCol_TabDimmedSelectedOverline] = style_.Colors[ImGuiCol_TabSelectedOverline];
}

namespace
{
/// \brief Width of the touch zone around a scrollbar (3DS pixels)
constexpr float SCROLLBAR_TOUCH_WIDTH = 28.0f;

/// \brief Geometry of a badge
struct BadgeMetrics
{
	float width;
	float height;
	float fontSize;
	ImVec2 textCenter; ///< Offset from the text origin to the visual center of the symbol
};

BadgeMetrics badgeMetrics (char const *const text_)
{
	auto const height   = ImGui::GetTextLineHeight () * 1.2f;
	auto const fontSize = ImGui::GetFontSize () * 0.85f;

	auto *const font = ImGui::GetFont ();
	auto const k     = fontSize / font->FontSize;

	auto const rawSize   = ImGui::CalcTextSize (text_);
	auto const textSize  = ImVec2 (rawSize.x * 0.85f, rawSize.y * 0.85f);
	auto const boxCenter = ImVec2 (textSize.x * 0.5f, textSize.y * 0.5f);

	// Vertical center of a capital letter. Some fonts (the 3DS system font) report the whole
	// glyph cell as the glyph box, which is no use for centering: there, estimate the ink from
	// the baseline and a typical cap height instead.
	auto capCenterY = boxCenter.y;
	if (auto const *const glyph = font->FindGlyph (static_cast<ImWchar> ('H')))
	{
		if (glyph->Y1 - glyph->Y0 > 0.85f * font->FontSize)
			capCenterY = font->Ascent * k * 0.65f;
		else
			capCenterY = (glyph->Y0 + glyph->Y1) * 0.5f * k;
	}

	// one character: circle; longer text: pill
	auto const width = std::strlen (text_) <= 1 ? height : textSize.x + height * 0.8f;
	return {width, height, fontSize, ImVec2 (boxCenter.x, capCenterY)};
}

/// \brief Strokes (polylines in an arbitrary unit box) of the symbols drawn as vectors
std::vector<std::vector<ImVec2>> symbolStrokes (char const c_)
{
	using Line = std::vector<ImVec2>;
	switch (c_)
	{
	case 'Y':
		return {Line {{-0.5f, -0.5f}, {0.0f, 0.0f}, {0.5f, -0.5f}}, Line {{0.0f, 0.0f}, {0.0f, 0.5f}}};
	case 'X':
		return {Line {{-0.5f, -0.5f}, {0.5f, 0.5f}}, Line {{0.5f, -0.5f}, {-0.5f, 0.5f}}};
	case 'A':
		return {Line {{-0.5f, 0.5f}, {0.0f, -0.5f}, {0.5f, 0.5f}}, Line {{-0.25f, 0.15f}, {0.25f, 0.15f}}};
	case 'B':
		return {Line {{-0.4f, -0.5f}, {-0.4f, 0.5f}},
		    Line {{-0.4f, -0.5f}, {0.1f, -0.5f}, {0.3f, -0.42f}, {0.35f, -0.25f}, {0.3f, -0.08f}, {0.1f, 0.0f}, {-0.4f, 0.0f}},
		    Line {{-0.4f, 0.0f}, {0.15f, 0.0f}, {0.38f, 0.08f}, {0.45f, 0.25f}, {0.38f, 0.42f}, {0.15f, 0.5f}, {-0.4f, 0.5f}}};
	case 'R':
		return {Line {{-0.4f, -0.5f}, {-0.4f, 0.5f}},
		    Line {{-0.4f, -0.5f}, {0.1f, -0.5f}, {0.3f, -0.42f}, {0.35f, -0.22f}, {0.3f, -0.02f}, {0.1f, 0.05f}, {-0.4f, 0.05f}},
		    Line {{0.0f, 0.05f}, {0.4f, 0.5f}}};
	case 'L':
		return {Line {{-0.35f, -0.5f}, {-0.35f, 0.5f}, {0.4f, 0.5f}}};
	case '+':
		return {Line {{-0.5f, 0.0f}, {0.5f, 0.0f}}, Line {{0.0f, -0.5f}, {0.0f, 0.5f}}};
	case '-':
		return {Line {{-0.5f, 0.0f}, {0.5f, 0.0f}}};
	default:
		return {};
	}
}

void drawBadge (ImDrawList *const dl_, ImVec2 const &min_, BadgeMetrics const &m_, char const *const text_)
{
	auto const center = ImVec2 (min_.x + m_.width * 0.5f, min_.y + m_.height * 0.5f);
	auto const white  = IM_COL32 (255, 255, 255, 255);
	auto const thick  = ImMax (1.0f, m_.height * 0.08f);

	// transparent background, white outline and symbol
	dl_->AddRect (min_,
	    ImVec2 (min_.x + m_.width, min_.y + m_.height),
	    white,
	    m_.height * 0.5f,
	    0,
	    thick);

	// Single symbols are drawn as strokes: centered by geometry, so it looks the same
	// whatever the font metrics are
	auto const strokes = std::strlen (text_) == 1 ? symbolStrokes (text_[0]) : std::vector<std::vector<ImVec2>> {};
	if (!strokes.empty ())
	{
		auto lo = ImVec2 (1e9f, 1e9f);
		auto hi = ImVec2 (-1e9f, -1e9f);
		for (auto const &line : strokes)
		{
			for (auto const &p : line)
			{
				lo = ImVec2 (ImMin (lo.x, p.x), ImMin (lo.y, p.y));
				hi = ImVec2 (ImMax (hi.x, p.x), ImMax (hi.y, p.y));
			}
		}

		auto const mid   = ImVec2 ((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f);
		auto const scale = m_.height * 0.46f / ImMax (hi.x - lo.x, hi.y - lo.y);

		std::vector<ImVec2> points;
		for (auto const &line : strokes)
		{
			points.clear ();
			for (auto const &p : line)
				points.emplace_back (center.x + (p.x - mid.x) * scale, center.y + (p.y - mid.y) * scale);

			dl_->AddPolyline (points.data (), static_cast<int> (points.size ()), white, ImDrawFlags_None, ImMax (1.0f, m_.height * 0.09f));
		}
		return;
	}

	dl_->AddText (ImGui::GetFont (),
	    m_.fontSize,
	    ImVec2 (center.x - m_.textCenter.x, center.y - m_.textCenter.y),
	    white,
	    text_);
}

/// \brief Helper to verify if an ImGuiWindow pointer is still valid in context
bool isWindowValid (ImGuiWindow const *const window_)
{
	if (!window_)
		return false;

	ImGuiContext &g = *GImGui;
	for (int i = 0; i < g.Windows.Size; ++i)
	{
		if (g.Windows[i] == window_ && window_->Active && !window_->Hidden)
			return true;
	}
	return false;
}

/// \brief Helper to find the scrollable window or child under pos
ImGuiWindow *findScrollableWindowAt (ImVec2 const &pos_, ImGuiAxis const axis_)
{
	ImGuiContext &g = *GImGui;
	ImGuiWindow *w  = g.HoveredWindow;
	if (!w)
	{
		for (int i = g.Windows.Size - 1; i >= 0; --i)
		{
			ImGuiWindow *candidate = g.Windows[i];
			if (candidate->Active && !candidate->Hidden && candidate->Rect ().Contains (pos_))
			{
				w = candidate;
				break;
			}
		}
	}

	while (w)
	{
		auto const scrollMax = axis_ == ImGuiAxis_X ? w->ScrollMax.x : w->ScrollMax.y;
		if (scrollMax > 0.0f && !(w->Flags & ImGuiWindowFlags_NoScrollWithMouse))
			return w;
		w = w->ParentWindow;
	}
	return nullptr;
}
}

void ui::TouchScroller::update (ImGuiIO &io_, bool const touching_, ImVec2 const &pos_)
{
	// distance before a touch becomes a scroll instead of a tap
	auto const threshold = 5.0f * scale ();

	if (touching_)
	{
		if (!m_active)
		{
			// First frame of touch: initialize tracking and press
			m_active    = true;
			m_axis      = Axis::None;
			m_start     = pos_;
			m_prev      = pos_;
			m_target    = nullptr;
			m_targetX   = nullptr;
			m_velocityY = 0.0f; // stop inertial coasting immediately on new touch
			m_velocityX = 0.0f;

			io_.AddMouseButtonEvent (0, true);
			return;
		}

		float const dx = pos_.x - m_start.x;
		float const dy = pos_.y - m_start.y;

		ImGuiContext &g = *GImGui;

		// A scrollbar or slider that is being held handles its own drag
		bool isScrollbarActive = false;
		if (g.ActiveId != 0 && g.ActiveIdWindow)
		{
			isScrollbarActive =
			    g.ActiveId == ImGui::GetWindowScrollbarID (g.ActiveIdWindow, ImGuiAxis_Y) ||
			    g.ActiveId == ImGui::GetWindowScrollbarID (g.ActiveIdWindow, ImGuiAxis_X);
		}

		// ImGui also makes the window "active" (ActiveId == MoveId) when the press lands on empty
		// space, even if the window cannot move; that is not a widget grabbing the touch
		bool const itemActive = g.ActiveId != 0 && !isScrollbarActive &&
		                        !(g.ActiveIdWindow && g.ActiveId == g.ActiveIdWindow->MoveId);

		// Mostly horizontal drag on an active item (e.g. slider): let the item handle it
		bool const isHorizontalItemDrag = itemActive && std::fabs (dx) > std::fabs (dy);

		if (m_axis == Axis::None && !m_locked && !isScrollbarActive)
		{
			if (!isHorizontalItemDrag && std::fabs (dy) > threshold && std::fabs (dy) > std::fabs (dx))
			{
				m_axis   = Axis::Vertical;
				m_target = findScrollableWindowAt (pos_, ImGuiAxis_Y);
			}
			else if (!itemActive && std::fabs (dx) > threshold && std::fabs (dx) > std::fabs (dy))
			{
				// only from empty space: a pressed button/slider keeps its horizontal drag
				m_targetX = findScrollableWindowAt (pos_, ImGuiAxis_X);
				if (m_targetX)
					m_axis = Axis::Horizontal;
			}

			if (m_axis != Axis::None)
			{
				// Cancel the pressed item so releasing does not fire a click
				ImGui::ClearActiveID ();
				io_.AddMouseButtonEvent (0, false);
			}
		}

		if (m_axis == Axis::Vertical)
		{
			float const deltaY = pos_.y - m_prev.y;

			if (!isWindowValid (m_target))
				m_target = findScrollableWindowAt (pos_, ImGuiAxis_Y);

			if (isWindowValid (m_target) && m_target->ScrollMax.y > 0.0f)
			{
				ImGui::SetScrollY (m_target,
				    ImClamp (m_target->Scroll.y - deltaY, 0.0f, m_target->ScrollMax.y));

				// Smooth velocity tracking for inertia
				m_velocityY = m_velocityY * 0.35f + deltaY * 0.65f;
			}
		}
		else if (m_axis == Axis::Horizontal)
		{
			float const deltaX = pos_.x - m_prev.x;

			if (!isWindowValid (m_targetX))
				m_targetX = findScrollableWindowAt (pos_, ImGuiAxis_X);

			if (isWindowValid (m_targetX) && m_targetX->ScrollMax.x > 0.0f)
			{
				ImGui::SetScrollX (m_targetX,
				    ImClamp (m_targetX->Scroll.x - deltaX, 0.0f, m_targetX->ScrollMax.x));

				m_velocityX = m_velocityX * 0.35f + deltaX * 0.65f;
			}
		}

		m_prev = pos_;
		return;
	}

	if (m_active)
	{
		// Touch released
		if (m_axis != Axis::None)
		{
			io_.AddMouseButtonEvent (0, false);
			ImGui::ClearActiveID ();

			// If the user stopped before lifting, drop the momentum
			if (std::fabs (m_velocityY) < 1.0f)
			{
				m_velocityY = 0.0f;
				m_target    = nullptr;
			}
			if (std::fabs (m_velocityX) < 1.0f)
			{
				m_velocityX = 0.0f;
				m_targetX   = nullptr;
			}
		}
		else
		{
			// Clean tap
			io_.AddMouseButtonEvent (0, false);
			m_velocityY = m_velocityX = 0.0f;
			m_target = m_targetX = nullptr;
		}

		m_active = false;
		m_axis   = Axis::None;
		return;
	}

	// Inertial scrolling coasting after a flick
	auto const coast = [] (ImGuiWindow *&window_, float &velocity_, bool const horizontal_)
	{
		if (isWindowValid (window_) && std::fabs (velocity_) > 0.5f)
		{
			if (horizontal_)
				ImGui::SetScrollX (
				    window_, ImClamp (window_->Scroll.x - velocity_, 0.0f, window_->ScrollMax.x));
			else
				ImGui::SetScrollY (
				    window_, ImClamp (window_->Scroll.y - velocity_, 0.0f, window_->ScrollMax.y));

			velocity_ *= 0.88f; // smooth deceleration friction
			if (std::fabs (velocity_) >= 0.5f)
				return;
		}

		velocity_ = 0.0f;
		window_   = nullptr;
	};

	coast (m_target, m_velocityY, false);
	coast (m_targetX, m_velocityX, true);
}

void ui::padScroll ()
{
	auto *const window = ImGui::GetCurrentWindow ();
	if (window->ScrollMax.y <= 0.0f)
		return;

	auto const id = window->ID;
	ImGui::SetKeyOwner (ImGuiKey_GamepadDpadUp, id);
	ImGui::SetKeyOwner (ImGuiKey_GamepadDpadDown, id);

	auto const step = px (6.0f);
	if (ImGui::IsKeyDown (ImGuiKey_GamepadDpadUp, id))
		ImGui::SetScrollY (window, ImMax (window->Scroll.y - step, 0.0f));
	if (ImGui::IsKeyDown (ImGuiKey_GamepadDpadDown, id))
		ImGui::SetScrollY (window, ImMin (window->Scroll.y + step, window->ScrollMax.y));
}

void ui::badge (char const *const text_)
{
	auto const m   = badgeMetrics (text_);
	auto const pos = ImGui::GetCursorScreenPos ();
	ImGui::Dummy (ImVec2 (m.width, m.height));
	drawBadge (ImGui::GetWindowDrawList (), pos, m, text_);
}

void ui::controlRow (std::initializer_list<char const *> const badges_, char const *const text_)
{
	auto const height = ImGui::GetTextLineHeight () * 1.2f;

	bool first = true;
	for (auto const b : badges_)
	{
		if (!first)
			ImGui::SameLine (0.0f, px (3.0f));
		badge (b);
		first = false;
	}

	ImGui::SameLine (0.0f, px (8.0f));

	// center the text vertically on the badge
	ImGui::SetCursorPosY (ImGui::GetCursorPosY () + (height - ImGui::GetTextLineHeight ()) * 0.5f);
	ImGui::TextWrapped ("%s", text_);
}

bool ui::badgeButton (
    char const *const id_, char const *const label_, char const *const badge_, ImVec2 const &size_)
{
	auto const pressed = ImGui::Button ((std::string ("##") + id_).c_str (), size_);

	auto const min = ImGui::GetItemRectMin ();
	auto const max = ImGui::GetItemRectMax ();
	auto *const dl = ImGui::GetWindowDrawList ();

	auto const textSize = ImGui::CalcTextSize (label_);
	auto const gap      = px (6.0f);

	BadgeMetrics m{};
	auto total = textSize.x;
	if (badge_)
	{
		m = badgeMetrics (badge_);
		total += m.width + gap;
	}

	auto x = (min.x + max.x - total) * 0.5f;
	auto const cy = (min.y + max.y) * 0.5f;

	if (badge_)
	{
		drawBadge (dl, ImVec2 (x, cy - m.height * 0.5f), m, badge_);
		x += m.width + gap;
	}

	dl->AddText (ImVec2 (x, cy - textSize.y * 0.5f), ImGui::GetColorU32 (ImGuiCol_Text), label_);

	return pressed;
}

ImVec2 ui::TouchScroller::map (bool const touching_, ImVec2 const &pos_)
{
	if (!touching_)
		return m_lastMapped;

	if (!m_active)
	{
		// New touch: decide whether it grabs a scrollbar
		m_locked = false;

		auto *const w = findScrollableWindowAt (pos_, ImGuiAxis_Y);
		if (w && w->ScrollbarY)
		{
			auto const bar = ImGui::GetWindowScrollbarRect (w, ImGuiAxis_Y);
			if (pos_.x >= bar.Max.x - px (SCROLLBAR_TOUCH_WIDTH) && pos_.x <= bar.Max.x + px (SCROLLBAR_TOUCH_WIDTH) &&
			    pos_.y >= bar.Min.y && pos_.y <= bar.Max.y)
			{
				m_locked = true;
				m_lockX  = (bar.Min.x + bar.Max.x) * 0.5f;
			}
		}
	}

	m_lastMapped = m_locked ? ImVec2 (m_lockX, pos_.y) : pos_;
	return m_lastMapped;
}

#endif
