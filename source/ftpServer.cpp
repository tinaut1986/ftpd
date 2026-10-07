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

#include "ftpServer.h"

#include "fs.h"
#include "ftpConfig.h"
#include "ftpSession.h"
#include "i18n.h"
#include "licenses.h"
#include "log.h"
#include "platform.h"
#include "sockAddr.h"
#include "socket.h"
#include "ui.h"

#ifndef __NDS__
#include "mdns.h"
#endif

#ifdef __NDS__
#include <dswifi9.h>
#endif

#ifdef __3DS__
#include <citro3d.h>
#endif

#ifndef CLASSIC
#include <imgui.h>

#include "updater.h"

#include <jansson.h>

#include <curl/easy.h>
#include <curl/multi.h>
#ifndef NDEBUG
#include <curl/curl.h>
#endif
#endif

#include <sys/statvfs.h>
using statvfs_t = struct statvfs;

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
using namespace std::chrono_literals;

#ifdef __NDS__
#define LOCKED(x) x
#else
#define LOCKED(x)                                                                                  \
	do                                                                                             \
	{                                                                                              \
		auto const lock = std::scoped_lock (m_lock);                                               \
		x;                                                                                         \
	} while (0)
#endif

namespace
{
/// \brief Application start time
auto const s_startTime = std::time (nullptr);

#ifdef __3DS__
/// \brief Timezone offset in seconds (only used on 3DS)
int s_tzOffset = 0;
#endif

#ifndef __NDS__
/// \brief Mutex for s_freeSpace
platform::Mutex s_lock;
#endif

/// \brief Free space string
std::string s_freeSpace;

#ifndef CLASSIC
/// \brief Bullet point whose text wraps at the window edge instead of being clipped
void bulletWrapped (char const *const fmt_, ...) IM_FMTARGS (1);
void bulletWrapped (char const *const fmt_, ...)
{
	va_list args;
	va_start (args, fmt_);
	ImGui::Bullet ();
	ImGui::TextWrappedV (fmt_, args);
	va_end (args);
}

#ifndef NDEBUG
std::string printable (std::string_view const data_)
{
	unsigned count = 0;
	for (auto const &c : data_)
	{
		if (c != '%' && (std::isprint (c) || std::isspace (c)))
			++count;
		else
			count += 3;
	}

	std::string result;
	result.reserve (count);

	for (auto const &c : data_)
	{
		if (c != '%' && (std::isprint (c) || std::isspace (c)))
			result.push_back (c);
		else
		{
			result.push_back ('%');

			auto const upper = (static_cast<unsigned char> (c) >> 4u) & 0xF;
			auto const lower = (static_cast<unsigned char> (c) >> 0u) & 0xF;

			result.push_back (gsl::narrow_cast<char> (upper < 10 ? upper + '0' : upper + 'A' - 10));
			result.push_back (gsl::narrow_cast<char> (lower < 10 ? lower + '0' : lower + 'A' - 10));
		}
	}

	return result;
}

int curlDebug (CURL *const handle_,
    curl_infotype const type_,
    char *const data_,
    std::size_t const size_,
    void *const user_)
{
	(void)handle_;
	(void)user_;

	auto const text = printable (std::string_view (data_, size_));

	switch (type_)
	{
	case CURLINFO_TEXT:
		info ("== Info: %s", text.c_str ());
		break;

	case CURLINFO_HEADER_OUT:
		info ("=> Send header: %s", text.c_str ());
		break;

	case CURLINFO_DATA_OUT:
		info ("=> Send data: %s", text.c_str ());
		break;

	case CURLINFO_SSL_DATA_OUT:
		info ("=> Send SSL data: %s", text.c_str ());
		break;

	case CURLINFO_HEADER_IN:
		info ("<= Receive header: %s", text.c_str ());
		break;

	case CURLINFO_DATA_IN:
		info ("<= Receive data: %s", text.c_str ());
		break;

	case CURLINFO_SSL_DATA_IN:
		info ("<= Receive SSL data: %s", text.c_str ());
		break;

	default:
		break;
	}

	return 0;
}
#endif

std::size_t curlCallback (void *const contents_,
    std::size_t const size_,
    std::size_t const count_,
    void *const user_)
{
	auto const total = size_ * count_;
	auto const start = static_cast<char *> (contents_);
	auto const end   = start + total;

	auto &result = *static_cast<std::string *> (user_);
	result.insert (std::end (result), start, end);

	return total;
}
#endif
}

///////////////////////////////////////////////////////////////////////////
FtpServer::~FtpServer ()
{
	m_quit = true;

#ifndef __NDS__
	m_thread.join ();
#endif

	saveLog ();

#ifndef CLASSIC
	if (m_uploadLogCurl)
	{
		curl_multi_remove_handle (m_uploadLogCurlM, m_uploadLogCurl);
		curl_easy_cleanup (m_uploadLogCurl);
		curl_mime_free (m_uploadLogMime);
	}

	if (m_uploadLogCurlM)
		curl_multi_cleanup (m_uploadLogCurlM);
#endif
}

FtpServer::FtpServer (UniqueFtpConfig config_)
    : m_config (std::move (config_))
#ifndef CLASSIC
      ,
      m_hostnameSetting (m_config->hostname ()),
      m_checkUpdatesSetting (m_config->checkUpdates ())
#endif
{
#ifndef __NDS__
	mdns::setHostname (m_config->hostname ());

	m_thread = platform::Thread (std::bind (&FtpServer::threadFunc, this));
#endif

#ifndef CLASSIC
	updater::setAutoCheck (m_config->checkUpdates ());
	updater::setBeta (m_config->updateBeta ());
	if (m_config->checkUpdates () && platform::networkVisible ())
		updater::checkAuto ();
#endif

#ifdef __3DS__
	s64 tzOffsetMinutes;
	if (R_SUCCEEDED (svcGetSystemInfo (&tzOffsetMinutes, 0x10000, 0x103)))
		s_tzOffset = tzOffsetMinutes * 60;
#endif
}

