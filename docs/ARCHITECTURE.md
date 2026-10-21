# Arquitectura

## Pizarra editable — 2.16.0

`elite_pen/board.hpp` y `src/board.cpp` contienen cámara, objetos, historial y
serialización independientes del escritorio. Las operaciones comparten objetos
inmutables y recursos PNG; una operación nueva invalida el rehacer. El borrador
agrupa el gesto en una sola operación. La presión acompaña a cada punto del trazo;
no se simplifican puntos eliminando su alineación con las muestras de presión.

`src/board_window.inl` añade una ventana Win32 normal y una ventana transitoria
para seleccionar capturas. Comparte el dispositivo gráfico y el renderizador de
figuras, texto y emojis, pero conserva herramientas, colores y documento propios.
Oculta temporalmente las superficies del escritorio sin reescribir sus ajustes.
Minimizar o cerrar la ventana vuelve a esas superficies y conserva la sesión.

La cámara transforma coordenadas de mundo a viewport. Una caché GPU del tamaño
del viewport evita reservar un lienzo gigantesco; incorpora solo el último objeto
en añadidos consecutivos y vuelve a dibujar objetos visibles al navegar. Las
imágenes conservan su PNG original y usan previsualizaciones GPU de hasta 4.096 px
por lado, con presupuesto de caché de 128 MiB. WM_POINTER lee el historial del
lápiz en orden temporal y normaliza presión a 0–1. El ratón pasa por el mismo
recorrido con presión constante. Desde 2.16.1 se aceptan también contactos de
lápiz entregados como ratón por controladores de compatibilidad. Solo se
suprimen duplicados mientras existe un contacto WM_POINTER propio; los contactos
sobre controles se siguen hasta su liberación, sin convertirlos en tinta.
Las secuencias no adquiridas conservan el manejo predeterminado de Windows;
la captura de ratón no cancela por sí sola una secuencia de lápiz.

`.epboard` v1 contiene cabecera, versión, tamaño y checksum FNV-1a de integridad
(no criptográfico), cámara, ajustes, recursos y objetos. El lector valida límites,
IDs únicos, geometría finita, presión, tipos y longitudes antes de reemplazar el
documento. WIC valida dimensiones reales y decodificación de las imágenes.
La escritura usa un archivo temporal, FlushFileBuffers y reemplazo atómico;
solo una versión anterior válida sustituye `.previous`. Un guardado fallido deja
el original intacto. Los archivos no ejecutan acciones ni abren rutas embebidas.

La recuperación usa una instantánea compartida escrita en segundo plano tras
una pausa de dibujo o cada quince segundos de actividad continua. Solo incluye
objetos incorporados al documento; la vista previa del gesto activo sigue siendo
transitoria. Cerrar/minimizar espera y guarda la última revisión; Nueva y
Abrir archivan primero el documento anterior. Las carpetas proceden de la ruta
real o portable de preferencias. No se abre automáticamente una pizarra privada
al arrancar. Una nueva acción global nombrada `HotkeyBoardOpen` llega sin asignar;
las claves anteriores siguen estables y el zoom conserva diez acciones contextuales.

## Capas

- `core`: modelo vectorial, geometria, generación determinista de polígonos,
  hit testing, historial y preferencias. No depende de ventanas y se prueba de forma
  determinista.
- `platform/windows`: ciclo Win32, DPI, atajos, bandeja, entrada y ampliacion.
- `graphics`: dispositivo D3D11/D2D compartido y superficies DirectComposition con
  alfa premultiplicado.
- `ui`: superficie de control intercambiable Paleta/Lineal, selector de herramientas,
  selector de color y configuracion. `control_layout.hpp` concentra geometría pura,
  hit zones y contratos de tamaño; `PaletteWindow` mantiene una sola ventana y un
  único enrutador de comandos para ambas presentaciones.
- `capture`: seleccion de region, captura GDI con respaldo DXGI Desktop Duplication,
  transferencia al portapapeles y codificacion PNG mediante Windows Imaging Component.
- `zoom`: control Magnifier nativo con salida DWM grabable al detectar OBS,
  región circular y aro DirectComposition para Lente, superficie de tinta, documento
  contextual para congelación y documento fuente/caché dispersa para Zoom editable.

## Flujo de datos

