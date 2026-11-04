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

#include "log.h"

#include "platform.h"

#ifndef CLASSIC
#include <imgui.h>
#endif

#include <sys/stat.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#ifndef FTPDLOG
#define FTPDLOG "ftpd.log"
#endif

namespace
{
#ifdef __3DS__
/// \brief Maximum number of log messages to keep
constexpr auto MAX_LOGS = 250;
#else
/// \brief Maximum number of log messages to keep
constexpr auto MAX_LOGS = 10000;
#endif

/// \brief Resolve a log path to one that does not depend on the process's current directory
/// \param path_ Log path
/// \note On the 3DS/Switch a path starting with '/' is resolved through the cwd's device, and the
///       cwd is process-wide (the session threads chdir). Pin it to the SD card instead.
std::string resolveLogPath (std::string_view const path_)
{
#if defined(__3DS__) || defined(__SWITCH__)
	if (path_.starts_with ('/'))
		return "sdmc:" + std::string (path_);
#endif
	return std::string (path_);
}

bool mkdirParent (std::string_view const path_)
{
	// skip the device part ("sdmc:"), which cannot be created
	std::size_t start = 0;
	if (auto const colon = path_.find (':'); colon != std::string_view::npos
	                                         && colon < path_.find_first_of ('/'))
		start = colon + 1;

	auto pos = path_.find_first_of ('/', start);
	while (pos != std::string_view::npos)
	{
		auto const dir = std::string (path_.substr (0, pos));
		// nothing before the slash (root), or only the device
		if (pos > start)
		{
			struct stat st {};
			if (::stat (dir.c_str (), &st) != 0)
			{
				// libctru's mkdir indexes the device table with FindDevice's result without
				// checking it, so a path whose device is not found (stat: ENODEV) crashes it
				if (errno != ENOENT)
					return false;

				if (::mkdir (dir.c_str (), 0777) != 0 && errno != EEXIST)
					return false;
			}
		}
		pos = path_.find_first_of ('/', pos + 1);
	}
	return true;
}

void appendErrorToLogFile (std::string_view const msg_)
{
	auto const path = resolveLogPath (FTPDLOG);
	if (!mkdirParent (path))
		return;

	FILE *const fp = std::fopen (path.c_str (), "a");
	if (!fp)
		return;

	std::fprintf (fp, "[ERROR] %.*s", static_cast<int> (msg_.size ()), msg_.data ());
	std::fclose (fp);
}

#ifdef CLASSIC
bool s_logUpdated = true;
#endif

/// \brief Message prefix
static char const *const s_prefix[] = {
    [DEBUG]    = "[DEBUG]",
    [INFO]     = "[INFO]",
    [ERROR]    = "[ERROR]",
    [COMMAND]  = "[COMMAND]",
    [RESPONSE] = "[RESPONSE]",
};

/// \brief Log message
struct Message
{
	/// \brief Parameterized constructor
	/// \param level_ Log level
	/// \param message_ Log message
	Message (LogLevel const level_, std::string message_)
	    : level (level_), message (std::move (message_))
	{
	}

	/// \brief Log level
	LogLevel level;
	/// \brief Log message
	std::string message;
};

/// \brief Log messages
std::vector<Message> s_messages;

#ifndef __NDS__
/// \brief Log lock
platform::Mutex s_lock;
#endif
}

void drawLog ()
{
#ifndef __NDS__
	auto const lock = std::scoped_lock (s_lock);
#endif

#ifdef CLASSIC
	if (!s_logUpdated)
		return;

	s_logUpdated = false;
#endif

	auto const maxLogs =
#ifdef CLASSIC
	    g_logConsole.windowHeight;
#else
	    MAX_LOGS;
#endif

	if (s_messages.size () > static_cast<unsigned> (maxLogs))
	{
		auto const begin = std::begin (s_messages);
		auto const end   = std::next (begin, s_messages.size () - maxLogs);
		s_messages.erase (begin, end);
	}

#ifdef CLASSIC
	char const *const s_colors[] = {
	    [DEBUG]    = "\x1b[33;1m", // yellow
	    [INFO]     = "\x1b[37;1m", // white
	    [ERROR]    = "\x1b[31;1m", // red
	    [COMMAND]  = "\x1b[32;1m", // green
	    [RESPONSE] = "\x1b[36;1m", // cyan
	};

	auto it = std::begin (s_messages);
	if (s_messages.size () > static_cast<unsigned> (g_logConsole.windowHeight))
		it = std::next (it, s_messages.size () - g_logConsole.windowHeight);

	consoleSelect (&g_logConsole);
	while (it != std::end (s_messages))
	{
		std::fputs (s_colors[it->level], stdout);
		std::fputs (it->message.c_str (), stdout);
		++it;
	}
	std::fflush (stdout);
	s_messages.clear ();
#else
	ImVec4 const s_colors[] = {
	    [DEBUG]    = ImVec4 (1.0f, 1.0f, 0.4f, 1.0f),          // yellow
	    [INFO]     = ImGui::GetStyleColorVec4 (ImGuiCol_Text), // normal
	    [ERROR]    = ImVec4 (1.0f, 0.4f, 0.4f, 1.0f),          // red
	    [COMMAND]  = ImVec4 (0.4f, 1.0f, 0.4f, 1.0f),          // green
	    [RESPONSE] = ImVec4 (0.4f, 1.0f, 1.0f, 1.0f),          // cyan
	};

	for (auto const &message : s_messages)
	{
		ImGui::PushStyleColor (ImGuiCol_Text, s_colors[message.level]);
		ImGui::TextUnformatted (s_prefix[message.level]);
		ImGui::SameLine ();
		ImGui::TextUnformatted (message.message.c_str ());
		ImGui::PopStyleColor ();
	}

	// auto-scroll if scroll bar is at end
	if (ImGui::GetScrollY () >= ImGui::GetScrollMaxY ())
		ImGui::SetScrollHereY (1.0f);
#endif
}