void FtpServer::draw ()
{
#ifdef __NDS__
	loop ();
#endif

#ifdef CLASSIC
	{
		char port[7];
#ifndef __NDS__
		auto const lock = std::scoped_lock (m_lock);
#endif
		if (m_socket)
			std::sprintf (port, ":%u", m_socket->sockName ().port ());

		consoleSelect (&g_statusConsole);
		std::printf ("\x1b[0;0H\x1b[32;1m%s \x1b[36;1m%s%s",
		    STATUS_STRING,
		    m_socket ? m_socket->sockName ().name () : "Waiting on WiFi",
		    m_socket ? port : "");

#ifndef __NDS__
		char timeBuffer[16];
		auto const now = std::time (nullptr);
		std::strftime (timeBuffer, sizeof (timeBuffer), "%H:%M:%S", std::localtime (&now));

		std::printf (" \x1b[37;1m%s", timeBuffer);
#endif

		std::fputs ("\x1b[K", stdout);
		std::fflush (stdout);
	}

	{
#ifndef __NDS__
		auto const lock = std::scoped_lock (s_lock);
#endif
		if (!s_freeSpace.empty ())
		{
			consoleSelect (&g_statusConsole);
			std::printf ("\x1b[0;%uH\x1b[32;1m%s",
			    static_cast<unsigned> (g_statusConsole.windowWidth - s_freeSpace.size () + 1),
			    s_freeSpace.c_str ());
			std::fflush (stdout);
		}
	}

	{
#ifndef __NDS__
		auto const lock = std::scoped_lock (m_lock);
#endif
		consoleSelect (&g_sessionConsole);
		std::fputs ("\x1b[2J", stdout);
		for (auto &session : m_sessions)
		{
			session->draw ();
			if (&session != &m_sessions.back ())
				std::fputc ('\n', stdout);
		}
		std::fflush (stdout);
	}

	drawLog ();
#else
	auto const &io    = ImGui::GetIO ();
	auto const width  = io.DisplaySize.x;
	auto const height = io.DisplaySize.y;

	ImGui::SetNextWindowPos (ImVec2 (0, 0), ImGuiCond_FirstUseEver);
#if defined(__3DS__) || defined(__SWITCH__)
#ifdef __3DS__
	auto const kDown         = hidKeysDown ();
	bool const pressSettings = kDown & KEY_Y;
	bool const pressHelp     = kDown & KEY_X;
	bool const pressBack     = kDown & KEY_B;
#else
	bool const pressSettings = ImGui::IsKeyPressed (ui::KEY_Y, false);
	bool const pressHelp     = ImGui::IsKeyPressed (ImGuiKey_GamepadFaceUp, false);
	bool const pressBack     = ImGui::IsKeyPressed (ImGuiKey_GamepadFaceRight, false);
#endif
	if (pressSettings)
	{
		if (m_showSettings)
			closeModals ();
		else
			openSettings ();
	}
	if (pressHelp)
	{
		if (m_showHelp)
			closeModals ();
		else
			openHelp ();
	}
	if (pressBack)
	{
		if (m_showSettings || m_showHelp)
		{
			closeModals ();
		}
	}

#ifdef __3DS__
	// top screen
	ImGui::SetNextWindowSize (ImVec2 (width, height * 0.5f));
	{
		std::array<char, 64> title{};
		{
			auto const serverLock = std::scoped_lock (m_lock);
			std::snprintf (title.data (), title.size (), "ftpd-EX###ftpd");
		}

		ImGui::Begin (title.data (),
		    nullptr,
		    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
		        ImGuiWindowFlags_NoNavFocus);
	}

	// Header on top screen
	{
		auto const serverLock = std::scoped_lock (m_lock);
		if (m_socket)
		{
			ImGui::TextColored (ImVec4 (0.2f, 0.85f, 0.45f, 1.0f), "%s", tr (STR_ONLINE));
			ImGui::SameLine ();
			ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "ftp://%s", m_name.c_str ());

			ImGui::TextDisabled ("%zu %s  •  " FTPD_VERSION_LABEL,
			    m_sessions.size (),
			    m_sessions.size () == 1 ? tr (STR_SESSION_SINGLE) : tr (STR_SESSIONS_PLURAL));
		}
		else
		{
			ImGui::TextColored (ImVec4 (1.0f, 0.65f, 0.15f, 1.0f), "%s", tr (STR_WAITING_WIFI));
			ImGui::SameLine ();
			ImGui::TextDisabled ("%s", tr (STR_NO_CONNECTION));
			ImGui::TextDisabled ("%s", tr (STR_ENABLE_WIFI_HINT));
		}
	}
	ImGui::Separator ();

	ImGui::BeginChild ("Logs", ImVec2 (0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
	drawLog ();

	// Direct D-Pad (3DS only) and L/R scroll controls
	if (!m_showSettings && !m_showHelp && !m_showAbout)
	{
#ifdef __3DS__
		auto const kHeld = hidKeysHeld ();
		if (kHeld & KEY_DUP)
			ImGui::SetScrollY (ImGui::GetScrollY () - 15.0f);
		if (kHeld & KEY_DDOWN)
			ImGui::SetScrollY (ImGui::GetScrollY () + 15.0f);
		if (kHeld & KEY_L)
			ImGui::SetScrollY (ImGui::GetScrollY () - 60.0f);
		if (kHeld & KEY_R)
			ImGui::SetScrollY (ImGui::GetScrollY () + 60.0f);
#else
		if (ImGui::IsKeyDown (ImGuiKey_GamepadL1))
			ImGui::SetScrollY (ImGui::GetScrollY () - ui::px (60.0f));
		if (ImGui::IsKeyDown (ImGuiKey_GamepadR1))
			ImGui::SetScrollY (ImGui::GetScrollY () + ui::px (60.0f));
#endif
	}
	ImGui::EndChild ();

	ImGui::End ();

	// bottom screen
	ImGui::SetNextWindowSize (ImVec2 (width * 0.8f, height * 0.5f));
	ImGui::SetNextWindowPos (ImVec2 (width * 0.1f, height * 0.5f), ImGuiCond_FirstUseEver);
	ImGui::Begin ("Sessions",
	    nullptr,
	    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
	        ImGuiWindowFlags_NoNavFocus);

	// Action buttons bar at the top of bottom screen
	{
		float const availW = ImGui::GetContentRegionAvail ().x;
		float const btnW   = (availW - ui::px (16.0f)) / 3.0f;

		if (ui::badgeButton ("settings", tr (STR_BTN_SETTINGS), "Y", ImVec2 (btnW, ui::px (24.0f))))
		{
			if (m_showSettings)
				closeModals ();
			else
				openSettings ();
		}
		ImGui::SameLine ();
		if (ui::badgeButton ("help", tr (STR_BTN_HELP), "X", ImVec2 (btnW, ui::px (24.0f))))
		{
			if (m_showHelp)
				closeModals ();
			else
				openHelp ();
		}
		ImGui::SameLine ();
		if (ImGui::Button (tr (STR_BTN_SCREENS), ImVec2 (btnW, ui::px (24.0f))))
		{
			platform::toggleBacklight ();
		}
	}

	ImGui::Separator ();

	showMenu ();

	drawSessionCards ();

	ImGui::End ();
#else
	// Switch: a single landscape window. Status line on top, log on the left, transfers on
	// the right and touch-sized action buttons along the bottom edge (thumb reach when
	// handheld). The system status icons and clock are drawn over the top edge.
	ImGui::SetNextWindowPos (ImVec2 (0.0f, 0.0f));
	ImGui::SetNextWindowSize (ImVec2 (width, height));
	ImGui::Begin ("ftpd-EX###ftpd",
	    nullptr,
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
	        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNavFocus);

	// status header
	{
		auto const serverLock = std::scoped_lock (m_lock);
		if (m_socket)
		{
			ImGui::TextColored (ImVec4 (0.2f, 0.85f, 0.45f, 1.0f), "%s", tr (STR_ONLINE));
			ImGui::SameLine ();
			ImGui::TextColored (ImVec4 (1.0f, 1.0f, 1.0f, 1.0f), "ftp://%s", m_name.c_str ());
			ImGui::TextDisabled ("%zu %s  •  " FTPD_VERSION_LABEL,
			    m_sessions.size (),
			    m_sessions.size () == 1 ? tr (STR_SESSION_SINGLE) : tr (STR_SESSIONS_PLURAL));
		}
		else
		{
			ImGui::TextColored (ImVec4 (1.0f, 0.65f, 0.15f, 1.0f), "%s", tr (STR_WAITING_WIFI));
			ImGui::SameLine ();
			ImGui::TextDisabled ("%s", tr (STR_NO_CONNECTION));
			ImGui::TextDisabled ("%s", tr (STR_ENABLE_WIFI_HINT));
		}
	}
	ImGui::Separator ();

	// body between the header and the button bar
	auto const &style   = ImGui::GetStyle ();
	auto const footerH  = ImGui::GetFrameHeight () * 1.7f;
	auto const bodyH    = ImGui::GetContentRegionAvail ().y - footerH - style.ItemSpacing.y;
	auto const spacingX = style.ItemSpacing.x;
	auto const leftW    = (ImGui::GetContentRegionAvail ().x - spacingX) * 0.55f;

	ImGui::BeginChild ("LogPane", ImVec2 (leftW, bodyH), true, ImGuiWindowFlags_HorizontalScrollbar);
	drawLog ();

	// L/R scroll the log
	if (!m_showSettings && !m_showHelp && !m_showAbout)
	{
		if (ImGui::IsKeyDown (ImGuiKey_GamepadL1))
			ImGui::SetScrollY (ImGui::GetScrollY () - ui::px (60.0f));
		if (ImGui::IsKeyDown (ImGuiKey_GamepadR1))
			ImGui::SetScrollY (ImGui::GetScrollY () + ui::px (60.0f));
	}
	ImGui::EndChild ();

	ImGui::SameLine ();

	ImGui::BeginChild ("SessionsPane", ImVec2 (0.0f, bodyH), true);
	drawSessionCards ();
	ImGui::EndChild ();

	// action buttons
	{
		float const btnW = (ImGui::GetContentRegionAvail ().x - 2.0f * spacingX) / 3.0f;

		if (ui::badgeButton ("settings", tr (STR_BTN_SETTINGS), "Y", ImVec2 (btnW, footerH)))
		{
			if (m_showSettings)
				closeModals ();
			else
				openSettings ();
		}
		ImGui::SameLine ();
		if (ui::badgeButton ("help", tr (STR_BTN_HELP), "X", ImVec2 (btnW, footerH)))
		{
			if (m_showHelp)
				closeModals ();
			else
				openHelp ();
		}
		ImGui::SameLine ();
		if (ui::badgeButton ("screens", tr (STR_BTN_SCREENS), "-", ImVec2 (btnW, footerH)))
			platform::toggleBacklight ();
	}

	showMenu ();

	ImGui::End ();
#endif
#else
	ImGui::SetNextWindowSize (ImVec2 (width, height));
	{
		std::array<char, 64> title{};

		{
			auto const serverLock = std::scoped_lock (m_lock);
			std::snprintf (title.data (),
			    title.size (),
			    STATUS_STRING " %s###ftpd",
			    m_socket ? m_name.c_str () : "Waiting for WiFi...");
		}

		ImGui::Begin (title.data (),
		    nullptr,
		    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
		        ImGuiWindowFlags_MenuBar
		);
	}

	showMenu ();

	ImGui::BeginChild (
	    "Logs", ImVec2 (0, 0.5f * height), false, ImGuiWindowFlags_HorizontalScrollbar);
	drawLog ();
	ImGui::EndChild ();

	ImGui::Separator ();

	{
		auto const lock = std::scoped_lock (m_lock);
		for (auto &session : m_sessions)
			session->draw ();
	}

	ImGui::End ();
#endif
#endif
}

bool FtpServer::quit ()
{
#ifndef CLASSIC
	if (updater::isRestartRequested ())
		return true;
#endif
	return m_quit;
}

UniqueFtpServer FtpServer::create ()
{
	updateFreeSpace ();

	auto config = FtpConfig::load (FTPDCONFIG);
	i18n::setLanguage (config->language ());

	return UniqueFtpServer (new FtpServer (std::move (config)));
}

std::string FtpServer::getFreeSpace ()
{
#ifndef __NDS__
	auto const lock = std::scoped_lock (s_lock);
#endif
	return s_freeSpace;
}

void FtpServer::updateFreeSpace ()
{
	statvfs_t st = {};
#if defined(__NDS__) || defined(__3DS__) || defined(__SWITCH__)
	if (::statvfs ("sdmc:/", &st) != 0)
#else
	if (::statvfs ("/", &st) != 0)
#endif
		return;

	auto freeSpace = fs::printSize (static_cast<std::uint64_t> (st.f_bsize) * st.f_bfree);

#ifndef __NDS__
	auto const lock = std::scoped_lock (s_lock);
#endif
	if (freeSpace != s_freeSpace)
		s_freeSpace = std::move (freeSpace);
}

std::time_t FtpServer::startTime ()
{
	return s_startTime;
}

#ifdef __3DS__
int FtpServer::tzOffset ()
{
	return s_tzOffset;
}
#endif

void FtpServer::handleNetworkFound ()
{
	SockAddr addr;
	if (!platform::networkAddress (addr))
		return;

	std::uint16_t port;

	{
#ifndef __NDS__
		auto const lock = m_config->lockGuard ();
#endif
		port = m_config->port ();
	}

	addr.setPort (port);

	auto socket = Socket::create (Socket::eStream);
	if (!socket)
		return;

	if (port != 0 && !socket->setReuseAddress (true))
		return;

	if (!socket->bind (addr))
		return;

	if (!socket->listen (10))
		return;

	auto const &sockName = socket->sockName ();
	auto const name      = sockName.name ();

	m_name.resize (std::strlen (name) + 3 + 5);
	m_name.resize (std::sprintf (m_name.data (), "[%s]:%u", name, sockName.port ()));

	info ("Started server at %s\n", m_name.c_str ());

	LOCKED (m_socket = std::move (socket));

#ifndef __NDS__
	socket = mdns::createSocket ();
	if (!socket)
		return;

	LOCKED (m_mdnsSocket = std::move (socket));
#endif

#ifndef CLASSIC
	updater::setAutoCheck (m_config->checkUpdates ());
	updater::setBeta (m_config->updateBeta ());
	if (m_config->checkUpdates () && updater::getState () == updater::State::Idle)
		updater::checkAuto ();
#endif
}

void FtpServer::handleNetworkLost ()
{
	{
		// destroy sessions
		std::vector<UniqueFtpSession> sessions;
		LOCKED (sessions = std::move (m_sessions));
	}

	{
		UniqueSocket sock;

		// destroy command socket
		LOCKED (sock = std::move (m_socket));

#ifndef __NDS__
		// destroy mDNS socket
		LOCKED (sock = std::move (m_mdnsSocket));
#endif
	}

	info ("Stopped server at %s\n", m_name.c_str ());
}

#ifndef CLASSIC
void FtpServer::drawSessionCards ()
{
	auto const lock = std::scoped_lock (m_lock);
	size_t activeXfers = 0;
	for (auto &session : m_sessions)
	{
		if (session->transferring ())
			++activeXfers;
	}

	if (activeXfers > 0)
	{
		for (auto &session : m_sessions)
		{
			if (session->transferring ())
				session->draw ();
		}

		if (activeXfers < m_sessions.size ())
		{
			auto const idleCount = m_sessions.size () - activeXfers;
			ImGui::Spacing ();
			ImGui::TextDisabled ("  + %zu %s %s",
			    idleCount,
			    idleCount == 1 ? tr (STR_SESSION_SINGLE) : tr (STR_SESSIONS_PLURAL),
			    tr (STR_IDLE));
		}
	}
	else
	{
		ImGui::Spacing ();
		ImGui::BeginChild ("EmptyCard", ImVec2 (0.0f, ui::px (138.0f)), true);
		{
			ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), "  ftpd-EX");
			ImGui::Separator ();
			ImGui::Spacing ();
			if (m_socket)
			{
				ImGui::TextWrapped ("%s", tr (STR_CONNECT_INSTRUCTIONS));
				ImGui::Spacing ();
				ImGui::BulletText ("%s %s", tr (STR_HOST_LABEL), m_name.c_str ());
				ImGui::BulletText ("%s", tr (STR_USER_LABEL));
				ImGui::BulletText (tr (STR_STORAGE_FREE), getFreeSpace ().c_str ());
				ImGui::Spacing ();
				if (m_sessions.empty ())
				{
					ImGui::TextDisabled ("%s", tr (STR_ACTIVE_TRANSFERS_HINT));
				}
				else
				{
					ImGui::TextColored (ImVec4 (0.2f, 0.85f, 0.45f, 1.0f),
					    tr (STR_SESSIONS_IDLE_HINT),
					    m_sessions.size (),
					    m_sessions.size () == 1 ? tr (STR_SESSION_SINGLE) : tr (STR_SESSIONS_PLURAL));
				}
			}
			else
			{
				ImGui::TextColored (ImVec4 (1.0f, 0.7f, 0.2f, 1.0f), "  %s", tr (STR_WAITING_WIFI));
				ImGui::TextDisabled ("  %s", tr (STR_TURN_ON_WIFI_HINT));
			}
		}
		ImGui::EndChild ();
	}
}

