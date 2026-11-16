# Guía de usuario de TeoPCE

[Inicio del proyecto](../README.md) · [Recorrido visual](TOUR.md)

**Guías:** [English](USER-GUIDE.md) · [Deutsch](USER-GUIDE-DE.md) · [Français](USER-GUIDE-FR.md) · Español · [Italiano](USER-GUIDE-IT.md) · [Українська](USER-GUIDE-UK.md)

La interfaz de la aplicación está actualmente en inglés. Estas instrucciones están traducidas; los nombres de los botones coinciden con los de la aplicación.

Para **Catalog Manager 0.2.0-rc4** y **TeoPCE 0.9.13**. Crea tu propio catálogo con tus propios juegos: no necesitas la colección del autor, una suscripción de IA ni una clave de API.

![Cuadrícula de carátulas de la biblioteca en la consola](images/console-grid.png)

*Vista previa de la interfaz de consola en un emulador; colección de ejemplo en el ordenador. [Origen de las capturas](SCREENSHOTS.md).*

## Antes de empezar

Utiliza una **PC Engine / CoreGrafx con Turbo EverDrive Pro**, una tarjeta SD que ya funcione en el menú nativo de EverDrive y tu propia BIOS de CD funcional para los juegos en CD. Sigue las instrucciones de Krikzz para el cartucho, el menú y el firmware; TeoPCE no instala ni actualiza el firmware.

Haz una copia de seguridad de la SD, especialmente de las partidas de `edturbo` y del estado existente en `00TEOPCE/user`. La copia automática del gestor cubre **TeoPCE.pce y 00TEOPCE**, no toda la tarjeta. Conserva el sistema de archivos que ya funciona: instalar TeoPCE no requiere formatear.

Apaga la consola antes de extraer la tarjeta. Utiliza un lector de tarjetas para ordenador; no es necesaria la depuración USB del cartucho.

## Descargar y abrir

