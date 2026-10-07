#include "i18n.h"

#ifdef __3DS__
#include <3ds.h>
#endif

namespace
{
Language s_currentLang = Language::English;

char const *const s_strings[static_cast<std::size_t> (Language::Count)][STR_COUNT] = {
    // English
    [static_cast<std::size_t> (Language::English)] = {
        [STR_APP_NAME]              = "ftpd-EX",
        [STR_APP_TITLE_BAR]          = "ftpd-EX",
        [STR_ONLINE]                = "● ONLINE ",
        [STR_WAITING_WIFI]          = "○ WAITING FOR WI-FI",
        [STR_NO_CONNECTION]         = "•  No connection",
        [STR_ENABLE_WIFI_HINT]      = "Enable Wi-Fi in the HOME menu",
        [STR_SESSION_SINGLE]        = "session active",
        [STR_SESSIONS_PLURAL]       = "sessions active",
        [STR_SESSIONS_TITLE]        = "Sessions",
        [STR_SERVER_READY]          = "FTP Server",
        [STR_CONNECT_INSTRUCTIONS]  = "Ready for connections. Connect with your FTP client:",
        [STR_HOST_LABEL]            = "Host:",
        [STR_USER_LABEL]            = "User: anonymous (or empty)",
        [STR_STORAGE_FREE]          = "Storage Free: %s",
        [STR_ACTIVE_TRANSFERS_HINT] = "Active transfers and downloads will appear here.",
        [STR_SESSIONS_IDLE_HINT]    = "● %zu %s (idle, waiting for transfer)",
        [STR_TURN_ON_WIFI_HINT]     = "Turn on Wi-Fi to start the server.",
        [STR_BTN_SETTINGS]          = "Settings",
        [STR_BTN_HELP]              = "Help",
        [STR_BTN_SCREENS]           = "Sleep LCD",
        [STR_BTN_APPLY]             = "Apply",
        [STR_BTN_SAVE]              = "Save",
        [STR_BTN_RESET]             = "Reset",
        [STR_BTN_CANCEL]            = "Cancel",
        [STR_BTN_CLOSE]             = "Close (B)",
        [STR_BTN_OK]                = "OK",
        [STR_UPLOAD_LOG]            = "Upload Log",
        [STR_SAVE_LOG_SD]           = "Save Log to SD",
        [STR_LOG_SAVED]             = "Log saved to SD",
        [STR_ABOUT]                 = "About",
        [STR_QUIT]                  = "Quit",
        [STR_SETTINGS_TITLE]        = "Settings",
        [STR_HELP_TITLE]            = "Help & Controls",
        [STR_ABOUT_TITLE]           = "About ftpd-EX",
        [STR_LANGUAGE]              = "Language",
        [STR_USER]                  = "User",
        [STR_PASS]                  = "Password",
        [STR_HOSTNAME]              = "Hostname",
        [STR_PORT]                  = "Port",
        [STR_DEFLATE_LEVEL]         = "Deflate Level",
        [STR_GET_MTIME]             = "Preserve Timestamp (mtime)",
        [STR_ENABLE_AP]            = "Enable Access Point",
        [STR_IDLE]                  = "Idle...",
        [STR_UNKNOWN_SIZE]          = "unknown size",
        [STR_UPLOADING]             = "Uploading...",
        [STR_DOWNLOADING]           = "Downloading...",
        [STR_HELP_TAB_CONTROLS]     = "Controls",
        [STR_HELP_TAB_CONNECT]      = "How to Connect",
        [STR_HELP_TAB_ABOUT]        = "About",
        [STR_HELP_CTRL_Y]           = "Open / Close Settings",
        [STR_HELP_CTRL_X]           = "Open / Close Help",
        [STR_HELP_CTRL_B]           = "Close current popup / Cancel",
        [STR_HELP_CTRL_A]           = "Confirm / Select option",
        [STR_HELP_CTRL_DPAD]        = "Scroll log lines",
        [STR_HELP_CTRL_LR]          = "Fast page scroll up / down",
        [STR_HELP_CTRL_SELECT]      = "Turn LCD screens off / on (battery saver during transfers)",
        [STR_HELP_CTRL_START]       = "Exit ftpd-EX",
        [STR_HELP_CTRL_TOUCH]       = "Direct touch interaction for buttons & options",
        [STR_HELP_CONNECT_DESC1]    = "1. Connect your console and PC/phone to the same Wi-Fi network.",
        [STR_HELP_CONNECT_DESC2]    = "2. Open FileZilla, WinSCP, Cyberduck, or your preferred FTP client.",
        [STR_HELP_CONNECT_DESC3]    = "3. Enter the Host IP, Port (5000), and connect as anonymous.",
        [STR_ABOUT_DESC]            = "ftpd-EX is an enhanced FTP server for Nintendo 3DS and Switch with robust network recovery, ETA progress estimation, native console controls, and dual-screen optimization.",
        [STR_ABOUT_CREDITS]         = "Fork of ftpd by Michael Theall (c) 2024.\nftpd-EX improvements by tinaut1986.",
        [STR_BTN_ABOUT_DETAILS]     = "Details & Licenses...",
        [STR_SYS_INFO_TITLE]        = "System & Performance",
        [STR_LABEL_PLATFORM]        = "Platform",
        [STR_LABEL_RENDERER]        = "Renderer",
        [STR_LABEL_CMD_BUF]         = "Command Buffer",
        [STR_LABEL_GPU_DRAW]        = "GPU Drawing",
        [STR_LABEL_GPU_PROC]        = "GPU Processing",
        [STR_GPU_PROC_NOTE]         = "CPU frame budget (~15ms of 16.6ms at 60 FPS)",
        [STR_SECTION_CONNECTIONS]   = "Active Connections",
        [STR_SECTION_LICENSES]      = "Component Licenses",
        [STR_NO_ACTIVE_SESSIONS]    = "No active connections",
        [STR_HOME_NOT_ALLOWED]      = "Press START to exit",
        [STR_UPDATES_SECTION]       = "Software Updates",
        [STR_CHECK_UPDATES]         = "Check for updates on launch",
        [STR_CHECK_FOR_UPDATES_BTN] = "Check for Updates",
        [STR_UPDATES_CHECKING]      = "Checking for updates...",
        [STR_UPDATES_UP_TO_DATE]    = "ftpd-EX is up to date",
        [STR_UPDATES_AVAILABLE]     = "Update available: %s",
        [STR_UPDATES_INSTALL_NOW]   = "Update Now",
        [STR_UPDATES_WHATS_NEW]     = "What's New",
        [STR_UPDATES_DOWNLOADING]   = "Downloading: %d%%",
        [STR_UPDATES_INSTALLING]    = "Installing update...",
        [STR_UPDATES_RESTART_PROMPT]= "Update installed successfully!\nRestart now to apply changes?",
        [STR_UPDATES_RESTART_NOW]   = "Restart Now",
        [STR_UPDATES_LATER]         = "Later",
        [STR_UPDATES_CLOSE]         = "Close",
        [STR_UPDATES_NO_NOTES]      = "No release notes available for this version.",
        [STR_UPDATES_ERROR]         = "Update failed: %s",
        [STR_UPDATES_PROMPT_TITLE]  = "Update Available###UpdatePrompt",
    },
    // Spanish
    [static_cast<std::size_t> (Language::Spanish)] = {
        [STR_APP_NAME]              = "ftpd-EX",
        [STR_APP_TITLE_BAR]          = "ftpd-EX",
        [STR_ONLINE]                = "● EN LÍNEA ",
        [STR_WAITING_WIFI]          = "○ ESPERANDO WI-FI",
        [STR_NO_CONNECTION]         = "•  Sin conexión",
        [STR_ENABLE_WIFI_HINT]      = "Activa el Wi-Fi en el menú HOME",
        [STR_SESSION_SINGLE]        = "sesión activa",
        [STR_SESSIONS_PLURAL]       = "sesiones activas",
        [STR_SESSIONS_TITLE]        = "Sesiones",
        [STR_SERVER_READY]          = "Servidor FTP",
        [STR_CONNECT_INSTRUCTIONS]  = "Listo para recibir conexiones. Conéctate con tu cliente FTP:",
        [STR_HOST_LABEL]            = "Servidor:",
        [STR_USER_LABEL]            = "Usuario: anonymous (o vacío)",
        [STR_STORAGE_FREE]          = "Espacio libre: %s",
        [STR_ACTIVE_TRANSFERS_HINT] = "Las transferencias activas aparecerán aqui.",
        [STR_SESSIONS_IDLE_HINT]    = "● %zu %s (en espera de transferencias)",
        [STR_TURN_ON_WIFI_HINT]     = "Enciende el Wi-Fi para iniciar el servidor.",
        [STR_BTN_SETTINGS]          = "Ajustes",
        [STR_BTN_HELP]              = "Ayuda",
        [STR_BTN_SCREENS]           = "Apagar LCD",
        [STR_BTN_APPLY]             = "Aplicar",
        [STR_BTN_SAVE]              = "Guardar",
        [STR_BTN_RESET]             = "Restablecer",
        [STR_BTN_CANCEL]            = "Cancelar",
        [STR_BTN_CLOSE]             = "Cerrar (B)",
        [STR_BTN_OK]                = "Aceptar",
        [STR_UPLOAD_LOG]            = "Subir registro",
        [STR_SAVE_LOG_SD]           = "Guardar registro en SD",
        [STR_LOG_SAVED]             = "Registro guardado en la SD",
        [STR_ABOUT]                 = "Acerca de",
        [STR_QUIT]                  = "Salir",
        [STR_SETTINGS_TITLE]        = "Ajustes",
        [STR_HELP_TITLE]            = "Ayuda y Controles",
        [STR_ABOUT_TITLE]           = "Acerca de ftpd-EX",
        [STR_LANGUAGE]              = "Idioma",
        [STR_USER]                  = "Usuario",
        [STR_PASS]                  = "Contraseña",
        [STR_HOSTNAME]              = "Nombre de host",
        [STR_PORT]                  = "Puerto",
        [STR_DEFLATE_LEVEL]         = "Nivel Deflate",
        [STR_GET_MTIME]             = "Preservar fecha (mtime)",
        [STR_ENABLE_AP]            = "Activar punto de acceso",
        [STR_IDLE]                  = "En espera...",
        [STR_UNKNOWN_SIZE]          = "tamaño desconocido",
        [STR_UPLOADING]             = "Subiendo...",
        [STR_DOWNLOADING]           = "Descargando...",
        [STR_HELP_TAB_CONTROLS]     = "Controles",
        [STR_HELP_TAB_CONNECT]      = "Cómo Conectarse",
        [STR_HELP_TAB_ABOUT]        = "Acerca de",
        [STR_HELP_CTRL_Y]           = "Abrir / Cerrar Ajustes",
        [STR_HELP_CTRL_X]           = "Abrir / Cerrar Ayuda",
        [STR_HELP_CTRL_B]           = "Cerrar ventana / Cancelar",
        [STR_HELP_CTRL_A]           = "Confirmar / Seleccionar opción",
        [STR_HELP_CTRL_DPAD]        = "Desplazar líneas del registro",
        [STR_HELP_CTRL_LR]          = "Salto rápido de pagina arriba / abajo",
        [STR_HELP_CTRL_SELECT]      = "Apagar / encender LCD (ahorro de batería en transferencias)",
        [STR_HELP_CTRL_START]       = "Salir de ftpd-EX",
        [STR_HELP_CTRL_TOUCH]       = "Control táctil directo para botones y opciones",
        [STR_HELP_CONNECT_DESC1]    = "1. Conecta tu consola y tu PC/móvil a la misma red Wi-Fi.",
        [STR_HELP_CONNECT_DESC2]    = "2. Abre FileZilla, WinSCP, Cyberduck o tu cliente FTP favorito.",
        [STR_HELP_CONNECT_DESC3]    = "3. Introduce la IP, el Puerto (5000) y conecta como anónimo.",
        [STR_ABOUT_DESC]            = "ftpd-EX es un servidor FTP mejorado para Nintendo 3DS y Switch con recuperación robusta de red, estimación de tiempo (ETA), controles de consola nativos y optimización para doble pantalla.",
        [STR_ABOUT_CREDITS]         = "Fork de ftpd por Michael Theall (c) 2024.\nMejoras en ftpd-EX por tinaut1986.",
        [STR_BTN_ABOUT_DETAILS]     = "Detalles y licencias...",
        [STR_SYS_INFO_TITLE]        = "Sistema y Rendimiento",
        [STR_LABEL_PLATFORM]        = "Plataforma",
        [STR_LABEL_RENDERER]        = "Renderizador",
        [STR_LABEL_CMD_BUF]         = "Buffer de comandos",
        [STR_LABEL_GPU_DRAW]        = "GPU Dibujado",
        [STR_LABEL_GPU_PROC]        = "GPU Procesado",
        [STR_GPU_PROC_NOTE]         = "Tiempo CPU/Frame (~15ms de 16.6ms a 60 FPS)",
        [STR_SECTION_CONNECTIONS]   = "Conexiones activas",
        [STR_SECTION_LICENSES]      = "Licencias de componentes",
        [STR_NO_ACTIVE_SESSIONS]    = "Sin conexiónes activas",
        [STR_HOME_NOT_ALLOWED]      = "Pulsa START para salir",
        [STR_UPDATES_SECTION]       = "Actualizaciones de software",
        [STR_CHECK_UPDATES]         = "Buscar actualizaciones al inicio",
        [STR_CHECK_FOR_UPDATES_BTN] = "Buscar actualizaciones",
        [STR_UPDATES_CHECKING]      = "Buscando actualizaciones...",
        [STR_UPDATES_UP_TO_DATE]    = "ftpd-EX está actualizado",
        [STR_UPDATES_AVAILABLE]     = "Actualización disponible: %s",
        [STR_UPDATES_INSTALL_NOW]   = "Actualizar ahora",
        [STR_UPDATES_WHATS_NEW]     = "Novedades",
        [STR_UPDATES_DOWNLOADING]   = "Descargando: %d%%",
        [STR_UPDATES_INSTALLING]    = "Instalando actualización...",
        [STR_UPDATES_RESTART_PROMPT]= "¡Actualización instalada con éxito!\n¿Deseas reiniciar ahora para aplicar los cambios?",
        [STR_UPDATES_RESTART_NOW]   = "Reiniciar ahora",
        [STR_UPDATES_LATER]         = "Más tarde",
        [STR_UPDATES_CLOSE]         = "Cerrar",
        [STR_UPDATES_NO_NOTES]      = "No hay notas disponibles para esta versión.",
        [STR_UPDATES_ERROR]         = "Error al actualizar: %s",
        [STR_UPDATES_PROMPT_TITLE]  = "Actualización disponible###UpdatePrompt",
    },
};
}

namespace i18n
{
void init (Language const defaultLang_)
{
	s_currentLang = defaultLang_;
}

void setLanguage (Language const lang_)
{
	if (static_cast<std::size_t> (lang_) < static_cast<std::size_t> (Language::Count))
		s_currentLang = lang_;
}

Language getLanguage ()
{
	return s_currentLang;
}

char const *getLanguageName (Language const lang_)
{
	switch (lang_)
	{
	case Language::English:
		return "English";
	case Language::Spanish:
		return "Español";
	default:
		return "Unknown";
	}
}

char const *get (StringId const id_)
{
	auto const langIdx = static_cast<std::size_t> (s_currentLang);
	if (id_ < STR_COUNT && s_strings[langIdx][id_])
		return s_strings[langIdx][id_];

	// fallback to English
	if (s_strings[0][id_])
		return s_strings[0][id_];

	return "";
}

Language detectSystemLanguage ()
{
#ifdef __3DS__
	u8 lang = CFG_LANGUAGE_EN;
	if (R_SUCCEEDED (cfguInit ()))
	{
		CFGU_GetSystemLanguage (&lang);
		cfguExit ();
	}

	if (lang == CFG_LANGUAGE_ES)
		return Language::Spanish;
#endif

	return Language::English;
}
}