void FtpServer::openSettings ()
{
	m_openSettingsRequested = true;
	m_openHelpRequested     = false;
	m_openAboutRequested    = false;
	m_closeModalsRequested  = false;
}

void FtpServer::openHelp ()
{
	if (m_helpSelectedTab < 0)
		m_helpSelectedTab = 0;
	m_openHelpRequested     = true;
	m_openSettingsRequested = false;
	m_openAboutRequested    = false;
	m_closeModalsRequested  = false;
}

void FtpServer::openAbout ()
{
	m_helpSelectedTab       = 2;
	m_openHelpRequested     = true;
	m_openSettingsRequested = false;
	m_openAboutRequested    = false;
	m_closeModalsRequested  = false;
}

void FtpServer::closeModals ()
{
	m_closeModalsRequested  = true;
	m_openSettingsRequested = false;
	m_openHelpRequested     = false;
	m_openAboutRequested    = false;
}

void FtpServer::showMenu ()
{
#if !defined(__3DS__) && !defined(__SWITCH__)
	if (ImGui::BeginMenuBar ())
	{
#if defined(__SWITCH__)
		if (ImGui::BeginMenu ("Menu \xee\x80\x83")) // Y Button
#else
		if (ImGui::BeginMenu ("Menu"))
#endif
		{
			if (ImGui::MenuItem (tr (STR_SETTINGS_TITLE)))
				openSettings ();

			if (ImGui::MenuItem (tr (STR_HELP_TITLE)))
				openHelp ();

			if (ImGui::MenuItem (tr (STR_UPLOAD_LOG)))
				uploadLog ();

			ImGui::Separator ();

			if (ImGui::MenuItem (tr (STR_ABOUT)))
				openAbout ();

			ImGui::Separator ();

			if (ImGui::MenuItem (tr (STR_QUIT)))
				m_quit = true;

			ImGui::EndMenu ();
		}
		ImGui::EndMenuBar ();
	}
#endif

	if (m_closeModalsRequested)
	{
		m_closeModalsRequested = false;
		m_showSettings         = false;
		m_showHelp             = false;
		m_showAbout            = false;
		m_showWhatsNew         = false;
		updater::dismissPrompt ();
		ImGui::CloseCurrentPopup ();
	}

	if (m_openSettingsRequested)
	{
		m_openSettingsRequested = false;
		m_showSettings          = true;
		m_showHelp              = false;
		m_showAbout             = false;
		m_showWhatsNew          = false;

#ifndef __NDS__
		auto const lock = m_config->lockGuard ();
#endif
		m_langSetting = m_config->language ();

		m_userSetting = m_config->user ();
		m_userSetting.resize (32);

		m_passSetting = m_config->pass ();
		m_passSetting.resize (32);

		m_hostnameSetting = m_config->hostname ();
		m_hostnameSetting.resize (32);

		m_portSetting = m_config->port ();

		m_deflateLevelSetting = m_config->deflateLevel ();

		m_checkUpdatesSetting = m_config->checkUpdates ();

#ifdef __3DS__
		m_getMTimeSetting = m_config->getMTime ();
#endif

#ifdef __SWITCH__
		m_enableAPSetting = m_config->enableAP ();

		m_ssidSetting = m_config->ssid ();
		m_ssidSetting.resize (19);

		m_passphraseSetting = m_config->passphrase ();
		m_passphraseSetting.resize (63);
#endif

		auto const title = std::string (tr (STR_SETTINGS_TITLE)) + "###Settings";
		ImGui::OpenPopup (title.c_str ());
	}

	if (m_openHelpRequested)
	{
		m_openHelpRequested = false;
		m_showHelp          = true;
		m_showSettings      = false;
		m_showAbout         = false;
		m_showWhatsNew      = false;

		auto const title = std::string (tr (STR_HELP_TITLE)) + "###Help";
		ImGui::OpenPopup (title.c_str ());
	}

	if (m_showSettings)
		showSettings ();

	if (m_showHelp)
		showHelp ();

	if (m_showWhatsNew)
		showWhatsNew ();

	if (!m_showWhatsNew && !m_showSettings && updater::getPrompt () != updater::Prompt::None)
		showUpdaterPrompt ();
}