La entrada de la superposicion se transforma a coordenadas del escritorio virtual.
El controlador produce una operacion de documento. El historial la aplica y notifica
una invalidacion. El renderizador vuelve a componer pizarra, documento y vista previa;
la paleta conserva su propia superficie y nunca se mezcla con el documento.
Pentágono y Hexágono conservan solo las dos esquinas del área de arrastre en el
documento; el núcleo deriva sus cinco o seis vértices en sentido horario desde el
vértice superior. Renderizado e hit testing consumen la misma función, por lo que el
borrador coincide con el contorno presentado sin almacenar puntos redundantes.
Los glifos de Flecha y Flecha curva reutilizan `arrow_head_points`, pero declaran una
escala óptica de icono que reduce el límite mínimo del cabezal de 12 a 6 px. Así ambos
dibujan las dos alas simétricas y la curva conserva la tangencia sin alterar la
geometría ni la accesibilidad de las flechas creadas en el documento.
Cuando cambia la herramienta, las superposiciones recuperan su modo de entrada y el
controlador restablece despues la paleta en la cima del grupo topmost. Asi sus zonas
accionables siguen recibiendo clics mientras el lienzo esta en modo de dibujo.

Durante zoom vivo, las superposiciones del escritorio dejan pasar la entrada. Al
pulsar el atajo en vista completa, un factor de presentación transitorio interpola
de 1x al valor configurado durante 180 ms con ease-out y ancla la fuente al punto de
invocación. El estado persistente no cambia y cualquier acción inmediata fuerza el
cuadro final antes de continuar. Al pulsar `P`, se inmoviliza el refresco del control
Magnifier y se copia su último cuadro
a la superficie `ZoomInk`; ésta recibe ratón, lápiz y tacto y conserva un `Document`
independiente. Al reanudar, la superficie vuelve a ser transparente, mantiene sus
vectores visibles y el control nativo continúa siguiendo el puntero. No se realizan
capturas continuas de CPU durante el zoom vivo. Con OBS abierto y una ventana externa
compatible, el host reemplaza visualmente Magnifier por un thumbnail DWM vivo,
recortado al mismo origen y escalado al viewport. Windows Graphics Capture incluye
esa composición; DXGI Desktop Duplication la omite en el hardware híbrido validado.

Al pulsar `E`, `ZoomInk` cambia a un documento independiente en coordenadas de la
fuente ampliada. En `MANO` la superficie de tinta se oculta, la raíz Magnifier deja
pasar ratón y teclado a la aplicación real y el origen continúa siguiendo el puntero;
en `LÁPIZ` copia una sola vez el cuadro ampliado, conserva origen
y factor mientras recibe gestos y libera esa instantánea al volver a navegar. La
vista transforma bloques GPU ya rasterizados en vez de reconstruir geometría durante
el paneo. La congelación `P` continúa usando su documento y su instantánea originales.

## Zoom de enfoque — 2.15.0

`elite_pen/focus.hpp` contiene geometría y escenas puras: ocho zonas de escritorio,
ocho máscaras por escena, validación finita, recolocación entre monitores, ajuste
de proporciones a 1–8× e interpolación smoothstep de rectángulos. Preferencias guarda
`FocusLast`, `FocusZone1–8`, duración, aislamiento y visibilidad por claves propias.
Las veinticuatro acciones nuevas preceden las diez contextuales; los nombres de
las claves anteriores no cambian. Una nueva combinación que colisiona con una
personalización anterior se deja vacía, conservando la personalización.

`src/focus_zoom.inl` se incluye dentro de la implementación Win32 tras declarar
las ventanas. `ZoomWindow` mantiene una máquina de estados de enfoque independiente
de factor, vista e inversión clásicos. Su temporizador de 16 ms interpola origen y
tamaño, sin depender del cursor ni capturar cuadros de CPU. Cuando el encuadre se
estabiliza conserva geometría y factor, pero refresca los píxeles de Magnifier;
DWM continúa vivo y actualiza el recorte si su ventana se mueve o desaparece.
Al pasar entre zonas conserva las máscaras de origen y destino durante la transición.

`FocusWindow` es un HUD DirectComposition por encima del zoom y por debajo de los
paneles y la paleta. Convierte selección y máscaras desde viewport a escritorio;
oscurece el exterior y dibuja máscaras opacas. Su barra permanente responde a DPI,
ocho espacios de zona y menús de guardado y velocidad. Se excluye del Magnifier.
La entrada permanece en el modo presentación para evitar entregar a la aplicación
real clics con coordenadas ampliadas. Esc cancela selección antes de salir.

