# Arquitectura

## Capas

- `core`: modelo vectorial, geometria, hit testing, historial y preferencias. No
  depende de ventanas y se prueba de forma determinista.
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
