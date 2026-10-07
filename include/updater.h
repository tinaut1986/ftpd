#pragma once

#include <cstddef>
#include <string>

#ifndef FTPD_VERSION_LABEL
#if defined(FTPD_IS_BETA) && FTPD_IS_BETA
#define FTPD_VERSION_LABEL FTPD_VERSION_STRING " BETA"
#else
#define FTPD_VERSION_LABEL FTPD_VERSION_STRING
#endif
#endif

namespace updater
{

enum class State
{
	Idle,
	Checking,
	Available,
	UpToDate,
	Downloading,
	Installing,
	Installed,
	Error
};

enum class Prompt
{
	None,
	AskInstall,
	Progress,
	AskRestart,
	Error
};

/// \brief Initialize the updater subsystem
/// \param argv0_ Binary path passed to main() (used for Switch NRO / 3DS 3DSX updating)
/// \param autoCheck_ Whether to trigger a check when network is ready
void init (char const *argv0_ = nullptr, bool autoCheck_ = true);

/// \brief Trigger an immediate check for updates in the background
void checkNow ();

/// \brief Trigger an automatic check for updates (raises modal prompt if update found)
void checkAuto ();

/// \brief Whether the current build is a Beta channel build
bool isBetaBuild ();

/// \brief Start downloading and installing the available update
void install ();

/// \brief Restart into the updated binary
void restart ();

/// \brief Respond to the currently active prompt
/// \param yes_ True for affirmative (Install / Restart), false to dismiss/cancel
void answerPrompt (bool yes_);

/// \brief Dismiss the active prompt
void dismissPrompt ();

/// \brief Get current updater state
State getState ();

/// \brief Get current active prompt
Prompt getPrompt ();

/// \brief Get progress percentage (0..100)
int getProgress ();

/// \brief Get remote release tag (e.g. "v1.2.2-EX")
std::string getRemoteTag ();

/// \brief Get status or error message
std::string getMessage ();

/// \brief Get formatted release notes for "What's New"
std::string getNotes ();

/// \brief Whether release notes are currently loaded
bool hasNotes ();

/// \brief Whether auto-check on startup is enabled
bool getAutoCheck ();

/// \brief Enable or disable auto-check on startup
void setAutoCheck (bool enabled_);

/// \brief Whether to include beta / pre-releases
bool getBeta ();

/// \brief Enable or disable beta channel
void setBeta (bool enabled_);

/// \brief Whether the updater is currently executing a background job
bool isBusy ();

/// \brief Whether running in homebrew loader mode (3DSX on 3DS, NRO on Switch)
bool isHomebrewMode ();

/// \brief Whether restart / chainloading is supported on current platform
bool restartSupported ();

/// \brief Whether a restart was requested by the user
bool isRestartRequested ();

} // namespace updater