#ifndef CLASSIC
std::string getLog ()
{
#ifndef __NDS__
	auto const lock = std::scoped_lock (s_lock);
#endif

	if (s_messages.empty ())
		return {};

	std::vector<Message const *> stack;
	stack.reserve (s_messages.size ());

	std::size_t size = 0;
	for (auto const &msg : s_messages | std::views::reverse)
	{
		auto const lineLen = std::strlen (s_prefix[msg.level]) + 1 + msg.message.size ();
		if (size + lineLen > 1024 * 1024)
			break;

		size += lineLen;
		stack.emplace_back (&msg);
	}

	std::string log;
	log.reserve (size);

	for (auto const &msg : stack | std::views::reverse)
	{
		log += s_prefix[msg->level];
		log += ' ';
		log += msg->message;
	}

	return log;
}
#endif

bool saveLog (char const *const path_)
{
	auto const logPath = resolveLogPath ((path_ && *path_) ? path_ : FTPDLOG);

	if (!mkdirParent (logPath))
		return false;

	FILE *const fp = std::fopen (logPath.c_str (), "w");
	if (!fp)
		return false;

#ifndef __NDS__
	auto const lock = std::scoped_lock (s_lock);
#endif

	for (auto const &msg : s_messages)
	{
		std::fprintf (fp, "%s %s", s_prefix[msg.level], msg.message.c_str ());
	}

	std::fclose (fp);
	return true;
}

void debug (char const *const fmt_, ...)
{
#ifndef NDEBUG
	va_list ap;

	va_start (ap, fmt_);
	addLog (DEBUG, fmt_, ap);
	va_end (ap);
#else
	(void)fmt_;
#endif
}

void info (char const *const fmt_, ...)
{
	va_list ap;

	va_start (ap, fmt_);
	addLog (INFO, fmt_, ap);
	va_end (ap);
}

void error (char const *const fmt_, ...)
{
	va_list ap;

	va_start (ap, fmt_);
	addLog (ERROR, fmt_, ap);
	va_end (ap);
}

void command (char const *const fmt_, ...)
{
	va_list ap;

	va_start (ap, fmt_);
	addLog (COMMAND, fmt_, ap);
	va_end (ap);
}

void response (char const *const fmt_, ...)
{
	va_list ap;

	va_start (ap, fmt_);
	addLog (RESPONSE, fmt_, ap);
	va_end (ap);
}

void addLog (LogLevel const level_, char const *const fmt_, va_list ap_)
{
#ifdef NDEBUG
	if (level_ == DEBUG)
		return;
#endif

#ifndef __NDS__
	thread_local
#endif
	    static char buffer[1024];

	std::vsnprintf (buffer, sizeof (buffer), fmt_, ap_);

	if (level_ == ERROR)
		appendErrorToLogFile (buffer);

#ifndef __NDS__
	auto const lock = std::scoped_lock (s_lock);
#endif
#ifndef NDEBUG
	// std::fprintf (stderr, "%s", s_prefix[level_]);
	// std::fputs (buffer, stderr);
#endif
	s_messages.emplace_back (level_, buffer);
#ifdef CLASSIC
	s_logUpdated = true;
#endif
}

void addLog (LogLevel const level_, std::string_view const message_)
{
#ifdef NDEBUG
	if (level_ == DEBUG)
		return;
#endif

	auto msg = std::string (message_);
	for (auto &c : msg)
	{
		// replace nul-characters with ? to avoid truncation
		if (c == '\0')
			c = '?';
	}

	if (level_ == ERROR)
		appendErrorToLogFile (msg);

#ifndef __NDS__
	auto const lock = std::scoped_lock (s_lock);
#endif
#ifndef NDEBUG
	// std::fprintf (stderr, "%s", s_prefix[level_]);
	// std::fwrite (msg.data (), 1, msg.size (), stderr);
#endif
	s_messages.emplace_back (level_, msg);
#ifdef CLASSIC
	s_logUpdated = true;
#endif
}
