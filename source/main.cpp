// BagoNX — gestor todo en uno para Nintendo Switch con homebrew.
#include <switch.h>
#include <cstdio>
#include "app.hpp"
#include "config.hpp"
#include "gfx.hpp"
#include "i18n.hpp"
#include "net.hpp"

int main(int argc, char** argv) {
    socketInitializeDefault();
    plInitialize(PlServiceType_User);
    setsysInitialize();
    splInitialize();
    spsmInitialize();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    cfgLoad();
    i18nSetLang(cfg().lang);
    net::init();

    if (gfx::init()) {
        appRun(argc > 0 ? argv[0] : nullptr);
    } else {
        // Sin gráficos: muestra un error en consola de texto
        consoleInit(nullptr);
        printf("No se pudo iniciar SDL2. Pulsa + para salir.\n");
        PadState pad;
        padInitializeDefault(&pad);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
            consoleUpdate(nullptr);
        }
        consoleExit(nullptr);
    }
    gfx::exit();

    net::exit();
    spsmExit();
    splExit();
    setsysExit();
    plExit();
    socketExit();
    return 0;
}