void FtpServer::showSettings ()
{
#ifdef __3DS__
	// Inset modal from bottom screen edges (320x240 on bottom screen, X: 40..360, Y: 240..480)
	ImGui::SetNextWindowSize (ImVec2 (304.0f, 224.0f));
	ImGui::SetNextWindowPos (ImVec2 (48.0f, 248.0f));
#else
	auto const &io    = ImGui::GetIO ();
	auto const width  = io.DisplaySize.x;
	auto const height = io.DisplaySize.y;

	ImGui::SetNextWindowSize (ImVec2 (width * 0.85f, height * 0.85f));
	ImGui::SetNextWindowPos (ImVec2 (width * 0.075f, height * 0.075f));
#endif

	ImGui::PushStyleVar (ImGuiStyleVar_WindowBorderSize, 1.5f);
	ImGui::PushStyleVar (ImGuiStyleVar_WindowRounding, 6.0f);
	ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.35f, 0.65f, 0.95f, 0.90f));

	bool const open = ImGui::BeginPopupModal ((std::string (tr (STR_SETTINGS_TITLE)) + "###Settings").c_str (),
	        nullptr,
	        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

	ImGui::PopStyleColor ();
	ImGui::PopStyleVar (2);

	if (open)
	{
		// Top header with prominent red [X] close button
		ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), "%s", tr (STR_SETTINGS_TITLE));
		ImGui::SameLine ();
		float const closeBtnWidth = ui::px (26.0f);
		ImGui::SetCursorPosX (ImGui::GetWindowWidth () - closeBtnWidth - ui::px (8.0f));
		ImGui::SetCursorPosY (ImGui::GetCursorPosY () - ui::px (2.0f));

		ImGui::PushStyleColor (ImGuiCol_Button, ImVec4 (0.75f, 0.20f, 0.20f, 0.85f));
		ImGui::PushStyleColor (ImGuiCol_ButtonHovered, ImVec4 (0.90f, 0.28f, 0.28f, 1.00f));
		ImGui::PushStyleColor (ImGuiCol_ButtonActive, ImVec4 (0.95f, 0.35f, 0.35f, 1.00f));
		bool const closeClicked = ImGui::Button ("X", ImVec2 (closeBtnWidth, ui::px (18.0f)));
		ImGui::PopStyleColor (3);

		if (closeClicked)
		{
			m_showSettings = false;
			i18n::setLanguage (m_config->language ());
			ImGui::CloseCurrentPopup ();
			ImGui::EndPopup ();
			return;
		}

		ImGui::Separator ();
		ImGui::Spacing ();

		// Scrollable form area (leaves 28px for bottom action buttons)
		ImGui::BeginChild ("SettingsForm", ImVec2 (0.0f, -ui::px (28.0f)), false);

		// Language selector
		int currentLang = static_cast<int> (m_langSetting);
		char const *languages[] = {
			i18n::getLanguageName (Language::English),
			i18n::getLanguageName (Language::Spanish),
		};
		if (ImGui::Combo (tr (STR_LANGUAGE), &currentLang, languages, IM_ARRAYSIZE (languages)))
		{
			m_langSetting = static_cast<Language> (currentLang);
			i18n::setLanguage (m_langSetting);
		}

		ImGui::InputText (tr (STR_USER),
		    m_userSetting.data (),
		    m_userSetting.size (),
		    ImGuiInputTextFlags_AutoSelectAll);

		ImGui::InputText (tr (STR_PASS),
		    m_passSetting.data (),
		    m_passSetting.size (),
		    ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_Password);

		ImGui::InputText (tr (STR_HOSTNAME),
		    m_hostnameSetting.data (),
		    m_hostnameSetting.size (),
		    ImGuiInputTextFlags_AutoSelectAll);

		ImGui::InputScalar (tr (STR_PORT),
		    ImGuiDataType_U16,
		    &m_portSetting,
		    nullptr,
		    nullptr,
		    "%u",
		    ImGuiInputTextFlags_AutoSelectAll);

		ImGui::SliderInt (
		    tr (STR_DEFLATE_LEVEL), &m_deflateLevelSetting, Z_NO_COMPRESSION, Z_BEST_COMPRESSION);

		ImGui::Checkbox (tr (STR_CHECK_UPDATES), &m_checkUpdatesSetting);

#ifdef __3DS__
		ImGui::Checkbox (tr (STR_GET_MTIME), &m_getMTimeSetting);
#endif

