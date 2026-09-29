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

#pragma once

#ifndef CLASSIC

#include <imgui.h>

#include <initializer_list>

struct ImGuiWindow;

/// \brief Shared look & feel and touch helpers for the ftpd-EX UI (3DS and Switch)
namespace ui
{
/// \brief Scale of pixel sizes relative to the 3DS layout the UI was designed for
float scale ();

/// \brief Scale a pixel size designed for the 3DS to the current platform
inline float px (float const v_)
{
	return v_ * scale ();
}

/// \brief Key the Switch backend sends for the Y button.
///
/// Y is not forwarded as ImGuiKey_GamepadFaceLeft: ImGui uses that key for its window
/// switcher (holding it pops up a one-entry list) and menu bar focus.
inline constexpr ImGuiKey KEY_Y = ImGuiKey_F24;

/// \brief Draw an inline controller-button badge: a circle with the symbol inside (letters
/// and symbols of one character), or a rounded pill for longer text such as "SELECT"
/// \param text_ Symbol to draw
void badge (char const *text_);

/// \brief Draw a row of badges followed by wrapped text, e.g. the Y badge and "Open settings"
/// \param badges_ Badge symbols, drawn left to right
/// \param text_ Description
void controlRow (std::initializer_list<char const *> badges_, char const *text_);

/// \brief Button with an optional badge and label centered together
/// \param id_ Unique ImGui id (without the leading ##)
/// \param label_ Button text
/// \param badge_ Badge symbol, or nullptr for a plain button
/// \param size_ Button size
/// \return Whether the button was pressed
bool badgeButton (char const *id_, char const *label_, char const *badge_, ImVec2 const &size_);

/// \brief Apply the ftpd-EX theme (rounding, touch-friendly scrollbars, colors)
/// \param style_ Style to modify
void applyTheme (ImGuiStyle &style_);

/// \brief Turns a touch point into clicks and drag-scrolling with inertia
///
/// The platform sets the mouse position; this class only emits the left button events
/// and scrolls the window under the finger once a vertical drag is detected.
class TouchScroller
{
public:
	/// \brief Position to give ImGui for a touch. A touch that lands near the edge of a
	/// scrollable window's scrollbar is snapped onto the (thin) scrollbar, so the effective
	/// touch target is much wider than what is drawn. Call before update().
	/// \param touching_ Whether the screen is touched this frame
	/// \param pos_ Raw touch position in ImGui coordinates
	ImVec2 map (bool touching_, ImVec2 const &pos_);

	/// \brief Update state; call once per frame before ImGui::NewFrame
	/// \param io_ ImGui IO
	/// \param touching_ Whether the screen is touched this frame
	/// \param pos_ Touch position in ImGui coordinates (only used while touching)
	void update (ImGuiIO &io_, bool touching_, ImVec2 const &pos_);

private:
	enum class Axis
	{
		None,
		Vertical,
		Horizontal
	};

	bool m_active = false;
	Axis m_axis   = Axis::None; ///< axis locked once a drag starts
	ImVec2 m_start  = ImVec2 (0.0f, 0.0f);
	ImVec2 m_prev   = ImVec2 (0.0f, 0.0f);
	ImGuiWindow *m_target = nullptr;
	float m_velocityY     = 0.0f;
	ImGuiWindow *m_targetX = nullptr;
	float m_velocityX      = 0.0f;
	bool m_locked         = false; ///< touch is grabbing a scrollbar
	float m_lockX         = 0.0f;
	ImVec2 m_lastMapped   = ImVec2 (0.0f, 0.0f);
};
}

#endif
