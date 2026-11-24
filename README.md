# BagoNX

Gestor todo en uno para Nintendo Switch con homebrew, escrito desde cero en C++ con libnx y SDL2.

## Funciones

- **CFW**: descarga e instala la última versión de Atmosphère (con `fusee.bin`) y de Hekate desde sus GitHub oficiales. Copia Hekate como `reboot_payload.bin` y nunca toca tu `hekate_ipl.ini`. También tiene un botón para reiniciar la consola.
- **Apps**: tienda con el catálogo público de la Homebrew App Store. Puedes filtrar por categoría (X), buscar con el teclado (Y), y instalar, actualizar o desinstalar apps. Registra las apps en el mismo sitio que la App Store (`/switch/appstore/.get/packages/`), así que las dos apps reconocen lo que instaló la otra.
- **Ajustes**: idioma español/inglés, tema oscuro/claro y autoactualización desde las releases de tu repositorio de GitHub.
- **Acerca de**: muestra el firmware, la versión de Atmosphère en uso, el espacio libre en la microSD y si la app corre en modo applet o aplicación.

Controles: L/R cambian de pestaña, A elige, B vuelve atrás y + sale.

## Compilar

### Opción A: GitHub Actions (sin instalar nada)

1. Sube esta carpeta a un repositorio de GitHub.
2. Cada push compila el `.nro` y lo deja en *Actions → Artifacts*.
3. Al crear una etiqueta `v1.0.0`, se publica una release con `bagonx.nro`. La autoactualización de la app usa esas releases, y el workflow ya le pasa el nombre del repositorio.

### Opción B: en tu PC

1. Instala devkitPro: https://devkitpro.org/wiki/Getting_Started
2. Instala las librerías:
   ```
   dkp-pacman -S switch-dev switch-sdl2 switch-sdl2_ttf switch-freetype switch-curl switch-zlib
   ```
3. Compila desde la terminal *MSYS2 de devkitPro*:
   ```
   make UPDATE_REPO=tu-usuario/bagonx
   ```

## Instalar en la consola

Copia `bagonx.nro` a `sd:/switch/bagonx/bagonx.nro` y ábrelo desde el Homebrew Menu. Para tener toda la memoria disponible, ábrelo con R pulsado sobre un juego.

## Archivos que usa

| Ruta | Para qué |
|---|---|
| `/config/bagonx/config.json` | idioma, tema y `update_repo` |
| `/config/bagonx/tmp/` | descargas temporales |
| `/switch/appstore/.get/packages/` | registro de apps instaladas (compatible con la App Store) |

## Icono

Pon un `icon.jpg` de 256×256 en la raíz del proyecto. Si no hay ninguno, se usa el icono por defecto de libnx.