#ifdef __SWITCH__
		ImGui::Checkbox (tr (STR_ENABLE_AP), &m_enableAPSetting);

		ImGui::InputText ("SSID",
		    m_ssidSetting.data (),
		    m_ssidSetting.size (),
		    ImGuiInputTextFlags_AutoSelectAll);
		auto const ssidError = platform::validateSSID (m_ssidSetting);
		if (ssidError)
			ImGui::TextColored (ImVec4 (1.0f, 0.4f, 0.4f, 1.0f), ssidError);

		ImGui::InputText ("Passphrase",
		    m_passphraseSetting.data (),
		    m_passphraseSetting.size (),
		    ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_Password);
		auto const passphraseError = platform::validatePassphrase (m_passphraseSetting);
		if (passphraseError)
			ImGui::TextColored (ImVec4 (1.0f, 0.4f, 0.4f, 1.0f), passphraseError);
#endif

		ImGui::EndChild ();

		// Bottom action buttons: Save and Reset (Cancel is done via [X] or (B))
		auto const &style = ImGui::GetStyle ();
		auto const btnWidth = (ImGui::GetContentRegionAvail ().x - style.ItemSpacing.x) * 0.5f;

		auto const save  = ImGui::Button (tr (STR_BTN_SAVE), ImVec2 (btnWidth, ui::px (22.0f)));
		ImGui::SameLine ();
		auto const reset = ImGui::Button (tr (STR_BTN_RESET), ImVec2 (btnWidth, ui::px (22.0f)));

		if (save)
		{
			m_showSettings = false;
			ImGui::CloseCurrentPopup ();

#ifndef __NDS__
			auto const lock = m_config->lockGuard ();
#endif

			m_config->setLanguage (m_langSetting);
			m_config->setUser (m_userSetting);
			m_config->setPass (m_passSetting);
			m_config->setHostname (m_hostnameSetting);
			m_config->setPort (m_portSetting);
			m_config->setDeflateLevel (m_deflateLevelSetting);
			m_config->setCheckUpdates (m_checkUpdatesSetting);
			updater::setAutoCheck (m_checkUpdatesSetting);

#ifdef __3DS__
			m_config->setGetMTime (m_getMTimeSetting);
#endif

#ifdef __SWITCH__
			m_config->setEnableAP (m_enableAPSetting);
			m_config->setSSID (m_ssidSetting);
			m_config->setPassphrase (m_passphraseSetting);
			m_apError = false;
#endif

			UniqueSocket socket;
			LOCKED (socket = std::move (m_socket));

			mdns::setHostname (m_hostnameSetting);

			if (!m_config->save (FTPDCONFIG))
				error ("Failed to save config\n");
		}

		if (reset)
		{
			static auto const defaults = FtpConfig::create ();

			m_langSetting         = defaults->language ();
			i18n::setLanguage (m_langSetting);
			m_userSetting         = defaults->user ();
			m_passSetting         = defaults->pass ();
			m_hostnameSetting     = defaults->hostname ();
			m_portSetting         = defaults->port ();
			m_checkUpdatesSetting = defaults->checkUpdates ();
#ifdef __3DS__
			m_getMTimeSetting = defaults->getMTime ();
#endif

#ifdef __SWITCH__
			m_enableAPSetting   = defaults->enableAP ();
			m_ssidSetting       = defaults->ssid ();
			m_passphraseSetting = defaults->passphrase ();
#endif
		}

		ImGui::EndPopup ();
	}
	else
	{
		m_showSettings = false;
		i18n::setLanguage (m_config->language ());
	}
}

