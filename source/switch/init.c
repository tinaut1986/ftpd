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

#include <switch.h>

#include <unistd.h>

#ifndef NDEBUG
/// \brief nxlink socket fd
static int s_fd = -1;
#endif

/// \brief Socket initialization configuration
///
/// The default buffer sizes are taken from the socket pool for every socket, including
/// command and PASV listening sockets. Clients such as KDE's KIO open one data connection
/// per file when refreshing a folder, and with 1 MiB defaults a few dozen of them (plus
/// the ones still closing) exhausted the pool: socket() failed with ENOBUFS until they
/// expired. Data sockets set their own size (FtpSession::SOCK_BUFFERSIZE) anyway.
static SocketInitConfig const s_socketInitConfig = {
    .tcp_tx_buf_size     = 128 * 1024,
    .tcp_rx_buf_size     = 128 * 1024,
    .tcp_tx_buf_max_size = 4 * 1024 * 1024,
    .tcp_rx_buf_max_size = 4 * 1024 * 1024,

    .udp_tx_buf_size = 0x2400,
    .udp_rx_buf_size = 0xA500,

    .sb_efficiency = 8,

    .num_bsd_sessions = 2,
    .bsd_service_type = BsdServiceType_User,
};

/// \brief Smaller socket pool for applet mode, where the heap is much smaller.
///
/// The pool is sb_efficiency * (tx max + rx max + udp), ~16 MiB here (64 MiB in full
/// mode). A burst of per-file listings needs ~128 KiB per data socket (they are set to
/// FtpSession::SOCK_BUFFERSIZE), so a 4x / ~8 MiB pool ran out after ~60 of them.
static SocketInitConfig const s_appletSocketInitConfig = {
    .tcp_tx_buf_size     = 32 * 1024,
    .tcp_rx_buf_size     = 32 * 1024,
    .tcp_tx_buf_max_size = 1 * 1024 * 1024,
    .tcp_rx_buf_max_size = 1 * 1024 * 1024,

    .udp_tx_buf_size = 0x2400,
    .udp_rx_buf_size = 0xA500,

    .sb_efficiency = 8,

    .num_bsd_sessions = 2,
    .bsd_service_type = BsdServiceType_User,
};

/// \brief Number of FS sessions
u32 __nx_fs_num_sessions = 1;

/// \brief Called before main ()
void userAppInit ()
{
	// disable immediate app close
	appletLockExit ();
	// disable auto-sleep
	appletSetAutoSleepDisabled (true);

	romfsInit ();
	plInitialize (PlServiceType_User);
	psmInitialize ();
	nifmInitialize (NifmServiceType_User);
	// applet mode (e.g. launched from the album) only gets a fraction of the memory
	AppletType const type = appletGetAppletType ();
	bool const applet = type != AppletType_Application && type != AppletType_SystemApplication;
	socketInitialize (applet ? &s_appletSocketInitConfig : &s_socketInitConfig);

#ifndef NDEBUG
	// s_fd = nxlinkStdioForDebug ();
#endif
}

void userAppExit ()
{
#ifndef NDEBUG
	if (s_fd >= 0)
	{
		close (s_fd);
		s_fd = -1;
	}
#endif

	socketExit ();
	nifmExit ();
	psmExit ();
	plExit ();
	romfsExit ();
	// Restore auto-sleep
	appletSetAutoSleepDisabled (false);
	appletUnlockExit ();
}