`scripts/focus-zoom-smoke-test.ps1` usa una copia portable y una fuente animada
independiente. Su opción `-Recordable` ejercita el compositor DWM existente sin
necesitar iniciar OBS. Las variables de QA exigen instancia aislada; la ruta normal
mantiene su selección de backend. Se distinguen los píxeles grabables de las consultas
de `MagGetWindowSource`, porque GDI no captura la composición final de Magnifier.

## Emojis — 2.14.0

`elite_pen/emoji.hpp` declara 72 secuencias Unicode con IDs estables y ocho espacios
de favoritos. Solo se añaden IDs al final. Preferencias valida los índices al cargar,
conserva los espacios vacíos (-1) y guarda atajos por sus claves, sin mover los de
las versiones anteriores. Nueve acciones globales preceden las diez contextuales.

`Tool::Emoji` se añade al final del enum y `Drawable::text` conserva una copia de
su secuencia. Dos puntos definen la caja; los eventos de entrada normal y ZoomInk
producen cuadrados de 64 px al hacer clic y convierten longitudes en Zoom editable.
El borrador selecciona la caja y el historial trata todo el emoji como una operación.
El renderizador compartido usa un formato Segoe UI Emoji de 64 DIPs, alineación central
y una transformación de caja; Direct2D recibe ENABLE_COLOR_FONT. Se conservan las
cachés GPU existentes y no se descargan ni redistribuyen fuentes o imágenes.

`EmojiWindow` es una superficie DirectComposition independiente. Reutiliza las seis
escalas y límites seguros de los controles flotantes; invierte la transformación para
clics, muestra tres páginas de catálogo y ocho favoritos. Su tamaño base es 366×336.
El controlador integra su HWND en exclusión Magnifier, detección de clics de panel,
orden topmost, tema y política de captura. Fijación, visibilidad y posición se guardan
por separado para que un cierre explícito no reaparezca al reiniciar.

Referencia de la API de color: https://learn.microsoft.com/windows/win32/directwrite/color-fonts

## Decisiones

### ADR-001: C++ nativo y DirectComposition

Se usa C++20/Win32 para minimizar tiempo de inicio, memoria, latencia y dependencias.
DirectComposition permite ventanas transparentes aceleradas sin copiar cada cuadro a
memoria de CPU.

### ADR-002: coordenadas virtuales en DIPs

El modelo guarda DIPs relativos al origen del escritorio virtual. La capa de ventana
convierte desde/hacia pixeles por monitor. Esto evita que el contenido cambie de
tamano al trasladar la paleta o modificar el escalado.

### ADR-003: historial por operaciones

El historial conserva operaciones reversibles, no imagenes completas. Una limpieza
guarda los objetos retirados y un borrado guarda indice y objeto, lo que mantiene
deshacer exacto con menor uso de memoria.

`Ctrl+Z` / `Ctrl+Y` son alias contextuales, no registros globales permanentes.
Un hook `WH_KEYBOARD_LL` filtra únicamente Z/Y y envía la operación a la cola de
interfaz; no dibuja ni modifica documentos dentro del callback. Es necesario
porque las superficies de tinta usan `MA_NOACTIVATE`: la app subyacente puede
conservar el foco mientras Elite Pen recibe los trazos. El filtro deja pasar
las teclas en Interact, zoom vivo/Mano, texto activo, configuración y diálogos.
Las asignaciones explícitas del usuario tienen prioridad. `HistoryShortcutRouter`
conserva la propiedad de cada pulsación hasta soltar la tecla, sin repetir acciones.
El hook se libera al cerrar; el texto tecleado no se almacena ni se registra.

`scripts/history-shortcuts-smoke-test.ps1` inyecta teclas reales con `SendInput`
contra una ventana temporal que cuenta los eventos recibidos. Verifica cambios
en los documentos y ausencia/presencia de eventos en la app de fondo. Requiere
cerrar otras instancias y confirma el foco antes de cada inyección. La copia de
QA únicamente acepta eventos etiquetados; el smoke habitual no instala el hook.

### ADR-004: herramienta portable reproducible

La compilacion se fija a LLVM-MinGW/UCRT x64 y se valida con SHA-256. La herramienta
queda ignorada por Git; su version y procedencia viven en `tools/toolchain.json`.

### ADR-005: configuracion portable o por usuario