void FtpServer::showHelp ()
{
#ifdef __3DS__
	// Inset modal from bottom screen edges (320x240 on bottom screen, X: 40..360, Y: 240..480)
	ImGui::SetNextWindowSize (ImVec2 (304.0f, 224.0f));
	ImGui::SetNextWindowPos (ImVec2 (48.0f, 248.0f));
#else
	auto const &io    = ImGui::GetIO ();
	auto const width  = io.DisplaySize.x;
	auto const height = io.DisplaySize.y;

	ImGui::SetNextWindowSize (ImVec2 (width * 0.85f, height * 0.85f));
	ImGui::SetNextWindowPos (ImVec2 (width * 0.075f, height * 0.075f));
#endif

	ImGui::PushStyleVar (ImGuiStyleVar_WindowBorderSize, 1.5f);
	ImGui::PushStyleVar (ImGuiStyleVar_WindowRounding, 6.0f);
	ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.35f, 0.65f, 0.95f, 0.90f));

	bool const open = ImGui::BeginPopupModal ((std::string (tr (STR_HELP_TITLE)) + "###Help").c_str (),
	        nullptr,
	        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

	ImGui::PopStyleColor ();
	ImGui::PopStyleVar (2);

	if (open)
	{
		// Top header with prominent red [X] close button
		ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), "%s", tr (STR_HELP_TITLE));
		ImGui::SameLine ();
		float const closeBtnWidth = ui::px (26.0f);
		ImGui::SetCursorPosX (ImGui::GetWindowWidth () - closeBtnWidth - ui::px (8.0f));
		ImGui::SetCursorPosY (ImGui::GetCursorPosY () - ui::px (2.0f));

		ImGui::PushStyleColor (ImGuiCol_Button, ImVec4 (0.75f, 0.20f, 0.20f, 0.85f));
		ImGui::PushStyleColor (ImGuiCol_ButtonHovered, ImVec4 (0.90f, 0.28f, 0.28f, 1.00f));
		ImGui::PushStyleColor (ImGuiCol_ButtonActive, ImVec4 (0.95f, 0.35f, 0.35f, 1.00f));
		bool const closeClicked = ImGui::Button ("X", ImVec2 (closeBtnWidth, ui::px (18.0f)));
		ImGui::PopStyleColor (3);

		if (closeClicked)
		{
			m_showHelp = false;
			ImGui::CloseCurrentPopup ();
			ImGui::EndPopup ();
			return;
		}

		ImGui::Separator ();
		ImGui::Spacing ();

		if (ImGui::BeginTabBar ("HelpTabs"))
		{
			if (ImGui::BeginTabItem (tr (STR_HELP_TAB_CONTROLS), nullptr, m_helpSelectedTab == 0 ? ImGuiTabItemFlags_SetSelected : 0))
			{
				ImGui::BeginChild ("ControlsScroll", ImVec2 (0.0f, 0.0f), false);
				ui::controlRow ({"Y"}, tr (STR_HELP_CTRL_Y));
				ui::controlRow ({"X"}, tr (STR_HELP_CTRL_X));
				ui::controlRow ({"B"}, tr (STR_HELP_CTRL_B));
				ui::controlRow ({"A"}, tr (STR_HELP_CTRL_A));
#ifdef __3DS__
				ui::controlRow ({"D-PAD"}, tr (STR_HELP_CTRL_DPAD));
#endif
				ui::controlRow ({"L", "R"}, tr (STR_HELP_CTRL_LR));
#ifdef __SWITCH__
				ui::controlRow ({"-"}, tr (STR_HELP_CTRL_SELECT));
				ui::controlRow ({"+"}, tr (STR_HELP_CTRL_START));
#else
				ui::controlRow ({"SELECT"}, tr (STR_HELP_CTRL_SELECT));
				ui::controlRow ({"START"}, tr (STR_HELP_CTRL_START));
#endif
				ui::controlRow ({"TOUCH"}, tr (STR_HELP_CTRL_TOUCH));
				ImGui::EndChild ();
				ImGui::EndTabItem ();
			}

			if (ImGui::BeginTabItem (tr (STR_HELP_TAB_CONNECT), nullptr, m_helpSelectedTab == 1 ? ImGuiTabItemFlags_SetSelected : 0))
			{
				ImGui::BeginChild ("ConnectScroll", ImVec2 (0.0f, 0.0f), false);
				ImGui::TextWrapped ("%s", tr (STR_HELP_CONNECT_DESC1));
				ImGui::Spacing ();
				ImGui::TextWrapped ("%s", tr (STR_HELP_CONNECT_DESC2));
				ImGui::Spacing ();
				ImGui::TextWrapped ("%s", tr (STR_HELP_CONNECT_DESC3));
				ImGui::Separator ();
				bulletWrapped ("%s %s", tr (STR_HOST_LABEL), m_socket ? m_name.c_str () : tr (STR_NO_CONNECTION));
				bulletWrapped ("%s %u", tr (STR_PORT), m_config->port ());
				bulletWrapped ("%s %s", tr (STR_USER), m_config->user ().empty () ? "anonymous" : m_config->user ().c_str ());
				ImGui::EndChild ();
				ImGui::EndTabItem ();
			}

			if (ImGui::BeginTabItem (tr (STR_HELP_TAB_ABOUT), nullptr, m_helpSelectedTab == 2 ? ImGuiTabItemFlags_SetSelected : 0))
			{
				ImGui::BeginChild ("AboutScroll", ImVec2 (0.0f, 0.0f), false);
				ImGui::TextColored (ImVec4 (0.20f, 0.85f, 0.45f, 1.0f), "ftpd-EX " FTPD_VERSION_LABEL);
				ImGui::TextDisabled ("based on ftpd v" FTPD_UPSTREAM_VERSION);
				ImGui::Spacing ();
				ImGui::TextWrapped ("%s", tr (STR_ABOUT_CREDITS));
				ImGui::Spacing ();

				float const halfWidth = (ImGui::GetContentRegionAvail ().x - ImGui::GetStyle ().ItemSpacing.x) * 0.5f;

				if (ImGui::Button (tr (STR_UPLOAD_LOG), ImVec2 (halfWidth, ui::px (22.0f))))
					uploadLog ();
				ImGui::SameLine ();

				static bool s_logSavedSuccess = false;
				static platform::steady_clock::time_point s_logSavedTime;
				if (ImGui::Button (tr (STR_SAVE_LOG_SD), ImVec2 (halfWidth, ui::px (22.0f))))
				{
					s_logSavedSuccess = saveLog ();
					s_logSavedTime    = platform::steady_clock::now ();
				}
				if (s_logSavedSuccess && platform::steady_clock::now () - s_logSavedTime < 3s)
				{
					ImGui::TextColored (ImVec4 (0.20f, 0.85f, 0.45f, 1.0f), "%s", tr (STR_LOG_SAVED));
				}
				ImGui::Spacing ();

				ImGui::Separator ();
				ImGui::TextColored (ImVec4 (0.40f, 0.75f, 1.0f, 1.0f), "%s", tr (STR_UPDATES_SECTION));
				auto const updState = updater::getState ();
				switch (updState)
				{
				case updater::State::Idle:
				case updater::State::UpToDate:
					ImGui::TextDisabled ("%s", tr (STR_UPDATES_UP_TO_DATE));
					break;
				case updater::State::Checking:
					ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), "%s", tr (STR_UPDATES_CHECKING));
					break;
				case updater::State::Available:
				{
					char buf[64];
					std::snprintf (buf, sizeof (buf), tr (STR_UPDATES_AVAILABLE), updater::getRemoteTag ().c_str ());
					ImGui::TextColored (ImVec4 (0.20f, 0.85f, 0.45f, 1.0f), "%s", buf);
					break;
				}
				case updater::State::Downloading:
					ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), tr (STR_UPDATES_DOWNLOADING), updater::getProgress ());
					ImGui::ProgressBar (updater::getProgress () / 100.0f, ImVec2 (-1.0f, ui::px (14.0f)));
					break;
				case updater::State::Installing:
					ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), "%s", tr (STR_UPDATES_INSTALLING));
					ImGui::ProgressBar (updater::getProgress () / 100.0f, ImVec2 (-1.0f, ui::px (14.0f)));
					break;
				case updater::State::Installed:
					ImGui::TextColored (ImVec4 (0.20f, 0.85f, 0.45f, 1.0f), "%s", tr (STR_UPDATES_RESTART_PROMPT));
					break;
				case updater::State::Error:
				{
					char buf[96];
					std::snprintf (buf, sizeof (buf), tr (STR_UPDATES_ERROR), updater::getMessage ().c_str ());
					ImGui::TextColored (ImVec4 (0.95f, 0.35f, 0.35f, 1.0f), "%s", buf);
					break;
				}
				}
				ImGui::Spacing ();

				// Row 1: Action button + Novedades
				if (updState == updater::State::Available)
				{
					if (ImGui::Button (tr (STR_UPDATES_INSTALL_NOW), ImVec2 (halfWidth, ui::px (22.0f))))
						updater::install ();
				}
				else if (updState == updater::State::Installed)
				{
					if (ImGui::Button (tr (STR_UPDATES_RESTART_NOW), ImVec2 (halfWidth, ui::px (22.0f))))
						updater::restart ();
				}
				else
				{
					ImGui::BeginDisabled (updater::isBusy ());
					if (ImGui::Button (tr (STR_CHECK_FOR_UPDATES_BTN), ImVec2 (halfWidth, ui::px (22.0f))))
						updater::checkNow ();
					ImGui::EndDisabled ();
				}

				ImGui::SameLine ();
				ImGui::BeginDisabled (!updater::hasNotes ());
				if (ImGui::Button (tr (STR_UPDATES_WHATS_NEW), ImVec2 (halfWidth, ui::px (22.0f))))
					m_showWhatsNew = true;
				ImGui::EndDisabled ();
				ImGui::Spacing ();

				// Row 2: Autoactualizar + Canal
				bool autoCheck = m_config->checkUpdates ();
				if (ImGui::Button (autoCheck ? tr (STR_UPDATES_AUTO_ON) : tr (STR_UPDATES_AUTO_OFF), ImVec2 (halfWidth, ui::px (22.0f))))
				{
					autoCheck = !autoCheck;
					m_config->setCheckUpdates (autoCheck);
					m_checkUpdatesSetting = autoCheck;
					updater::setAutoCheck (autoCheck);
				}

				ImGui::SameLine ();
				bool beta = m_config->updateBeta ();
				if (ImGui::Button (beta ? tr (STR_UPDATES_CHANNEL_BETA) : tr (STR_UPDATES_CHANNEL_STABLE), ImVec2 (halfWidth, ui::px (22.0f))))
				{
					beta = !beta;
					m_config->setUpdateBeta (beta);
					updater::setBeta (beta);
				}
				ImGui::Spacing ();

				ImGui::Separator ();
				if (ImGui::TreeNode (tr (STR_SECTION_LICENSES)))
				{
					if (ImGui::TreeNode (g_dearImGuiVersion))
					{
						ImGui::TextWrapped ("%s", g_dearImGuiCopyright);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_mitLicense);
						ImGui::TreePop ();
					}

#if defined(__NDS__)
#elif defined(__3DS__)
					if (ImGui::TreeNode (g_libctruVersion))
					{
						ImGui::TextWrapped ("%s", g_zlibLicense);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_zlibLicense);
						ImGui::TreePop ();
					}

					if (ImGui::TreeNode (g_citro3dVersion))
					{
						ImGui::TextWrapped ("%s", g_citro3dCopyright);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_zlibLicense);
						ImGui::TreePop ();
					}
#elif defined(__SWITCH__)
					if (ImGui::TreeNode (g_libnxVersion))
					{
						ImGui::TextWrapped ("%s", g_libnxCopyright);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_libnxLicense);
						ImGui::TreePop ();
					}

					if (ImGui::TreeNode (g_deko3dVersion))
					{
						ImGui::TextWrapped ("%s", g_deko3dCopyright);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_zlibLicense);
						ImGui::TreePop ();
					}

					if (ImGui::TreeNode (g_zstdVersion))
					{
						ImGui::TextWrapped ("%s", g_zstdCopyright);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_zstdLicense);
						ImGui::TreePop ();
					}
#else
					if (ImGui::TreeNode (g_glfwVersion))
					{
						ImGui::TextWrapped ("%s", g_glfwCopyright);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_zlibLicense);
						ImGui::TreePop ();
					}
#endif

#if defined(__NDS__) || defined(__3DS__) || defined(__SWITCH__)
					if (ImGui::TreeNode (g_globVersion))
					{
						ImGui::TextWrapped ("%s", g_globCopyright);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_globLicense);
						ImGui::TreePop ();
					}

					if (ImGui::TreeNode (g_collateVersion))
					{
						ImGui::TextWrapped ("%s", g_collateCopyright);
						ImGui::Separator ();
						ImGui::TextWrapped ("%s", g_collateLicense);
						ImGui::TreePop ();
					}
