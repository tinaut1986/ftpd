#include "updater.h"

#include "log.h"
#include "platform.h"
#include "i18n.h"

#include <curl/curl.h>

#ifdef __3DS__
#include <3ds.h>
#endif

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace
{

#ifndef FTPD_VERSION_STRING
#define FTPD_VERSION_STRING "v1.3.1-EX"
#endif

#ifndef FTPD_IS_BETA
#define FTPD_IS_BETA 0
#endif

constexpr bool kIsBetaBuild = (FTPD_IS_BETA != 0);

constexpr auto UPDATER_DEFAULT_URL = "https://api.github.com/repos/tinaut1986/ftpd/releases?per_page=8";
constexpr auto UPDATER_JSON_MAX    = 192 * 1024;
constexpr auto UPDATER_CHUNK       = 64 * 1024;
constexpr auto UPDATER_NOTES_MAX   = 8192;
constexpr auto UPDATER_FILE_BUF    = 256 * 1024;

#ifdef __3DS__
constexpr auto UPDATER_CIA_TEMP_PATH = "/config/ftpd/update.cia";
#endif

enum class TargetAsset
{
	Cia,
	ThreeDSX,
	Nro,
	Unknown
};

struct ReleaseInfo
{
	std::string tag;
	std::string assetUrl;
	bool prerelease = false;
};

// State variables
std::atomic<updater::State> s_state {updater::State::Idle};
std::atomic<updater::Prompt> s_prompt {updater::Prompt::None};
std::atomic<int> s_progress {0};
std::atomic<bool> s_busy {false};
std::atomic<bool> s_restartRequested {false};

bool s_autoCheck = true;
bool s_beta      = kIsBetaBuild;

std::string s_targetPath;
ReleaseInfo s_currentRelease;

platform::Mutex s_mutex;
std::string s_remoteTag;
std::string s_message;
std::string s_notes;

std::unique_ptr<platform::Thread> s_workerThread;

// -----------------------------------------------------------------------------
// Version Parsing & Comparison
// -----------------------------------------------------------------------------

bool parseVersion (std::string_view s, long out[3], bool &isDev)
{
	if (!s.empty () && (s[0] == 'v' || s[0] == 'V'))
		s.remove_prefix (1);

	out[0] = out[1] = out[2] = 0;
	isDev                    = false;

	for (int i = 0; i < 3; ++i)
	{
		if (s.empty () || !std::isdigit (static_cast<unsigned char> (s[0])))
			return false;

		char *end = nullptr;
		std::string const part (s);
		out[i] = std::strtol (part.c_str (), &end, 10);
		if (end == part.c_str ())
			return false;

		auto const consumed = static_cast<std::size_t> (end - part.c_str ());
		s.remove_prefix (std::min (consumed, s.size ()));
		if (i < 2)
		{
			if (s.empty () || s[0] != '.')
				return false;
			s.remove_prefix (1);
		}
	}

	isDev = (s.find ("-dev") != std::string_view::npos ||
	         s.find ("-beta") != std::string_view::npos);
	return true;
}

bool isNewer (std::string_view current, std::string_view remote)
{
	long cur[3], rem[3];
	bool curDev = false, remDev = false;

	if (!parseVersion (current, cur, curDev) || !parseVersion (remote, rem, remDev))
		return false;

	for (int i = 0; i < 3; ++i)
	{
		if (rem[i] != cur[i])
			return rem[i] > cur[i];
	}

	return curDev && !remDev;
}

bool isNewerBuild (std::string_view current, bool currentIsBeta,
    std::string_view remote, bool remotePrerelease)
{
	if (isNewer (current, remote))
		return true;

	if (!currentIsBeta || remotePrerelease)
		return false;

	long cur[3], rem[3];
	bool curDev = false, remDev = false;
	if (!parseVersion (current, cur, curDev) || !parseVersion (remote, rem, remDev))
		return false;

	return !curDev && !remDev && cur[0] == rem[0] && cur[1] == rem[1] && cur[2] == rem[2];
}

// -----------------------------------------------------------------------------
// Platform Helpers
// -----------------------------------------------------------------------------

TargetAsset getTargetAssetType ()
{
#if defined(__SWITCH__)
	return TargetAsset::Nro;
#elif defined(__3DS__)
	return envIsHomebrew () ? TargetAsset::ThreeDSX : TargetAsset::Cia;
#else
	return TargetAsset::Unknown;
#endif
}

bool isMatchingAsset (std::string_view const url, TargetAsset const type)
{
	// Check for ftpd-ex pattern
	auto const lowerMatches = (url.find ("ftpd-ex") != std::string_view::npos ||
	                           url.find ("ftpd_ex") != std::string_view::npos);
	if (!lowerMatches)
		return false;

	switch (type)
	{
	case TargetAsset::Cia:
		return url.ends_with (".cia");
	case TargetAsset::ThreeDSX:
		return url.ends_with (".3dsx");
	case TargetAsset::Nro:
		return url.ends_with (".nro");
	default:
		return false;
	}
}

void setMessage (std::string const &msg)
{
	auto const lock = std::scoped_lock (s_mutex);
	s_message       = msg;
}

void fail (std::string const &msg, long code = 0)
{
	if (code != 0)
	{
		char buf[96];
		std::snprintf (buf, sizeof (buf), "%s (0x%lX)", msg.c_str (), code);
		setMessage (buf);
	}
	else
	{
		setMessage (msg);
	}
	s_state = updater::State::Error;
}

// -----------------------------------------------------------------------------
// JSON Scanning & Release Notes
// -----------------------------------------------------------------------------

bool findString (char const *seg, char const *segEnd, char const *key,
    char *out, std::size_t outSize)
{
	char pat[48];
	std::snprintf (pat, sizeof (pat), "\"%s\"", key);
	char const *p = std::strstr (seg, pat);
	if (!p || p >= segEnd)
		return false;

	p += std::strlen (pat);
	while (p < segEnd && (*p == ' ' || *p == ':'))
		++p;

	if (p >= segEnd || *p != '"')
		return false;
	++p;

	std::size_t n = 0;
	while (p < segEnd && *p != '"' && n + 1 < outSize)
	{
		if (*p == '\\' && p + 1 < segEnd)
			++p; // Skip escape character
		out[n++] = *p++;
	}

	if (p >= segEnd || *p != '"')
		return false;

	out[n] = '\0';
	return true;
}

bool findAssetUrl (char const *seg, char const *segEnd, TargetAsset type,
    char *out, std::size_t outSize)
{
	static constexpr auto kKey = "\"browser_download_url\"";
	char const *p              = seg;

	while ((p = std::strstr (p, kKey)) != nullptr && p < segEnd)
	{
		char url[384];
		if (findString (p, segEnd, "browser_download_url", url, sizeof (url)))
		{
			if (isMatchingAsset (url, type))
			{
				std::strncpy (out, url, outSize - 1);
				out[outSize - 1] = '\0';
				return true;
			}
		}
		p += sizeof (kKey) - 1;
	}
	return false;
}

bool pickRelease (char const *json, bool allowBeta, TargetAsset type, ReleaseInfo &out)
{
	static constexpr auto kTag = "\"tag_name\"";
	char const *p              = json;

	while ((p = std::strstr (p, kTag)) != nullptr)
	{
		char const *next   = std::strstr (p + sizeof (kTag) - 1, kTag);
		char const *segEnd = next ? next : p + std::strlen (p);
		char tagBuf[32];
		char urlBuf[384];

		if (findString (p, segEnd, "tag_name", tagBuf, sizeof (tagBuf)))
		{
			bool prerelease  = false;
			char const *pre  = std::strstr (p, "\"prerelease\"");
			if (pre && pre < segEnd)
			{
				pre += sizeof ("\"prerelease\"") - 1;
				while (pre < segEnd && (*pre == ' ' || *pre == ':'))
					++pre;
				prerelease = (std::strncmp (pre, "true", 4) == 0);
			}

			if ((allowBeta || !prerelease) &&
			    findAssetUrl (p, segEnd, type, urlBuf, sizeof (urlBuf)))
			{
				out.tag        = tagBuf;
				out.assetUrl   = urlBuf;
				out.prerelease = prerelease;
				return true;
			}
		}
		p = segEnd;
		if (!next)
			break;
	}
	return false;
}

int hexVal (char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

void jsonUnescape (char const *p, char const *end, char *dst, std::size_t cap)
{
	std::size_t n = 0;
	while (p < end && n + 4 < cap)
	{
		char c = *p++;
		if (c != '\\' || p >= end)
		{
			dst[n++] = c;
			continue;
		}
		c = *p++;
		switch (c)
		{
		case 'n': dst[n++] = '\n'; break;
		case 't': dst[n++] = ' '; break;
		case 'r': break;
		case 'u': {
			long cp = 0;
			for (int i = 0; i < 4 && p < end && hexVal (*p) >= 0; ++i)
				cp = cp * 16 + hexVal (*p++);
			if (cp < 0x80)
				dst[n++] = static_cast<char> (cp);
			else if (cp < 0x800)
			{
				dst[n++] = static_cast<char> (0xC0 | (cp >> 6));
				dst[n++] = static_cast<char> (0x80 | (cp & 0x3F));
			}
			else
				dst[n++] = '?';
			break;
		}
		default: dst[n++] = c; break;
		}
	}
	dst[n] = '\0';
}

bool appendLine (std::string &out, std::string_view line)
{
	if (out.size () + line.size () + 8 >= UPDATER_NOTES_MAX)
		return false;
	out.append (line);
	out.push_back ('\n');
	return true;
}

bool appendBlock (char *blk, std::string &out)
{
	char *line = blk;
	while (*line)
	{
		char *nl = std::strchr (line, '\n');
		if (nl)
			*nl = '\0';

		char const *r = line;
		while (*r == ' ' || *r == '\t')
			++r;

		// Skip headings (markdown #), empty lines, images (![), links/comments (<)
		if (*r != '#' && *r != '\0' && *r != '!' && *r != '<')
		{
			if (std::strncmp (r, "http", 4) == 0)
			{
				if (!nl)
					break;
				line = nl + 1;
				continue;
			}

			std::string clean;
			if ((r[0] == '*' || r[0] == '-') && r[1] == ' ')
			{
				clean.append ("- ");
				r += 2;
			}
			else
			{
				clean.append ("- ");
			}

			while (*r)
			{
				if (*r == '`')
				{
					++r;
					continue;
				}
				if (r[0] == '*' && r[1] == '*')
				{
					r += 2;
					continue;
				}
				clean.push_back (*r++);
			}

			while (!clean.empty () && (clean.back () == ' ' || clean.back () == '\r'))
				clean.pop_back ();

			if (clean.size () > 2)
			{
				if (!appendLine (out, clean))
					return false;
			}
		}

		if (!nl)
			break;
		line = nl + 1;
	}
	return true;
}

int collectNotes (char const *json, bool allowBeta, std::string_view current,
    bool currentIsBeta, std::string &out)
{
	static constexpr auto kTag        = "\"tag_name\"";
	static constexpr auto kNotesBegin = "<!-- ftpd-notes -->";
	static constexpr auto kNotesEnd   = "<!-- /ftpd-notes -->";

	char const *p = json;
	int count     = 0;
	bool full     = false;

	out.clear ();

	while (!full && (p = std::strstr (p, kTag)) != nullptr)
	{
		char const *next   = std::strstr (p + sizeof (kTag) - 1, kTag);
		char const *segEnd = next ? next : p + std::strlen (p);
		char tag[32];

		if (findString (p, segEnd, "tag_name", tag, sizeof (tag)))
		{
			bool prerelease = false;
			char const *pre = std::strstr (p, "\"prerelease\"");
			if (pre && pre < segEnd)
			{
				pre += sizeof ("\"prerelease\"") - 1;
				while (pre < segEnd && (*pre == ' ' || *pre == ':'))
					++pre;
				prerelease = (std::strncmp (pre, "true", 4) == 0);
			}

			if ((allowBeta || !prerelease) &&
			    isNewerBuild (current, currentIsBeta, tag, prerelease))
			{
				std::string head = (count ? "\n== " : "== ") + std::string (tag) + " ==";
				if (!appendLine (out, head))
					break;
				++count;

				char const *body = std::strstr (p, "\"body\"");
				char const *b0   = nullptr;
				char const *b1   = nullptr;

				if (body && body < segEnd)
				{
					char const *q = body + sizeof ("\"body\"") - 1;
					while (q < segEnd && (*q == ' ' || *q == ':'))
						++q;
					if (q < segEnd && *q == '"')
					{
						char const *e = ++q;
						while (e < segEnd && *e != '"')
							e += (*e == '\\' && e + 1 < segEnd) ? 2 : 1;
						b0 = q;
						b1 = e;
					}
				}

				char const *m0 = nullptr;
				char const *m1 = b1;
				for (char const *s0 = b0; b0 && s0 + std::strlen (kNotesBegin) <= b1; ++s0)
				{
					if (std::strncmp (s0, kNotesBegin, std::strlen (kNotesBegin)) == 0)
					{
						m0 = s0 + std::strlen (kNotesBegin);
						break;
					}
				}
				for (char const *s0 = m0; m0 && s0 + std::strlen (kNotesEnd) <= b1; ++s0)
				{
					if (std::strncmp (s0, kNotesEnd, std::strlen (kNotesEnd)) == 0)
					{
						m1 = s0;
						break;
					}
				}

				if (m0)
				{
					char blk[4096];
					jsonUnescape (m0, m1, blk, sizeof (blk));
					if (!appendBlock (blk, out))
						full = true;
				}
				else
				{
					appendLine (out, "- " + std::string (tr (STR_UPDATES_NO_NOTES)));
				}
			}
		}
		p = segEnd;
		if (!next)
			break;
	}

	return count;
}

// -----------------------------------------------------------------------------
// HTTP Helpers (libcurl)
// -----------------------------------------------------------------------------

struct MemoryBuffer
{
	std::string data;
};

std::size_t memWriteCallback (void *contents, std::size_t size, std::size_t nmemb, void *userp)
{
	auto const realsize = size * nmemb;
	auto *mem          = static_cast<MemoryBuffer *> (userp);
	if (mem->data.size () + realsize > UPDATER_JSON_MAX)
		return 0; // Truncate safely
	mem->data.append (static_cast<char const *> (contents), realsize);
	return realsize;
}

struct FileDownloadCtx
{
	FILE *file             = nullptr;
	std::size_t totalBytes = 0;
	std::size_t gotBytes   = 0;
	int baseProgress       = 0;
	int progressSpan       = 100;
};

std::size_t fileWriteCallback (void *contents, std::size_t size, std::size_t nmemb, void *userp)
{
	auto const realsize = size * nmemb;
	auto *ctx          = static_cast<FileDownloadCtx *> (userp);

	if (ctx->file)
	{
		auto const written = std::fwrite (contents, 1, realsize, ctx->file);
		if (written != realsize)
			return 0;
	}

	ctx->gotBytes += realsize;
	if (ctx->totalBytes > 0)
	{
		auto const pct = static_cast<int> ((ctx->gotBytes * ctx->progressSpan) / ctx->totalBytes);
		s_progress.store (std::clamp (ctx->baseProgress + pct, 0, 100));
	}
	return realsize;
}

int xferInfoCallback (void *clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
	auto *ctx = static_cast<FileDownloadCtx *> (clientp);
	if (dltotal > 0)
	{
		ctx->totalBytes = static_cast<std::size_t> (dltotal);
		auto const pct  = static_cast<int> ((dlnow * ctx->progressSpan) / dltotal);
		s_progress.store (std::clamp (ctx->baseProgress + pct, 0, 100));
	}
	return 0;
}

bool httpGet (std::string const &url, MemoryBuffer &out)
{
	CURL *curl = curl_easy_init ();
	if (!curl)
	{
		fail ("curl_easy_init failed");
		return false;
	}

	curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
	curl_easy_setopt (curl, CURLOPT_USERAGENT, "ftpd-EX-updater/" FTPD_VERSION_STRING);
	curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt (curl, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt (curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt (curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
	curl_easy_setopt (curl, CURLOPT_LOW_SPEED_TIME, 30L);
	curl_easy_setopt (curl, CURLOPT_BUFFERSIZE, static_cast<long> (UPDATER_CHUNK));
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, memWriteCallback);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, &out);

	auto const res = curl_easy_perform (curl);
	long httpCode = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_easy_cleanup (curl);

	if (res != CURLE_OK)
	{
		fail (curl_easy_strerror (res));
		return false;
	}
	if (httpCode != 200)
	{
		fail ("HTTP error", httpCode);
		return false;
	}
	return true;
}

bool httpDownloadFile (std::string const &url, std::string const &destPath,
    int baseProgress, int progressSpan)
{
	FILE *fp = std::fopen (destPath.c_str (), "wb");
	if (!fp)
	{
		fail ("Cannot write file: " + destPath);
		return false;
	}
	std::setvbuf (fp, nullptr, _IOFBF, UPDATER_FILE_BUF);

	FileDownloadCtx ctx;
	ctx.file         = fp;
	ctx.baseProgress = baseProgress;
	ctx.progressSpan = progressSpan;

	CURL *curl = curl_easy_init ();
	if (!curl)
	{
		std::fclose (fp);
		std::remove (destPath.c_str ());
		fail ("curl_easy_init failed");
		return false;
	}

	curl_easy_setopt (curl, CURLOPT_URL, url.c_str ());
	curl_easy_setopt (curl, CURLOPT_USERAGENT, "ftpd-EX-updater/" FTPD_VERSION_STRING);
	curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt (curl, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt (curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt (curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
	curl_easy_setopt (curl, CURLOPT_LOW_SPEED_TIME, 30L);
	curl_easy_setopt (curl, CURLOPT_BUFFERSIZE, static_cast<long> (UPDATER_CHUNK));
	curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, fileWriteCallback);
	curl_easy_setopt (curl, CURLOPT_WRITEDATA, &ctx);
	curl_easy_setopt (curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
	curl_easy_setopt (curl, CURLOPT_XFERINFODATA, &ctx);
	curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0L);

	auto const res = curl_easy_perform (curl);
	long httpCode = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_easy_cleanup (curl);
	std::fclose (fp);

	if (res != CURLE_OK)
	{
		std::remove (destPath.c_str ());
		fail (curl_easy_strerror (res));
		return false;
	}
	if (httpCode != 200)
	{
		std::remove (destPath.c_str ());
		fail ("HTTP download error", httpCode);
		return false;
	}
	return true;
}

// -----------------------------------------------------------------------------
// 3DS CIA Installation
// -----------------------------------------------------------------------------

#ifdef __3DS__
FS_MediaType getInstalledMediaType (u64 *outPid)
{
	u64 pid              = 0;
	u8 media             = MEDIATYPE_SD;
	bool reg = false, ld = false;
	APT_AppletAttr attr;

	if (R_SUCCEEDED (APT_GetAppletInfo (APPID_APPLICATION, &pid, &media, &reg, &ld, &attr)) &&
	    (media == MEDIATYPE_SD || media == MEDIATYPE_NAND))
	{
		if (outPid)
			*outPid = pid;
		return static_cast<FS_MediaType> (media);
	}
	if (outPid)
		*outPid = 0;
	return MEDIATYPE_SD;
}

bool installCiaFromFile (char const *ciaPath, FS_MediaType media, bool overwrite)
{
	FILE *f = std::fopen (ciaPath, "rb");
	if (!f)
	{
		fail ("Cannot open CIA on SD");
		return false;
	}

	std::fseek (f, 0, SEEK_END);
	auto const total = static_cast<u32> (std::ftell (f));
	std::fseek (f, 0, SEEK_SET);

	auto *buf = static_cast<u8 *> (std::malloc (UPDATER_CHUNK));
	if (!buf || total == 0)
	{
		if (buf)
			std::free (buf);
		std::fclose (f);
		fail ("Out of memory for CIA buffer");
		return false;
	}

	Handle cia;
	Result r = overwrite ? AM_StartCiaInstallOverwrite (&cia, media) : AM_StartCiaInstall (media, &cia);
	if (R_FAILED (r))
	{
		std::free (buf);
		std::fclose (f);
		fail ("AM_StartCiaInstall failed", r);
		return false;
	}

	u32 offset = 0;
	while (offset < total)
	{
		auto const n = std::fread (buf, 1, UPDATER_CHUNK, f);
		u32 wrote = 0;
		if (n == 0)
			r = -1;
		else
			r = FSFILE_Write (cia, &wrote, offset, buf, static_cast<u32> (n), 0);

		if (R_FAILED (r) || wrote != n)
		{
			AM_CancelCIAInstall (cia);
			std::free (buf);
			std::fclose (f);
			fail ("FSFILE_Write CIA failed", r);
			return false;
		}

		offset += static_cast<u32> (n);
		s_progress.store (70 + static_cast<int> ((static_cast<u64> (offset) * 30) / total));
	}

	std::free (buf);
	std::fclose (f);

	r = AM_FinishCiaInstall (cia);
	if (R_FAILED (r))
	{
		fail ("AM_FinishCiaInstall failed", r);
		return false;
	}
	return true;
}
#endif

// -----------------------------------------------------------------------------
// Background Worker Tasks
// -----------------------------------------------------------------------------

bool doCheck ()
{
	s_state = updater::State::Checking;
	setMessage ("");

	MemoryBuffer jsonBuf;
	if (!httpGet (UPDATER_DEFAULT_URL, jsonBuf))
		return false;

	auto const targetType = getTargetAssetType ();
	ReleaseInfo rel;
	bool const allowBeta = s_beta || kIsBetaBuild;
	if (!pickRelease (jsonBuf.data.c_str (), allowBeta, targetType, rel))
	{
		fail ("No compatible release found");
		return false;
	}

	std::string notes;
	if (collectNotes (jsonBuf.data.c_str (), allowBeta, FTPD_VERSION_STRING, kIsBetaBuild, notes) == 0)
	{
		collectNotes (jsonBuf.data.c_str (), allowBeta, "v0.0.0", false, notes);
	}

	{
		auto const lock = std::scoped_lock (s_mutex);
		s_remoteTag     = rel.tag;
		s_notes         = std::move (notes);
		s_currentRelease = rel;
	}

	if (!isNewerBuild (FTPD_VERSION_STRING, kIsBetaBuild, rel.tag, rel.prerelease))
	{
		s_state = updater::State::UpToDate;
		return false;
	}

	s_state = updater::State::Available;
	return true;
}

void doInstall ()
{
	s_progress = 0;
	s_state    = updater::State::Downloading;
	setMessage ("");

#if defined(__3DS__)
	auto const targetType = getTargetAssetType ();
	std::string destFile;
	if (targetType == TargetAsset::Cia)
	{
		destFile = UPDATER_CIA_TEMP_PATH;
		if (!httpDownloadFile (s_currentRelease.assetUrl, destFile, 0, 70))
			return;

		s_state = updater::State::Installing;
		Result r = amInit ();
		if (R_FAILED (r))
		{
			std::remove (destFile.c_str ());
			fail ("amInit failed", r);
			return;
		}

		u64 pid = 0;
		auto media = getInstalledMediaType (&pid);

		if (!installCiaFromFile (destFile.c_str (), media, true))
		{
			// Try delete title and fresh install fallback
			if (pid != 0 && R_SUCCEEDED (AM_DeleteTitle (media, pid)))
			{
				if (!installCiaFromFile (destFile.c_str (), media, false))
				{
					amExit ();
					return;
				}
			}
			else
			{
				amExit ();
				return;
			}
		}

		amExit ();
		std::remove (destFile.c_str ());
	}
	else // 3DSX mode
	{
		std::string target = s_targetPath.empty () ? "/3ds/ftpd-ex.3dsx" : s_targetPath;
		std::string temp   = target + ".update";

		if (!httpDownloadFile (s_currentRelease.assetUrl, temp, 0, 90))
			return;

		s_state = updater::State::Installing;
		std::remove (target.c_str ());
		if (std::rename (temp.c_str (), target.c_str ()) != 0)
		{
			fail ("Failed to replace 3DSX binary");
			return;
		}
	}
#elif defined(__SWITCH__)
	std::string target = s_targetPath.empty () ? "sdmc:/switch/ftpd-ex.nro" : s_targetPath;
	// Normalize path if necessary
	if (!target.starts_with ("sdmc:") && target.starts_with ("/"))
		target = "sdmc:" + target;

	std::string temp = target + ".update";
	if (!httpDownloadFile (s_currentRelease.assetUrl, temp, 0, 90))
		return;

	s_state = updater::State::Installing;

	// Close romfs to ensure any file handle on the running NRO is released
	romfsExit ();

	std::remove (target.c_str ());
	if (std::rename (temp.c_str (), target.c_str ()) != 0)
	{
		// Fallback: keep temp file and update target path for envSetNextLoad
		info ("Rename failed, will chainload .update directly\n");
		target = temp;
	}
	s_targetPath = target;
#else
	fail ("Auto-update not supported on this platform");
	return;
#endif

	s_progress = 100;
	s_state    = updater::State::Installed;
	s_prompt   = updater::Prompt::AskRestart;
}

void workerEntryPoint (bool autoCheck, bool installJob)
{
	if (installJob)
	{
		doInstall ();
	}
	else
	{
		bool const avail = doCheck ();
		if (avail && autoCheck)
			s_prompt = updater::Prompt::AskInstall;
	}

	if (s_state == updater::State::Error && s_prompt == updater::Prompt::Progress)
		s_prompt = updater::Prompt::Error;

	s_busy = false;
}

void startJob (bool autoCheck, bool installJob)
{
	if (s_busy.exchange (true))
		return;

	if (s_workerThread)
	{
		s_workerThread->join ();
		s_workerThread.reset ();
	}

	s_workerThread = std::make_unique<platform::Thread> ([autoCheck, installJob] () {
		workerEntryPoint (autoCheck, installJob);
	});
}

} // namespace

// -----------------------------------------------------------------------------
// Public Interface
// -----------------------------------------------------------------------------

namespace updater
{

void init (char const *argv0_, bool autoCheck_)
{
	if (argv0_ && argv0_[0] != '\0')
		s_targetPath = argv0_;

	s_autoCheck = autoCheck_;
	if (s_autoCheck && platform::networkVisible ())
		checkAuto ();
}

void checkNow ()
{
	startJob (false, false);
}

void checkAuto ()
{
	startJob (true, false);
}

bool isBetaBuild ()
{
	return kIsBetaBuild;
}

void install ()
{
	if (s_state == State::Available)
	{
		s_prompt = Prompt::Progress;
		startJob (false, true);
	}
}

void restart ()
{
	s_restartRequested = true;

#if defined(__3DS__)
	aptSetChainloaderToSelf ();
#elif defined(__SWITCH__)
	if (envHasNextLoad ())
	{
		std::string path = s_targetPath.empty () ? "sdmc:/switch/ftpd-ex.nro" : s_targetPath;
		envSetNextLoad (path.c_str (), path.c_str ());
	}
#endif
}

void answerPrompt (bool yes_)
{
	switch (s_prompt.load ())
	{
	case Prompt::AskInstall:
		if (yes_)
			install ();
		else
			s_prompt = Prompt::None;
		break;
	case Prompt::AskRestart:
		if (yes_)
			restart ();
		else
			s_prompt = Prompt::None;
		break;
	case Prompt::Error:
		s_prompt = Prompt::None;
		break;
	default:
		break;
	}
}

void dismissPrompt ()
{
	s_prompt = Prompt::None;
}

State getState ()
{
	return s_state.load ();
}

Prompt getPrompt ()
{
	return s_prompt.load ();
}

int getProgress ()
{
	return s_progress.load ();
}

std::string getRemoteTag ()
{
	auto const lock = std::scoped_lock (s_mutex);
	return s_remoteTag;
}

std::string getMessage ()
{
	auto const lock = std::scoped_lock (s_mutex);
	return s_message;
}

std::string getNotes ()
{
	auto const lock = std::scoped_lock (s_mutex);
	return s_notes;
}

bool hasNotes ()
{
	auto const lock = std::scoped_lock (s_mutex);
	return !s_notes.empty ();
}

bool getAutoCheck ()
{
	return s_autoCheck;
}

void setAutoCheck (bool enabled_)
{
	s_autoCheck = enabled_;
}

bool getBeta ()
{
	return s_beta;
}

void setBeta (bool enabled_)
{
	s_beta = enabled_;
}

bool isBusy ()
{
	return s_busy.load ();
}

bool isHomebrewMode ()
{
#if defined(__SWITCH__)
	return true;
#elif defined(__3DS__)
	return envIsHomebrew ();
#else
	return true;
#endif
}

bool restartSupported ()
{
#if defined(__3DS__)
	return true;
#elif defined(__SWITCH__)
	return envHasNextLoad ();
#else
	return false;
#endif
}

bool isRestartRequested ()
{
	return s_restartRequested.load ();
}

} // namespace updater
