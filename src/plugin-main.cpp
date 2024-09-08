#include "plugin-main.hpp"
#include "window-dock-ui.hpp"

WindowDockUI windowDockUI;

bool obs_module_load(void) {
    obs_frontend_add_tools_menu_item(obs_module_text("OBSMenu.CustomWindowDocks"), [](void*){
        windowDockUI.openCustomWindowDocksUI(nullptr);
    }, nullptr);

    windowDockUI.restoreDocksOnStartup();
    blog(LOG_INFO, "Custom Window Docks plugin loaded successfully");

    return true;
}

void obs_module_unload(void) {
    windowDockUI.freeEmbeddedWindowsOnClose();
    blog(LOG_INFO, "Custom Window Docks plugin unloaded");
}