#endif
					ImGui::TreePop ();
				}

				ImGui::EndChild ();
				ImGui::EndTabItem ();
			}

			// Clear selected tab trigger after processing
			m_helpSelectedTab = -1;

			ImGui::EndTabBar ();
		}

		ImGui::EndPopup ();
	}
	else
	{
		m_showHelp = false;
	}
}

void FtpServer::showAbout ()
{
	openAbout ();
}

void FtpServer::showWhatsNew ()
{
#ifdef __3DS__
	ImGui::SetNextWindowSize (ImVec2 (304.0f, 224.0f));
	ImGui::SetNextWindowPos (ImVec2 (48.0f, 248.0f));
#else
	auto const &io    = ImGui::GetIO ();
	auto const width  = io.DisplaySize.x;
	auto const height = io.DisplaySize.y;

	ImGui::SetNextWindowSize (ImVec2 (width * 0.85f, height * 0.85f));
	ImGui::SetNextWindowPos (ImVec2 (width * 0.075f, height * 0.075f));
#endif

	ImGui::PushStyleVar (ImGuiStyleVar_WindowBorderSize, 1.5f);
	ImGui::PushStyleVar (ImGuiStyleVar_WindowRounding, 6.0f);
	ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.35f, 0.65f, 0.95f, 0.90f));

	std::string const title = std::string (tr (STR_UPDATES_WHATS_NEW)) + "###WhatsNew";
	ImGui::OpenPopup (title.c_str ());

	bool const open = ImGui::BeginPopupModal (title.c_str (),
	        nullptr,
	        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

	ImGui::PopStyleColor ();
	ImGui::PopStyleVar (2);

	if (open)
	{
		ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), "%s", tr (STR_UPDATES_WHATS_NEW));
		ImGui::SameLine ();
		float const closeBtnWidth = ui::px (26.0f);
		ImGui::SetCursorPosX (ImGui::GetWindowWidth () - closeBtnWidth - ui::px (8.0f));
		ImGui::SetCursorPosY (ImGui::GetCursorPosY () - ui::px (2.0f));

		ImGui::PushStyleColor (ImGuiCol_Button, ImVec4 (0.75f, 0.20f, 0.20f, 0.85f));
		ImGui::PushStyleColor (ImGuiCol_ButtonHovered, ImVec4 (0.90f, 0.28f, 0.28f, 1.00f));
		ImGui::PushStyleColor (ImGuiCol_ButtonActive, ImVec4 (0.95f, 0.35f, 0.35f, 1.00f));
		bool const closeClicked = ImGui::Button ("X", ImVec2 (closeBtnWidth, ui::px (18.0f)));
		ImGui::PopStyleColor (3);

		if (closeClicked)
		{
			m_showWhatsNew = false;
			ImGui::CloseCurrentPopup ();
			ImGui::EndPopup ();
			return;
		}

		ImGui::Separator ();
		ImGui::Spacing ();

		auto const notes = updater::getNotes ();
		ImGui::BeginChild ("NotesScroll", ImVec2 (0.0f, ImGui::GetContentRegionAvail ().y - ui::px (28.0f)), true);
		if (notes.empty ())
		{
			ImGui::TextDisabled ("%s", tr (STR_UPDATES_NO_NOTES));
		}
		else
		{
			std::string_view remaining (notes);
			while (!remaining.empty ())
			{
				auto const pos = remaining.find ('\n');
				auto const line = (pos == std::string_view::npos) ? remaining : remaining.substr (0, pos);
				remaining       = (pos == std::string_view::npos) ? std::string_view () : remaining.substr (pos + 1);

				if (line.starts_with ("== "))
				{
					ImGui::Spacing ();
					ImGui::TextColored (ImVec4 (1.0f, 0.85f, 0.20f, 1.0f), "%.*s", static_cast<int> (line.size ()), line.data ());
				}
				else if (!line.empty ())
				{
					ImGui::TextWrapped ("%.*s", static_cast<int> (line.size ()), line.data ());
				}
			}
		}
		ImGui::EndChild ();

		if (ImGui::Button (tr (STR_UPDATES_CLOSE), ImVec2 (-1.0f, ui::px (22.0f))))
		{
			m_showWhatsNew = false;
			ImGui::CloseCurrentPopup ();
		}

		ImGui::EndPopup ();
	}
	else
	{
		m_showWhatsNew = false;
	}
}

void FtpServer::showUpdaterPrompt ()
{
	auto const prompt = updater::getPrompt ();
	if (prompt == updater::Prompt::None)
		return;

#ifdef __3DS__
	ImGui::SetNextWindowSize (ImVec2 (304.0f, 180.0f));
	ImGui::SetNextWindowPos (ImVec2 (48.0f, 270.0f));
#else
	auto const &io    = ImGui::GetIO ();
	auto const width  = io.DisplaySize.x;
	auto const height = io.DisplaySize.y;

	ImGui::SetNextWindowSize (ImVec2 (width * 0.65f, height * 0.40f));
	ImGui::SetNextWindowPos (ImVec2 (width * 0.175f, height * 0.30f));
#endif

	ImGui::PushStyleVar (ImGuiStyleVar_WindowBorderSize, 1.5f);
	ImGui::PushStyleVar (ImGuiStyleVar_WindowRounding, 6.0f);
	ImGui::PushStyleColor (ImGuiCol_Border, ImVec4 (0.35f, 0.65f, 0.95f, 0.90f));

	char const *popupId = "UpdaterPromptModal###Prompt";
	ImGui::OpenPopup (popupId);

	bool const open = ImGui::BeginPopupModal (popupId,
	        nullptr,
	        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

	ImGui::PopStyleColor ();
	ImGui::PopStyleVar (2);

	if (open)
	{
		switch (prompt)
		{
		case updater::Prompt::AskInstall:
		{
			ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), "%s", tr (STR_UPDATES_SECTION));
			ImGui::Separator ();
			ImGui::Spacing ();

			char buf[64];
			std::snprintf (buf, sizeof (buf), tr (STR_UPDATES_AVAILABLE), updater::getRemoteTag ().c_str ());
			ImGui::TextWrapped ("%s", buf);
			ImGui::TextDisabled ("%s -> %s", FTPD_VERSION_LABEL, updater::getRemoteTag ().c_str ());
			ImGui::Spacing ();

			auto const &style = ImGui::GetStyle ();
			float const availWidth = ImGui::GetContentRegionAvail ().x;
			float const halfWidth  = (availWidth - style.ItemSpacing.x) * 0.5f;

			// Row 1: Actualizar ahora + Mas tarde
			if (ImGui::Button (tr (STR_UPDATES_INSTALL_NOW), ImVec2 (halfWidth, ui::px (22.0f))))
			{
				updater::answerPrompt (true);
			}
			ImGui::SameLine ();
			if (ImGui::Button (tr (STR_UPDATES_LATER), ImVec2 (halfWidth, ui::px (22.0f))))
			{
				updater::answerPrompt (false);
				ImGui::CloseCurrentPopup ();
			}

			// Row 2: Novedades centered below
			ImGui::Spacing ();
			float const notesWidth = halfWidth;
			float const offsetX    = (availWidth - notesWidth) * 0.5f;
			if (offsetX > 0.0f)
				ImGui::SetCursorPosX (ImGui::GetCursorPosX () + offsetX);

			if (ImGui::Button (tr (STR_UPDATES_WHATS_NEW), ImVec2 (notesWidth, ui::px (22.0f))))
			{
				m_showWhatsNew = true;
			}
			break;
		}
		case updater::Prompt::Progress:
		{
			ImGui::TextColored (ImVec4 (0.35f, 0.75f, 1.0f, 1.0f), "%s", tr (STR_UPDATES_SECTION));
			ImGui::Separator ();
			ImGui::Spacing ();

			auto const state = updater::getState ();
			if (state == updater::State::Downloading)
			{
				ImGui::Text (tr (STR_UPDATES_DOWNLOADING), updater::getProgress ());
			}
			else
			{
				ImGui::Text ("%s", tr (STR_UPDATES_INSTALLING));
			}
			ImGui::Spacing ();
			ImGui::ProgressBar (updater::getProgress () / 100.0f, ImVec2 (-1.0f, ui::px (16.0f)));
			break;
		}
		case updater::Prompt::AskRestart:
		{
			ImGui::TextColored (ImVec4 (0.20f, 0.85f, 0.45f, 1.0f), "%s", tr (STR_UPDATES_SECTION));
			ImGui::Separator ();
			ImGui::Spacing ();

			ImGui::TextWrapped ("%s", tr (STR_UPDATES_RESTART_PROMPT));
			ImGui::Spacing ();

			auto const &style = ImGui::GetStyle ();
			float const btnWidth = (ImGui::GetContentRegionAvail ().x - style.ItemSpacing.x) * 0.5f;

			if (ImGui::Button (tr (STR_UPDATES_RESTART_NOW), ImVec2 (btnWidth, ui::px (22.0f))))
			{
				updater::answerPrompt (true);
				ImGui::CloseCurrentPopup ();
			}
			ImGui::SameLine ();
			if (ImGui::Button (tr (STR_UPDATES_LATER), ImVec2 (btnWidth, ui::px (22.0f))))
			{
				updater::answerPrompt (false);
				ImGui::CloseCurrentPopup ();
			}
			break;
		}
		case updater::Prompt::Error:
		{
			ImGui::TextColored (ImVec4 (0.95f, 0.35f, 0.35f, 1.0f), "%s", tr (STR_UPDATES_SECTION));
			ImGui::Separator ();
			ImGui::Spacing ();

			char buf[96];
			std::snprintf (buf, sizeof (buf), tr (STR_UPDATES_ERROR), updater::getMessage ().c_str ());
			ImGui::TextWrapped ("%s", buf);
			ImGui::Spacing ();

			if (ImGui::Button (tr (STR_BTN_OK), ImVec2 (-1.0f, ui::px (22.0f))))
			{
				updater::answerPrompt (false);
				ImGui::CloseCurrentPopup ();
			}
			break;
		}
		default:
			break;
		}

		ImGui::EndPopup ();
	}
}

