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
        [STR_ACTIVE_TRANSFERS_HINT] = "Active transfers and downloads will appear here.",
        [STR_TURN_ON_WIFI_HINT]     = "Turn on Wi-Fi to start the server.",
        [STR_BTN_SETTINGS]          = "(Y) Settings",
        [STR_BTN_HELP]              = "(X) Help",
        [STR_BTN_SCREENS]           = "(SELECT) Screens",
        [STR_BTN_APPLY]             = "Apply",
        [STR_BTN_SAVE]              = "Save",
        [STR_BTN_RESET]             = "Reset",
        [STR_BTN_CANCEL]            = "Cancel",
        [STR_BTN_CLOSE]             = "Close (B)",
        [STR_BTN_OK]                = "OK",
        [STR_UPLOAD_LOG]            = "Upload Log",
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
        [STR_IDLE]                  = "Idle...",
        [STR_UNKNOWN_SIZE]          = "unknown size",
        [STR_HELP_TAB_CONTROLS]     = "Controls",
        [STR_HELP_TAB_CONNECT]      = "How to Connect",
        [STR_HELP_TAB_ABOUT]        = "About",
        [STR_HELP_CTRL_Y]           = "Open / Close Settings",
        [STR_HELP_CTRL_X]           = "Open / Close Help",
        [STR_HELP_CTRL_B]           = "Close current popup / Cancel",
        [STR_HELP_CTRL_A]           = "Confirm / Select option",
        [STR_HELP_CTRL_DPAD]        = "Scroll log lines",
        [STR_HELP_CTRL_LR]          = "Fast page scroll up / down",
        [STR_HELP_CTRL_SELECT]      = "Turn screens off / on (battery saver)",
        [STR_HELP_CTRL_START]       = "Exit ftpd-EX",
        [STR_HELP_CTRL_TOUCH]       = "Direct touch interaction for buttons & options",
        [STR_HELP_CONNECT_DESC1]    = "1. Connect your 3DS and PC/phone to the same Wi-Fi network.",
        [STR_HELP_CONNECT_DESC2]    = "2. Open FileZilla, WinSCP, Cyberduck, or your preferred FTP client.",
        [STR_HELP_CONNECT_DESC3]    = "3. Enter the Host IP, Port (5000), and connect as anonymous.",
        [STR_ABOUT_DESC]            = "ftpd-EX is an enhanced FTP server for Nintendo 3DS with robust network recovery, ETA progress estimation, native console controls, and dual-screen optimization.",
        [STR_ABOUT_CREDITS]         = "Fork of ftpd by Michael Theall (c) 2024.\nftpd-EX improvements by tinaut1986.",
    },
    // Spanish
    [static_cast<std::size_t> (Language::Spanish)] = {
        [STR_APP_NAME]              = "ftpd-EX",
        [STR_APP_TITLE_BAR]          = "ftpd-EX",
        [STR_ONLINE]                = "● EN LINEA ",
        [STR_WAITING_WIFI]          = "○ ESPERANDO WI-FI",
        [STR_NO_CONNECTION]         = "•  Sin conexion",
        [STR_ENABLE_WIFI_HINT]      = "Activa el Wi-Fi en el menu HOME",
        [STR_SESSION_SINGLE]        = "sesion activa",
        [STR_SESSIONS_PLURAL]       = "sesiones activas",
        [STR_SESSIONS_TITLE]        = "Sesiones",
        [STR_SERVER_READY]          = "Servidor FTP",
        [STR_CONNECT_INSTRUCTIONS]  = "Listo para recibir conexiones. Conectate con tu cliente FTP:",
        [STR_HOST_LABEL]            = "Servidor:",
        [STR_USER_LABEL]            = "Usuario: anonymous (o vacio)",
        [STR_ACTIVE_TRANSFERS_HINT] = "Las transferencias activas apareceran aqui.",
        [STR_TURN_ON_WIFI_HINT]     = "Enciende el Wi-Fi para iniciar el servidor.",
        [STR_BTN_SETTINGS]          = "(Y) Ajustes",
        [STR_BTN_HELP]              = "(X) Ayuda",
        [STR_BTN_SCREENS]           = "(SELECT) Pantallas",
        [STR_BTN_APPLY]             = "Aplicar",
        [STR_BTN_SAVE]              = "Guardar",
        [STR_BTN_RESET]             = "Restablecer",
        [STR_BTN_CANCEL]            = "Cancelar",
        [STR_BTN_CLOSE]             = "Cerrar (B)",
        [STR_BTN_OK]                = "Aceptar",
        [STR_UPLOAD_LOG]            = "Subir registro",
        [STR_ABOUT]                 = "Acerca de",
        [STR_QUIT]                  = "Salir",
        [STR_SETTINGS_TITLE]        = "Ajustes",
        [STR_HELP_TITLE]            = "Ayuda y Controles",
        [STR_ABOUT_TITLE]           = "Acerca de ftpd-EX",
        [STR_LANGUAGE]              = "Idioma",
        [STR_USER]                  = "Usuario",
        [STR_PASS]                  = "Contrasena",
        [STR_HOSTNAME]              = "Nombre de host",
        [STR_PORT]                  = "Puerto",
        [STR_DEFLATE_LEVEL]         = "Nivel Deflate",
        [STR_GET_MTIME]             = "Preservar fecha (mtime)",
        [STR_IDLE]                  = "En espera...",
        [STR_UNKNOWN_SIZE]          = "tamano desconocido",
        [STR_HELP_TAB_CONTROLS]     = "Controles",
        [STR_HELP_TAB_CONNECT]      = "Como Conectarse",
        [STR_HELP_TAB_ABOUT]        = "Acerca de",
        [STR_HELP_CTRL_Y]           = "Abrir / Cerrar Ajustes",
        [STR_HELP_CTRL_X]           = "Abrir / Cerrar Ayuda",
        [STR_HELP_CTRL_B]           = "Cerrar ventana / Cancelar",
        [STR_HELP_CTRL_A]           = "Confirmar / Seleccionar opcion",
        [STR_HELP_CTRL_DPAD]        = "Desplazar lineas del registro",
        [STR_HELP_CTRL_LR]          = "Salto rapido de pagina arriba / abajo",
        [STR_HELP_CTRL_SELECT]      = "Apagar / Encender pantallas (Ahorro de bateria)",
        [STR_HELP_CTRL_START]       = "Salir de ftpd-EX",
        [STR_HELP_CTRL_TOUCH]       = "Control tactil directo para botones y opciones",
        [STR_HELP_CONNECT_DESC1]    = "1. Conecta tu 3DS y tu PC/movil a la misma red Wi-Fi.",
        [STR_HELP_CONNECT_DESC2]    = "2. Abre FileZilla, WinSCP, Cyberduck o tu cliente FTP favorito.",
        [STR_HELP_CONNECT_DESC3]    = "3. Introduce la IP, el Puerto (5000) y conecta como anonimo.",
        [STR_ABOUT_DESC]            = "ftpd-EX es un servidor FTP mejorado para Nintendo 3DS con recuperacion robusta de red, estimacion de tiempo (ETA), controles de consola nativos y optimizacion para doble pantalla.",
        [STR_ABOUT_CREDITS]         = "Fork de ftpd por Michael Theall (c) 2024.\nMejoras en ftpd-EX por tinaut1986.",
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
		return "Espanol";
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