La presencia de `portable.flag` mueve el archivo de preferencias a `data` junto al
ejecutable. La version instalada usa LocalAppData. Ambas rutas mantienen el mismo
formato y no requieren registro, servicio ni permisos de administrador.

### ADR-006: documento contextual de zoom

La tinta de una sesión de zoom no entra en el documento del escritorio. La congelación
`P` conserva coordenadas de viewport, porque su imagen no se desplaza; Zoom editable
conserva un segundo documento en coordenadas de fuente, porque su contenido sí navega.
Esta frontera hace contextuales Limpiar, Deshacer y Rehacer, mantiene compatibles las
sesiones anteriores y evita que una operación de un modo contamine el otro.

### ADR-007: atajos declarativos y registro transaccional

Las ocho acciones globales se almacenan como modificadores y tecla virtual. Al cambiar
una combinación se desregistran y registran todas como una transacción; si Windows
rechaza alguna, se restaura el conjunto anterior. El formato INI es idéntico en las
ediciones instalada y portable.

### ADR-008: temas mediante tokens compartidos

La interfaz nativa conserva C++/Win32 y DirectComposition. Un contrato único de
tokens traduce el sistema visual de Elite Slides a colores Direct2D y GDI para los
temas Oscuro y Claro. La preferencia `Theme` vive en el mismo INI portable o local;
el cambio invalida todas las superficies y renueva el chrome nativo sin reiniciar.

### ADR-009: anotaciones ancladas y caché dispersa en Zoom editable

`ZoomViewportTransform` define de manera pura las conversiones entre viewport y
fuente, incluidos origen negativo, escala fraccionaria y longitudes. Los objetos se
guardan en la fuente y se rasterizan en bitmaps Direct2D de 512 × 512 px identificados
por coordenadas enteras con signo. La estructura solo crea los bloques tocados: evita
un bitmap del escritorio virtual completo y limita el atlas a 160 bloques (~160 MiB)
con respaldo vectorial si se alcanza el máximo. Append actualiza el nuevo objeto;
Clear libera bloques; Undo, Redo y borrado
reconstruyen una sola vez. Un cuadro de navegación filtra bloques visibles y ejecuta
solo `DrawBitmap` con la transformación actual.

La máquina de estados es explícita: `Off`, `Navigate`, `Annotate`. Solo `Annotate`
retira `WS_EX_TRANSPARENT`, muestra `ZoomInk` y captura gestos; `Navigate` deshabilita
el capturador de clic, oculta la tinta, hace transparentes a entrada la raíz y el hijo
Magnifier y devuelve el foco a la última aplicación externa. La barra se excluye del
filtro de Magnifier para no reaparecer dentro de la ampliación, pero tanto ella como
la raíz, la tinta y el indicador de lupa usan `WDA_NONE`: OBS debe capturar lo que el
presentador ve. El orden topmost se recompone como Magnifier, tinta, barra y paleta.

### ADR-010: lente circular separada del factor de ampliación

La vista `L` mantiene un viewport cuadrado para que Magnifier y la ruta DWM conserven
una transformación uniforme, pero aplica la misma región elíptica Win32 a la raíz y
a `ZoomInk`. Así se recortan zoom vivo, instantánea y anotaciones sin máscaras por
cuadro ni lecturas de CPU. `ZoomLensFrame` es una superficie DirectComposition
transparente, excluida de Magnifier y capturable con `WDA_NONE`; dibuja únicamente
anillos alfa de bajo costo y deja pasar toda la entrada.

`ZoomLensDiameter` es una preferencia independiente de `ZoomFactor`. Los tamaños
discretos evitan estados ambiguos en Configuración; `Shift+rueda` y `[` / `]` cambian
el área visible, mientras rueda y `+`/`-` conservan su contrato de aumento. En cada
monitor el diámetro se limita al espacio físico disponible.

Desde 2.10.2 la fuente lógica de `L` permanece centrada en el puntero aunque rebase
el monitor. `clip_zoom_source` intersecta los píxeles disponibles y calcula un destino
proporcional: Magnifier recibe una fuente válida y un hijo desplazado/recortado;
DWM recibe ambos rectángulos sin recentrar la miniatura. El fondo neutro de la raíz
cubre el espacio exterior al monitor. La fuente lógica sigue siendo la referencia
para tinta, congelación y foco; `F` y `D` mantienen su clamp. El cálculo es constante,
sin capturas por cuadro ni nuevas superficies GPU.