void FtpServer::uploadLog ()
{
#ifndef __NDS__
	auto const lock = std::scoped_lock (m_lock);
#endif
	if (!m_uploadLogCurlM)
		m_uploadLogCurlM = curl_multi_init ();

	if (m_uploadLogCurlM && !m_uploadLogCurl.load (std::memory_order_relaxed))
	{
		m_uploadLogData = getLog ();

		auto const handle = curl_easy_init ();

#ifndef NDEBUG
		curl_easy_setopt (handle, CURLOPT_DEBUGFUNCTION, &curlDebug);
		curl_easy_setopt (handle, CURLOPT_DEBUGDATA, nullptr);
		curl_easy_setopt (handle, CURLOPT_VERBOSE, 1L);
#endif

		// write result into string
		m_uploadLogResult.clear ();
		curl_easy_setopt (handle, CURLOPT_WRITEFUNCTION, &curlCallback);
		curl_easy_setopt (handle, CURLOPT_WRITEDATA, &m_uploadLogResult);

		// set headers
		static char contentType[]       = "Content-Type: text/plain";
		static curl_slist const headers = {contentType, nullptr};
		curl_easy_setopt (handle, CURLOPT_URL, "https://pastie.io/documents");
		curl_easy_setopt (handle, CURLOPT_HTTPHEADER, &headers);

		// set form data
		auto const mime = curl_mime_init (handle);
		auto const part = curl_mime_addpart (mime);
		curl_mime_name (part, "data");
		curl_mime_data (part, m_uploadLogData.data (), m_uploadLogData.size ());
		curl_easy_setopt (handle, CURLOPT_MIMEPOST, mime);

		// add to multi handle
		curl_multi_add_handle (m_uploadLogCurlM, handle);

		// signal network thread to process
		m_uploadLogMime = mime;
		m_uploadLogCurl.store (handle, std::memory_order_relaxed);
	}
}
#endif

void FtpServer::loop ()
{
	if (!m_socket)
	{
#ifndef CLASSIC
#ifdef __SWITCH__
		if (!m_apError)
		{
			bool enable;
			std::string ssid;
			std::string passphrase;

			{
				auto const lock = m_config->lockGuard ();
				enable          = m_config->enableAP ();
				ssid            = m_config->ssid ();
				passphrase      = m_config->passphrase ();
			}

			m_apError = !platform::enableAP (enable, ssid, passphrase);
		}
#endif
#endif

		if (platform::networkVisible ())
			handleNetworkFound ();
	}
	else if (!platform::networkVisible ())
	{
		handleNetworkLost ();
		return;
	}

#ifndef CLASSIC
	{
		auto const lock = std::scoped_lock (m_lock);
		if (m_uploadLogCurl.load (std::memory_order_relaxed))
		{
			int busy      = 0;
			auto const mc = curl_multi_perform (m_uploadLogCurlM, &busy);
			if (mc != CURLM_OK)
			{
				error ("curl_multi_perform: %d %s\n", mc, curl_multi_strerror (mc));

				curl_multi_remove_handle (m_uploadLogCurlM, m_uploadLogCurl);
				curl_easy_cleanup (m_uploadLogCurl);
				curl_mime_free (m_uploadLogMime);
				m_uploadLogMime = nullptr;
				m_uploadLogCurl.store (nullptr, std::memory_order_relaxed);
			}
			else
			{
				int count      = 0;
				auto const msg = curl_multi_info_read (m_uploadLogCurlM, &count);
				if (msg && msg->msg == CURLMSG_DONE && msg->easy_handle == m_uploadLogCurl)
				{
					if (msg->data.result != CURLE_OK)
						info ("cURL finished with status %d\n", msg->data.result);

					json_error_t err;
					auto const root = json_loads (m_uploadLogResult.c_str (), 0, &err);
					if (json_is_object (root))
					{
						auto const key = json_object_get (root, "key");
						if (json_is_string (key))
							info (
							    "Log uploaded to https://pastie.io/%s\n", json_string_value (key));
					}
					else
						error ("Failed to upload log\n");

					json_decref (root);

					curl_multi_remove_handle (m_uploadLogCurlM, m_uploadLogCurl);
					curl_easy_cleanup (m_uploadLogCurl);
					curl_mime_free (m_uploadLogMime);
					m_uploadLogMime = nullptr;
					m_uploadLogCurl.store (nullptr, std::memory_order_relaxed);
				}
			}
		}
	}
#endif

	// poll listen socket
	if (m_socket)
	{
		Socket::PollInfo info{*m_socket, POLLIN, 0};
		auto const rc = Socket::poll (&info, 1, 0ms);
		if (rc < 0)
		{
			handleNetworkLost ();
			return;
		}

		if (rc > 0 && (info.revents & POLLIN))
		{
			auto socket = m_socket->accept ();
			if (!socket)
			{
				// Handles might be exhausted by pending close sockets. Reclaim and retry.
				FtpSession::clearPendingCloseSockets (m_sessions);
				socket = m_socket->accept ();
			}

			if (socket)
			{
				auto session = FtpSession::create (*m_config, std::move (socket));
				LOCKED (m_sessions.emplace_back (std::move (session)));
			}
		}
	}

#ifndef __NDS__
	// poll mDNS socket
	if (m_socket && m_mdnsSocket)
		mdns::handleSocket (m_mdnsSocket.get (), m_socket->sockName ());
#endif

	{
		std::vector<UniqueFtpSession> deadSessions;
		{
			// remove dead sessions
#ifndef __NDS__
			auto const lock = std::scoped_lock (m_lock);
#endif
			auto it = std::begin (m_sessions);
			while (it != std::end (m_sessions))
			{
				auto &session = *it;
				if (session->dead ())
				{
					deadSessions.emplace_back (std::move (session));
					it = m_sessions.erase (it);
				}
				else
					++it;
			}
		}
	}

	// poll sessions
	if (!m_sessions.empty ())
	{
		if (!FtpSession::poll (m_sessions))
			handleNetworkLost ();
	}
#ifndef __NDS__
	// avoid busy polling in background thread
	else
		platform::Thread::sleep (16ms);
#endif
}

void FtpServer::threadFunc ()
{
	while (!m_quit)
		loop ();
}