Descarga el ZIP de tu plataforma desde la [versión RC4](https://github.com/marvelfamily/TeoPCE/releases/tag/v0.2.0-rc4) y extráelo por completo. El archivo de código fuente que GitHub genera automáticamente está destinado al desarrollo y no es una aplicación independiente.

**Mac:** abre `TeoPCE Catalog Manager.app`. Esta descarga es para Apple Silicon, está firmada ad hoc y no está notarizada. Si se bloquea el primer inicio, comprueba la descarga y la suma de verificación y, si aparece la opción, utiliza **System Settings → Privacy & Security → Open Anyway** en macOS. No desactives Gatekeeper globalmente. No hay una versión empaquetada para Mac Intel.

**Windows:** ejecuta `TeoPCE Catalog Manager.exe` después de extraerlo, no la copia que aparece al abrir el ZIP. No está firmado, por lo que Windows puede mostrar una advertencia de aplicación no reconocida. Revisa el código fuente y la suma de verificación antes de decidir ejecutarlo. RC4 se probó mediante emulación x64 en Windows 11 ARM64; una compilación posterior alojada en GitHub también superó su comprobación de inicio. Sigue pendiente una prueba manual completa del binario distribuido en Windows Intel/AMD nativo.

Las aplicaciones empaquetadas no necesitan una instalación independiente de Python. Para verificar un ZIP, compara su hash completo con `SHA256SUMS.txt` de la misma versión:

```sh
# macOS Terminal
shasum -a 256 TeoPCE-Manager-0.2.0-rc4-macOS-arm64.zip
```

```powershell
# Windows PowerShell
Get-FileHash .\TeoPCE-Manager-0.2.0-rc4-Windows-x64.zip -Algorithm SHA256
```

## Seleccionar y preparar

En **Your card**, pulsa **Choose card…** y selecciona la **raíz de la SD que contiene edturbo**, no la propia carpeta `edturbo`. Si se detectó una tarjeta automáticamente, comprueba la ruta mostrada.

Pulsa **Prepare collection**. El gestor busca juegos, calcula hashes del contenido, valida las referencias a pistas de CD, busca coincidencias de metadatos y trucos y descarga las imágenes disponibles. Las colecciones grandes de CD tardan porque se leen todas las pistas. **Activity** muestra el trabajo real; mantén la tarjeta conectada.

La preparación crea un paquete independiente en el ordenador y no instala nada en la SD. Antes de instalar, revisa **View skipped games & warnings**.

### Estructura de archivos y formatos

Los juegos pueden permanecer en sus carpetas. Ejemplo, no es obligatorio usar estos nombres:

```text
SD root/
├── edturbo/               native EverDrive data, BIOS and saves
├── HuCard/Example.pce
├── CD/Example/
│   ├── Example.cue
│   ├── Track 01.bin
│   └── Track 02.bin
├── TeoPCE.pce             added by installation
└── 00TEOPCE/              added by installation
    ├── data/              catalog, images, details and attribution
    ├── cheats/            per-game packs
    └── user/              settings and cheat selections
```

- Usa archivos `.pce` o `.cue` junto con **todas** las pistas a las que hacen referencia; los nombres y las mayúsculas deben coincidir con el CUE.
- ZIP, 7z, CHD, CCD y M3U no se admiten como entrada directa. Primero extrae o convierte tus propios soportes.
- `MODE1/2048` requiere una conversión a sectores sin procesar verificada por separado. El gestor lo indica, pero no reescribe el disco.
- Las rutas relativas de los juegos deben usar ASCII imprimible y tener menos de 128 bytes. Se admiten como máximo 1.200 candidatos para el catálogo.
- Los archivos de SuperGrafx pueden reconocerse, pero se ocultan en CoreGrafx; su lanzamiento no está admitido en esta versión.
- Distintas revisiones de HuCard con el mismo nombre base pueden entrar en conflicto en el almacenamiento nativo de partidas; los conflictos comunicados requieren cambiar los nombres de forma deliberada.
- Las carpetas ocultas o cuyo nombre empieza por guion bajo, `edturbo` y `00TEOPCE` quedan excluidas del análisis normal de juegos.

## Revisar y editar

![Colección de juegos en Catalog Manager](images/manager-collection.png)

**Collection** ofrece búsqueda, recuentos de cobertura y **Missing covers**. `Cmd+F` / `Ctrl+F` lleva el foco a la búsqueda; `Esc` la borra. Selecciona un juego para consultar **Game card**, **Edit** o **Cheats**.

En Edit puedes cambiar título, año, categoría y descripción, o usar **Choose cover…** (PNG/JPG/JPEG). Mantén disponible la imagen elegida para la preparación. Si no conoces el año, déjalo vacío. Usa texto conciso en inglés: el conjunto limitado de caracteres de la consola normaliza los caracteres no ASCII y puede eliminarlos.

Pulsa **Save edits → Refresh collection**, vuelve a revisar y después instala. Una edición por sí sola no actualiza la tarjeta; la instalación permanece desactivada mientras el resultado preparado no esté actualizado.

### ¿Qué requiere conexión?

| Contenido | Funcionamiento |
| --- | --- |
| Datos y trucos | Instantáneas incluidas y fijadas; no se necesita cuenta |
| Carátulas y capturas | Las imágenes disponibles de Libretro Thumbnails se descargan y guardan en caché local |
| Descripciones en prosa | Se introducen en Edit o proceden de una referencia preparada de tu propiedad mediante la CLI |
| Imágenes ausentes | Se indican claramente; puedes añadir tu propia carátula |
| Trabajo sin conexión | Solo datos incluidos, caché e imágenes locales; las imágenes no disponibles siguen faltando |

La cobertura no es completa para todas las ediciones. No se inventan datos. Los juegos se leen localmente y no se suben; las solicitudes de imágenes identifican el título solicitado ante el servidor que las proporciona. Consulta el registro de actividad y el informe para conocer las fuentes y las imágenes ausentes.

## Instalar e iniciar

Selecciona **Install on card**, confirma el aviso y mantén la SD conectada hasta que aparezca **Installed and verified**. El instalador comprueba si han cambiado los juegos o el estado de la aplicación, verifica una copia de seguridad en el ordenador, prepara la nueva aplicación y verifica el conjunto instalado. Ante fallos detectados intenta revertir los cambios; si se desconecta la tarjeta o se corta la corriente, igualmente hay que comprobar la tarjeta y la copia de seguridad.

Expulsa la tarjeta de forma segura desde el sistema operativo, introdúcela en la consola apagada, inicia el menú de EverDrive y ejecuta **TeoPCE.pce** desde la raíz. La ROM y `00TEOPCE` deben proceder de la misma generación preparada. No mezcles archivos individuales del catálogo ni uses el catálogo generado por otra persona.

## Controles en la consola

El pie de pantalla muestra los controles de la pantalla actual.

| Pantalla | Controles |
| --- | --- |
| Lista de juegos | Cruceta: seleccionar/página; **I** detalles; **SELECT** búsqueda/filtros; **RUN** jugar |
| Detalles | **I** imagen; **II** volver; **SELECT** menú del juego; **RUN** jugar; arriba/abajo texto; izquierda/derecha juego |
| Menú del juego | Iniciar, favorito, trucos, información del archivo, volver a EverDrive |
| Trucos | **I** activar/desactivar e instrucciones; **SELECT** opciones; **II** cancelar; izquierda/derecha página; **RUN** guardar y volver |

### Navegación, búsqueda y filtros

Pulsa **SELECT** en la biblioteca para abrir **FIND AND FILTERS**. Arriba/abajo selecciona una opción; izquierda/derecha la cambia. **I** aplica los ajustes o abre el selector de título, género o letra en esas filas. **II** cancela. **RUN** borra las opciones de filtro; aplícalas para volver a todos los juegos.

| Opción | Qué puedes elegir |
| --- | --- |
| Title prefix | Escribe el comienzo de un título con el teclado en pantalla, hasta 12 caracteres; RUN busca |
| Library | Todos los juegos, favoritos o abiertos recientemente |
| Category | Lanzamientos originales, traducciones, homebrew, museo, herramientas/pruebas, sin licencia o todas las categorías |
| Media | HuCard, CD-ROM o ambos |
| Genre | Selección en una lista paginada, por ejemplo Action, Adventure, Platform, Puzzle, RPG o Shoot'em Up |
| Letter | A–Z, títulos que empiezan por números o símbolos mediante `#`, o todas las letras |
| Region/lang | Japón, EE. UU., parche en inglés, otros o todas |
| Cheats | Cualquier juego o juegos con trucos seleccionables |
| View | Lista con imágenes o cuadrícula de seis carátulas |
| Theme | Neon, Ember, Ice, Classic |

Los filtros se combinan. Por ejemplo, elige **Original releases + HuCard + Action**. La biblioteca normal se ordena de A a Z; Recently opened usa el orden del historial. Esta versión no permite ordenar por año ni por editorial. Los filtros solo cambian la lista mostrada; no mueven archivos. **II** en una biblioteca filtrada elimina los filtros.

El título seleccionado aparece sobre la lista o cuadrícula y los nombres largos ocupan varias líneas. Los detalles muestran soporte, región, género, año cuando se conoce y la descripción y los datos disponibles del desarrollador y la editorial. **I** cambia entre la carátula y la captura del juego cuando están disponibles; arriba/abajo desplaza el texto e izquierda/derecha cambia de juego.

Añade o elimina un favorito mediante **Details → SELECT → Add/Remove favorite**. Se conservan hasta 64 favoritos y ocho juegos abiertos recientemente. El historial registra las fichas de juegos abiertas, no el tiempo de juego. Al reiniciar, TeoPCE selecciona la entrada disponible más reciente del historial en la biblioteca sin filtros. También se guardan las preferencias de vista y tema.

Unas etiquetas breves describen el filtro actual: `FAV` favoritos, `OPEN` recientes, `B*` títulos que empiezan por B, `R:JP` Japón, `R:US` EE. UU., `L:EN` parche en inglés y `CH` trucos seleccionables. El área del título también identifica el soporte y la región del juego seleccionado; `*` marca un favorito.

**Details → SELECT** también abre las acciones del juego: iniciar, favoritos, trucos, información del archivo y volver a EverDrive. La información del archivo ayuda a identificar la entrada y la ruta seleccionadas del catálogo al comunicar un problema.

## Trucos

El gestor busca automáticamente los paquetes correspondientes. Su pestaña Cheats muestra una vista previa de los grupos; la activación se hace **en la consola**:

1. Juego → **I** detalles → **SELECT** menú del juego → **CHEATS**.
2. Selecciona un grupo, pulsa **I** y lee las instrucciones que aparezcan.
3. **RUN** guarda la selección y vuelve. Inicia el juego.
4. **SELECT → All cheats off** desactiva y guarda inmediatamente todos los grupos de ese juego. No es necesario pulsar RUN de nuevo.

Las selecciones guardadas se conservan; el menú indica los códigos activos. **II** cancela los cambios de selección no guardados.

### Coincidencias, límites y HELD

La biblioteca incluida contiene **1.177 grupos completos para 347 ediciones exactas**. La correspondencia de HuCard usa el hash del archivo completo. La de CD incluye los bytes exactos del CUE y todas las pistas referenciadas, incluido el audio. Otras revisiones, traducciones o CUE creados de nuevo pueden no coincidir. No cambies el nombre de un paquete para forzar la compatibilidad.

**53 asociaciones de CD también requieren la ruta relativa registrada**, porque las políticas de inicio compiladas dependen de ella. El gestor muestra esa ruta cuando es distinta. Mover un juego instalado puede afectar al estado guardado: usa una configuración nueva con copia de seguridad o realiza una migración deliberada.

Algunas combinaciones tienen protecciones de inicio de CD incompatibles y se bloquean. El cartucho también tiene un límite de 16 ranuras; un grupo puede consumir varias y se mantiene completo. Reduce la selección cuando sea necesario.

**HELD significa problemas conocidos o datos no admitidos**, no simplemente «sin probar en consola». Estas entradas están ocultas por defecto y pueden consultarse con **Show held codes**. Las entradas retenidas por una política que cumplen los requisitos ofrecen **Accept risk** con una explicación; los códigos incorrectos o no admitidos no pueden desbloquearse. Las comprobaciones de combinaciones siguen aplicándose. Ocultar HELD no desactiva los códigos activos.

Las instrucciones pueden exigir una acción en la pantalla de título, un personaje o un modo. Un arranque correcto no demuestra el efecto en todos los niveles. Si aparece un error de BIOS con un código, desactiva los códigos, compara el inicio nativo e informa del grupo y la revisión exactos.

**Import extra cheats…** acepta bibliotecas JSON de TeoPCE de versión 1, no archivos arbitrarios de emulador. Las importaciones explícitas tienen prioridad sobre las coincidencias del contenido incluido; los paquetes y selecciones ya instalados tienen prioridad sobre ambos durante las actualizaciones. Consulta [CHEAT-LIBRARY.md](CHEAT-LIBRARY.md).

### Partidas e IGM

El IGM nativo de HuCard y el guardado y carga rápidos se probaron en el hardware del propietario. IGM sale al **menú de Krikzz**; desde allí vuelve a iniciar TeoPCE.

En la configuración de CD probada **no hay IGM nativo ni estados de guardado rápido**. El guardado normal del juego o de la BIOS es independiente. TeoPCE no añade una superposición durante el juego para activar o desactivar códigos de CD.

## Actualizaciones, almacenamiento y recuperación

Después de añadir juegos: **Prepare / Refresh collection → revisar → Install on card**. Los perfiles y selecciones existentes se conservan cuando su identidad sigue siendo válida. Vuelve a preparar después de jugar si cambió el estado de la aplicación.

Eliminar, renombrar o sustituir juegos instalados no es una migración transparente. Si la biblioteca anterior ya no coincide, conserva la copia de seguridad y resuelve la migración; no borres perfiles para eludir la comprobación. Puedes preparar una tarjeta nueva de forma independiente. Actualizar TeoPCE no actualiza el firmware de EverDrive.

| Dentro de la carpeta personal del ordenador | Finalidad |
| --- | --- |
| `TeoPCE/prepared/` | Generaciones preparadas y `REPORT.json` |
| `TeoPCE/metadata/` | Cambios del editor asociados a la ruta de montaje de la tarjeta |
| `TeoPCE/backups/installation-…/` | ROM y datos anteriores; `RESULT.json` tras una instalación correcta |
| `.cache/teopce-library/` | Caché de descargas |

Conserva los cambios y las copias que necesites. Una nueva ruta de montaje o letra de unidad de Windows puede afectar a la búsqueda de cambios. Los resultados preparados antiguos y la caché pueden eliminarse con el gestor cerrado; al borrar la caché habrá que descargar de nuevo.

Para restaurar: cierra el gestor, copia los actuales `TeoPCE.pce` y `00TEOPCE` en otro lugar y después **sustituye ambos** por la pareja de una misma copia de seguridad. No mezcles generaciones. No modifiques los juegos ni `edturbo`. Una copia de la primera instalación puede no contener ninguno de los dos elementos de la aplicación y no puede restaurar una versión anterior que nunca existió. Expulsa la tarjeta de forma segura después de copiar.

## Solución de problemas

| Síntoma | Siguiente paso |
| --- | --- |
| No se encuentra la tarjeta | Elige su raíz manualmente; confirma que está montada y contiene `edturbo` |
| Se omiten juegos | Lee el informe; comprueba formato, referencias CUE, rutas ASCII, carpetas excluidas y MODE1/2048 |
| También falla en el menú nativo | Diagnostica la imagen, BIOS o tarjeta; TeoPCE no repara el contenido |
| Solo falla el inicio desde TeoPCE | Anota título/ruta, consola/firmware, error exacto y estado con todos los trucos desactivados |
| No hay códigos compatibles | El contenido exacto o la ruta requerida no coincidieron; el inicio normal sigue disponible |
| Instalación desactivada | Guarda los cambios y actualiza; la preparación debe terminar para la tarjeta seleccionada |
| Cambió la tarjeta o la aplicación | Vuelve a preparar; no instales una generación antigua sobre un estado más reciente |
| Falta año o imagen | Prueba la preparación en línea o aporta los tuyos; el año desconocido puede quedar vacío |
| Instalación interrumpida | Conserva copias de seguridad y archivos de preparación, comprueba la tarjeta; nunca borres un bloqueo mientras se ejecuta un instalador |

Utiliza el [formulario de incidencias](https://github.com/marvelfamily/TeoPCE/issues/new/choose) e incluye versiones, pasos, texto del error y comparación con el inicio nativo. Oculta las rutas absolutas personales. No adjuntes juegos, BIOS, partidas ni volcados de la tarjeta.

Las comprobaciones manuales, de plataforma y de hardware pendientes para RC4 están en [RELEASE-STATUS.md](../RELEASE-STATUS.md). La atribución y las condiciones de redistribución están en [THIRD-PARTY.md](../THIRD-PARTY.md); las tarjetas preparadas conservan la atribución de metadatos y trucos en `00TEOPCE/data`.
