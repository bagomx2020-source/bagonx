#include "i18n.hpp"
#include <cstring>

namespace {

struct Str { const char* key; const char* es; const char* en; };

const Str kStrings[] = {
    {"tab_cfw", "CFW", "CFW"},
    {"tab_apps", "Apps", "Apps"},
    {"tab_settings", "Ajustes", "Settings"},
    {"tab_about", "Acerca de", "About"},

    {"status", "Estado", "Status"},
    {"firmware", "Firmware", "Firmware"},
    {"ams_installed", "Atmosphère en uso", "Running Atmosphère"},
    {"not_detected", "No detectado", "Not detected"},
    {"downloads", "Descargas", "Downloads"},
    {"dl_ams", "Atmosphère (última versión)", "Atmosphère (latest)"},
    {"dl_ams_sub", "Atmosphere-NX · incluye fusee.bin", "Atmosphere-NX · includes fusee.bin"},
    {"dl_hekate", "Hekate (última versión)", "Hekate (latest)"},
    {"dl_hekate_sub", "CTCaer · también se copia como reboot_payload.bin", "CTCaer · also copied as reboot_payload.bin"},
    {"dl_both", "Atmosphère + Hekate", "Atmosphère + Hekate"},
    {"dl_both_sub", "Actualiza los dos de una vez", "Update both at once"},
    {"system", "Sistema", "System"},
    {"reboot", "Reiniciar consola", "Reboot console"},
    {"reboot_sub", "Con Atmosphère arranca reboot_payload.bin", "With Atmosphère it boots reboot_payload.bin"},

    {"confirm", "Confirmar", "Confirm"},
    {"confirm_install", "¿Descargar e instalar {} en la microSD?", "Download and install {} to the microSD?"},
    {"confirm_reboot", "¿Reiniciar la consola ahora?", "Reboot the console now?"},
    {"yes", "Sí", "Yes"},
    {"no", "No", "No"},
    {"ok", "Aceptar", "OK"},
    {"cancel", "Cancelar", "Cancel"},
    {"back", "Atrás", "Back"},
    {"select", "Elegir", "Select"},
    {"exit", "Salir", "Exit"},
    {"tabs", "Pestañas", "Tabs"},
    {"install", "Instalar", "Install"},
    {"update", "Actualizar", "Update"},
    {"reinstall", "Reinstalar", "Reinstall"},
    {"uninstall", "Desinstalar", "Uninstall"},
    {"installed", "Instalada", "Installed"},
    {"error", "Error", "Error"},
    {"done", "Listo", "Done"},
    {"cancelled", "Operación cancelada.", "Operation cancelled."},
    {"working", "Trabajando…", "Working…"},

    {"ams_done", "Atmosphère {} instalado. Reinicia la consola para usarlo.", "Atmosphère {} installed. Reboot the console to use it."},
    {"hekate_done", "Hekate {} instalado.", "Hekate {} installed."},
    {"querying", "Consultando GitHub…", "Checking GitHub…"},
    {"downloading", "Descargando {}…", "Downloading {}…"},
    {"extracting", "Extrayendo {}…", "Extracting {}…"},
    {"no_asset", "No se encontró el archivo en la versión {}.", "File not found in release {}."},

    {"loading_catalog", "Cargando catálogo…", "Loading catalog…"},
    {"catalog_error", "No se pudo cargar el catálogo", "Could not load the catalog"},
    {"retry", "Reintentar", "Retry"},
    {"category", "Categoría", "Category"},
    {"search", "Buscar", "Search"},
    {"all", "Todas", "All"},
    {"cat_game", "Juegos", "Games"},
    {"cat_emu", "Emuladores", "Emulators"},
    {"cat_tool", "Herramientas", "Tools"},
    {"cat_advanced", "Avanzado", "Advanced"},
    {"cat_theme", "Temas", "Themes"},
    {"cat_aesthetics", "Estética", "Aesthetics"},
    {"cat_concept", "Conceptos", "Concepts"},
    {"cat_misc", "Varios", "Misc"},
    {"cat_legacy", "Antiguas", "Legacy"},
    {"apps_count", "{} apps", "{} apps"},
    {"no_results", "Sin resultados", "No results"},
    {"app_installed_msg", "{} instalada.", "{} installed."},
    {"app_removed_msg", "{} desinstalada.", "{} uninstalled."},
    {"version", "Versión", "Version"},
    {"size", "Tamaño", "Size"},

    {"tab_ports", "Ports", "Ports"},
    {"loading_ports", "Cargando la lista de ports…", "Loading the ports list…"},
    {"ports_error", "No se pudo cargar la lista de ports", "Could not load the ports list"},
    {"ports_count", "{} ports", "{} ports"},
    {"ports_info", "Fuente: github.com/robzilla10001/SwitchPorts · se descarga la última release de cada port",
                   "Source: github.com/robzilla10001/SwitchPorts · each port's latest release is downloaded"},
    {"port_free", "Listo para jugar", "Ready to play"},
    {"port_needs_data", "Necesita tus archivos del juego", "Needs your game files"},
    {"port_ready_hdr", "Listos para jugar", "Ready to play"},
    {"port_data_hdr", "Necesitan tus archivos del juego original", "Need files from your original game"},
    {"port_done", "{} instalado.", "{} installed."},
    {"port_installed", "Instalado", "Installed"},
    {"port_source", "Origen", "Source"},
    {"port_data_warn", "Este port no incluye el juego: solo el motor. Tienes que copiar tus propios archivos del juego original.",
                       "This port does not include the game, only the engine. You must copy your own original game files."},

    {"language", "Idioma", "Language"},
    {"theme", "Tema", "Theme"},
    {"theme_dark", "Oscuro", "Dark"},
    {"theme_light", "Claro", "Light"},
    {"updates", "Actualizaciones", "Updates"},
    {"check_update", "Buscar actualización de " APP_TITLE, "Check for " APP_TITLE " updates"},
    {"repo", "Repositorio: {}", "Repository: {}"},
    {"up_to_date", "Ya tienes la última versión ({}).", "You already have the latest version ({})."},
    {"new_version", "Hay una nueva versión: {}. ¿Instalarla?", "A new version is available: {}. Install it?"},
    {"update_done", APP_TITLE " se actualizó. Cierra y vuelve a abrir la app.", APP_TITLE " was updated. Close and reopen the app."},
    {"update_done_restart", APP_TITLE " se actualizó a {}. Hay que reiniciar la app para usar la nueva versión.",
                            APP_TITLE " was updated to {}. Restart the app to use the new version."},
    {"update_now", "Actualizar ahora", "Update now"},
    {"restart_now", "Reiniciar ahora", "Restart now"},
    {"later", "Más tarde", "Later"},
    {"update_to", "Actualizar a {}", "Update to {}"},
    {"update_to_sub", "Descarga la nueva versión y reemplaza la actual, sin reinstalar nada",
                      "Downloads the new version and replaces this one, no reinstall needed"},
    {"new_badge", "Nueva", "New"},
    {"update_badge", "Actualización {} disponible", "Update {} available"},
    {"auto_update", "Buscar actualizaciones al abrir", "Check for updates on launch"},
    {"auto_update_sub", "Avisa al arrancar si hay una versión nueva en GitHub", "Notifies on launch when GitHub has a new version"},
    {"on", "Sí", "On"},
    {"off", "No", "Off"},
    {"repo_not_set", "Configura tu repositorio de GitHub en /config/bagonx/config.json (campo update_repo, p. ej. usuario/bagonx).",
                     "Set your GitHub repository in /config/bagonx/config.json (update_repo field, e.g. user/bagonx)."},
    {"bad_nro", "El archivo descargado no es un .nro válido.", "The downloaded file is not a valid .nro."},
    {"help", "Ayuda", "Help"},
    {"config_file", "Archivo de configuración", "Config file"},

    {"mode", "Modo", "Mode"},
    {"mode_app", "Aplicación (memoria completa)", "Application (full memory)"},
    {"mode_applet", "Applet (memoria limitada)", "Applet (limited memory)"},
    {"sd_free", "Espacio libre en la microSD", "Free microSD space"},
    {"credits", "Créditos", "Credits"},
    {"credits_sub", "Atmosphère-NX · CTCaer (Hekate) · Homebrew App Store (fortheusers) · SwitchPorts (robzilla10001)",
                    "Atmosphère-NX · CTCaer (Hekate) · Homebrew App Store (fortheusers) · SwitchPorts (robzilla10001)"},
};

bool g_english = false;

} // namespace

void i18nSetLang(const std::string& lang) { g_english = (lang == "en"); }

const char* tr(const char* key) {
    for (const Str& s : kStrings)
        if (strcmp(s.key, key) == 0) return g_english ? s.en : s.es;
    return key;
}
