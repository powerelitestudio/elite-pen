# Informe de calidad — Elite Pen 2.14.0

## Emojis, favoritos y panel fijo — 2.14.0

Validación local: 2026-10-16.

- Compilación Release sin errores ni advertencias del compilador. Núcleo,
  preferencias y rendimiento CPU aprobados en Debug y Release. Se validan 72 IDs
  únicos, secuencias completas, coordenadas negativas, borrado e historial por
  objeto, favoritos vacíos, recuperación de IDs inválidos y compatibilidad de atajos.
- Smoke completo de interfaz aprobado: Herramientas > Emojis, tres categorías,
  seis escalas de 80 % a 300 %, selección escalada, pin, arrastre, cierre y desfijado.
  El menú de clic derecho se abre y acciona con entrada real de ratón, guarda un
  emoji diferente en favorito 1 y ese cambio sobrevive al reinicio.
- Los editores de Configuración asignan F17–F24 a los ocho favoritos. Eventos
  globales de teclado activan cada emoji desde Cursor; cada clic añade el glifo
  correcto, con un paso de deshacer, rehacer y borrado completo. No se programa ni
  se valida físicamente un macropad externo.
- Se prueban emojis en Zoom editable y congelado. Con zoom vivo, un clic físico
  simulado selecciona el favorito desde el panel fijado sin congelar la vista;
  el panel conserva su lugar por encima del zoom y se excluye de Magnifier.
- El ejecutable instalado se copia a una sesión portable aislada: el panel restaura
  300 %, fijación y posición (-440, -1056) en DISPLAY5 tras reiniciar. Dimensiones
  1098×1008 en la tercera pantalla detectada de 2400×1080; el área de trabajo mantiene
  accesible el panel. No se modifica la resolución ni el escalado de Windows.
- Capturas de las tres categorías y ocho emojis colocados en una pizarra confirman
  los colores nativos. La muestra de lienzo contiene 2.763 muestras cromáticas
  comprobadas automáticamente; también se inspecciona visualmente el resultado.
- Rendimiento GPU, 5.000 trazos / 240 cuadros: población 351,858 ms, primer render
  74,182 ms, cuadro en caché 1,593 ms y dibujo activo 0,987 ms. Zoom editable:
  primer Lápiz 70,929 ms, Mano 0,124 ms y Lápiz caliente 19,833 ms. Memoria 330,06 MiB.
  Todos los límites del benchmark se cumplen.
- La instalación se respalda antes de sustituir sus archivos. El SHA-256 del
  ejecutable instalado, la distribución portable y el build validado coincide:
  `EA953F40D755E55BCC714DC25FC43EF2FAF19C1C63D188D655FA241D35EF060E`.
  El INI real mantiene exactamente su hash durante actualización y validación.

Las evidencias detalladas permanecen en `artifacts/emoji-update-2.14.0-20261016-161957/`,
ignoradas por Git. La actualización de código no publica por sí sola una nueva Release.

## Panel de figuras ampliable y fijo — 2.13.0

Validación: 2026-10-16.

- Núcleo, preferencias y rendimiento CPU aprobados en Debug y Release. Las seis
  escalas de Figuras se guardan independientemente de la paleta; se verifican
  dimensiones, conversión de zonas de clic, coordenadas negativas y valores futuros.
  Las instalaciones anteriores conservan sus atajos y arrancan al 100 % sin fijación.
- Smoke completo de interfaz aprobado con el binario Release final: acceso desde
  Herramientas, los seis tamaños de 293 × 186 a 1098 × 696 px, selección escalada,
  modo fijo, convivencia con otras herramientas y Configuración, y arrastre real
  de la cabecera mediante coordenadas del escritorio.
- Se cierra y reinicia el proceso limpiamente; el panel fijado recupera visibilidad,
  posición y tamaño 300 %. Se prueban el botón de cierre escalado y la vuelta al
  cierre automático al desactivar Fijado.
- Una copia aislada del ejecutable instalado restaura el panel al 300 % en la tercera
  pantalla detectada, situada sobre la principal: posición (-440, -1040), tamaño
  1098 × 696 px. La posición se conserva tras reiniciar y el hash coincide con el build.
- Con zoom vivo se verifica el orden de ventanas y se envía un clic físico simulado
  al panel: cambia la figura sin congelar el zoom. La ventana de Figuras se excluye
  de la fuente del Magnifier y respeta la preferencia de exclusión de capturas.
- Rendimiento GPU de 5.000 trazos y 240 cuadros aprobado: caché 0,525 ms/cuadro,
  dibujo activo 1,122 ms/cuadro, Mano editable 0,105 ms/cuadro y 243,49 MiB.
  Población 307,536 ms; primer cuadro 54,578 ms; primer Lápiz editable 81,854 ms
  y retorno caliente 21,929 ms. Todos dentro de los presupuestos existentes.
- La instalación local se respalda y actualiza conservando los bytes de preferencias.
  Las pruebas usan copias portables aisladas. No se declara una nueva validación
  física de panel táctil, OBS, presión o de toda la matriz Windows 10/11.

Fecha de validación de símbolos y atajos: 2026-10-16.

## Símbolos y macropad — 2.12.0

- Núcleo, preferencias y rendimiento CPU aprobados en Debug y Release. La geometría
  común cubre las cuatro figuras, arrastres invertidos, coordenadas negativas,
  contornos, huecos, punto de advertencia y un solo paso de borrar/deshacer/rehacer.
- Se conservan las claves guardadas de polígonos y zoom; los cuatro símbolos llegan
  sin asignar. F13–F16 sobreviven al guardado y carga de preferencias.
- Smoke completo de interfaz aprobado con el binario Release final. Asigna F13–F16
  desde los editores y envía eventos de teclado a Windows para activar los cuatro
  símbolos desde modo cursor. También verifica volver a capturar un atajo registrado
  sin activar su herramienta anterior y muestra nombres explícitos F1–F24.
- Once figuras en tres filas de 232 px; panel completo de 544 px con Configuración
  accesible. Se prueban selección por iconos, clic para estampar, arrastre y borrado
  completo de cada símbolo. La caja de detección se invalida al completar el clic
  para incluir los 64 px finales y evitar tinta fuera del área cacheada.
- Zoom congelado y Zoom editable admiten los cuatro símbolos y los deshacen en un
  solo paso; el resto del recorrido de texto, capturas, pizarras y zoom sigue pasando.
- Rendimiento GPU con 5.000 trazos y 240 cuadros: caché 0,500 ms/cuadro; dibujo
  activo 0,963 ms/cuadro; Mano editable 0,131 ms/cuadro; memoria 250,33 MiB.
  Población 244,530 ms, primer cuadro 54,826 ms; primer Lápiz editable 68,339 ms
  y retorno caliente 17,371 ms. Todos los presupuestos aprobados.
- Instalación local actualizada a 2.12.0 después de respaldar aplicación, preferencias
  y registro. El ejecutable instalado coincide con el build validado y los bytes de
  preferencias permanecen idénticos. Las pruebas usan copias portables aisladas.
- La activación de macropad se valida mediante teclado simulado; no se declara una
  prueba ni programación de hardware físico. Tampoco una nueva matriz física de
  OBS, presión o Windows 10/11. La última Release pública sigue siendo 2.10.3.

Fecha de validación de tamaños: 2026-10-15.

## Tamaños Extra grande y Gigante — 2.11.0

- Extra grande 200 % y Gigante 300 % amplían Paleta a 348 × 336 y 522 × 504 px,
  y Lineal a 91 × 811 y 137 × 1217 px. Las cuatro opciones anteriores conservan
  sus índices, dimensiones y el valor predeterminado Estándar.
- Pruebas de núcleo, preferencias y rendimiento CPU aprobadas en Debug y Release.
  La persistencia recorre los seis índices y comprueba tamaño contraído, un monitor
  con coordenadas negativas y una superficie menor que el control gigante.
- Smoke completo de interfaz aprobado: seis opciones, dimensiones, selección de
  color y grosor, contracción/expansión y cambio entre ambas presentaciones.
  El selector dispone de ancho suficiente para mostrar las etiquetas completas.
- Rendimiento GPU con 5.000 trazos y 240 cuadros: caché 0,784 ms/cuadro; dibujo
  activo 1,044 ms/cuadro; Mano del zoom editable 0,078 ms/cuadro; memoria 247,75 MiB.
  Población 411,186 ms, primer cuadro 98,598 ms; primer Lápiz editable 140,915 ms
  y retorno con caché caliente 39,239 ms. Todos dentro de los presupuestos.
- Transparencia: geometría y texto inline aprobados. El muestreo de píxeles de la
  paleta no estuvo disponible en esta sesión; no se declara una aprobación óptica
  completa. La tabla del smoke incluye los seis tamaños.
- Instalación del usuario respaldada antes de actualizar; las pruebas de UI usan
  copias portables aisladas y no cambian sus preferencias.
- No se declara una nueva validación física de presión, OBS o de la matriz completa
  Windows 10/11. La evidencia histórica se conserva a continuación.

## Referencia histórica de 2.10.3

Fecha: 2026-09-24
Equipo de referencia: Lenovo 80NV, Intel Core i7-6700HQ, 12 GB RAM, Intel HD 530,
GeForce GTX 960M, Windows 10 Pro 22H2 x64, dos monitores con escalado mixto.

## Historial desde teclado — 2.10.3

- Causa: solo estaban registrados `Ctrl+Alt+Z` y `Ctrl+Shift+Y`. Las superficies
  de dibujo no activan su ventana y no recibían `Ctrl+Z` / `Ctrl+Y` de la app
  que conservaba el foco. El nuevo filtro es contextual, sin cambiar preferencias.
- Compilación Release, pruebas del núcleo y preferencias aprobadas. Pruebas puras
  del ciclo pulsar/repetir/soltar, cambio de modo con tecla mantenida, prioridad
  de asignaciones explícitas y paso de combinaciones con Shift, Alt o Windows.
- `history-shortcuts-smoke-test.ps1`: 47 comprobaciones con `SendInput` real y una
  ventana desechable que conserva el foco. Escritorio, pizarras blanca/negra, línea,
  borrador, limpiar/restaurar, historial vacío, invalidación de rehacer, vista previa
  interrumpida, texto confirmado, zoom congelado y Lápiz del Zoom editable.
- Cursor, zoom vivo, Mano, texto activo y Configuración dejan pasar Ctrl+Z/Y;
  se comprueba que el receptor externo recibe ambas combinaciones y el documento
  de Elite Pen no cambia. Los tests no escriben en aplicaciones del usuario.
- Smoke completo de interfaz aprobado. Se comprueba que Ayuda coincide con la
  versión del ejecutable y se revisa visualmente la explicación en Atajos.
- Rendimiento del núcleo: agregar 5.000 trazos 11,96 ms; 250 borrados fallidos
  24,74 ms; 10.000 ciclos deshacer/rehacer 0,39 ms; limpiar/restaurar 0,48 ms;
  simplificar 100.000 muestras 39,16 ms. Todos dentro de sus presupuestos.
- Las pruebas de congelación usan captura sintética. No se declara una nueva
  validación visual de OBS ni de Windows 11.
- Portable local actualizado a 2.10.3: todos los archivos coinciden por SHA-256
  con el paquete validado, conservando intacto `data/settings.ini`. Respaldo del
  paquete anterior dentro de `artifacts/portable-backups/`, no en Aplicaciones.
  SHA-256 del ejecutable final:
  `2396C14FD172CFE8D48C7975723D0F1D65BC4C5982FC110C73A9EA5F1A265850`.

### Paquetes de distribución — 2.10.3

- Portable e instalador generados desde el mismo ejecutable Release validado.
  Se inspeccionó el ZIP: ocho archivos, sin preferencias del usuario ni datos
  locales, con versión 2.10.3 y hashes internos correctos. LICENSE, NOTICE,
  TRADEMARKS y LEEME coinciden con sus fuentes.
- Instalación silenciosa, inicio, smoke completo de interfaz y desinstalación
  aislados aprobados. No había otra edición instalada al iniciar la prueba.
- Transparencia: geometría y texto inline aprobados; el muestreo de píxeles de
  la paleta no estuvo disponible en esta sesión de Windows y no se da por validado.
- Se repitieron núcleo, preferencias y rendimiento: todos aprobados. La versión
  continúa bajo Apache License 2.0 y los ejecutables no están firmados digitalmente.
- SHA-256 de `Elite.Pen.Portable.2.10.3.zip`:
  `7627BB5F02AC1D26BA74692CF82EDCAFE99CB53F3A39FF48523F2929583A8F43`.
- SHA-256 de `Elite.Pen.Setup.2.10.3.exe`:
  `24A837682D7295E52DDACD0782140D0B9D1FAD6466039D4717F90401D3224FA9`.

## Corrección de bordes de lente — 2.10.2

- Reproducción en 2.10.1: con puntero en `(8, 8)` y lente de 360 px a 2x,
  el foco quedaba en `(90, 90)`. Las esquinas del monitor podían quedar fuera de
  la máscara circular al desplazar la fuente para contenerla en el escritorio.
- Release compilado sin errores; núcleo y preferencias aprobados. Se añadieron
  1.920 combinaciones puras de monitor, diámetro, factor y posición, además de
  recortes parciales de ventanas, ausencia de intersección y preservación de F/D.
- `scripts/lens-edge-smoke-test.ps1`: 80 casos aprobados en los dos monitores
  conectados (cuatro esquinas y cuatro bordes por cinco tamaños). Consulta
  `MagGetWindowSource` dentro del proceso propietario y los rectángulos reales
  de las ventanas para verificar posición, escala y límites aceptados por Windows.
- El punto de entrada determinista solo se acepta con `ELITE_PEN_QA_INSTANCE_ID`;
  permite probar sin competir con el ratón real ni modificar preferencias del usuario.
- Congelar/reanudar en los bordes preserva el foco. Esta comprobación utiliza la
  captura sintética de QA; no equivale a validar los píxeles de una grabación.
- Smoke completo de interfaz Release aprobado: paleta, herramientas, anotación,
  zoom F/L/D, congelación, zoom editable, captura y preferencias.
- La comprobación óptica automatizada no pudo mostrar de forma fiable su fondo
  de prueba en el escritorio capturado; no se declara aprobada. La ruta DWM para
  OBS tiene cobertura geométrica del recorte, sin una nueva grabación OBS validada.
- Rendimiento del núcleo: agregar 5.000 trazos 29,78 ms; borrados fallidos 44,54 ms;
  deshacer/rehacer 0,46 ms; limpiar/restaurar 0,70 ms; simplificar 42,01 ms. Todos
  dentro del presupuesto. No se incorporan capturas continuas ni superficies GPU.
- Portable local actualizado a 2.10.2; se verificaron todos los archivos contra el
  staging y las preferencias conservaron su SHA-256. Ejecutable verificado:
  `8E7F7D0F3DEAA588975DF4D69604BB8A66DD45F996186E9A2CB7B27FBEFE39BC`.

## Cobertura automatizada de base (2.10.1)

- Modelo vectorial: geometria, limites, hit testing, simplificacion, historial,
  borrado compuesto, limpiar, deshacer y rehacer.
- Polígonos 2.10: generación exacta de cinco y seis vértices, orientación superior,
  contorno cerrado, hit testing por arista, centro vacío, modificación explícita y
  entrada inválida segura.
- Interfaz real: inicio, una superposicion por monitor, seis colores directos, cinco
  grosores, punta lapiz/cursor, ojo, pizarras blanca/negra, configuracion, todas las
  herramientas, texto, captura, zoom completo/lente/acoplado, inversion y cierre.
- Iteracion 1.1: orden de los cinco colores rapidos, selector `+`, ancho de paleta,
  prioridad de la paleta sobre el lienzo en modo Lapiz, accesos directos de Texto,
  Figuras y Configuracion, y altura reducida del panel geometrico.
- Regresion 1.1.1: clics enviados deliberadamente a traves del overlay sobre color,
  grosor, ojo, punta, pizarra, Texto, Figuras, Configuracion y Limpiar.
- Iteracion 1.2: sexto acceso directo verde, arco compacto de colores y estado oculto
  representado mediante un ojo cerrado sin `X`.
- Iteracion 1.3: panel de figuras solo con iconos, curva cubica Bezier, texto directo
  y transparente, papelera, grosor inicial configurable, indicador inclinado sin
  fondo, selector `+` sin circulo y paleta de 280 px de alto.
- Iteracion 1.4: paleta principal de 174 x 168 px, escalado uniforme al 60 %, zonas
  de clic y tooltips transformados, union alineada entre ferula y mango, e iconos del
  mango sin fondos circulares.
- Iteracion 1.5: mango sin iconos y 10 % mas corto, clic unificado sobre su segmento,
  panel general con Configuracion y retiro total del indicador inferior.
- Iteracion 1.6: sistema visual Obsidian Atelier, contraste de selección, gradientes
  de bajo costo, estados hover, paneles coherentes y Configuración con marco y
  controles propios. La geometría y las zonas interactivas de la paleta no cambian.
- Iteración 1.7: arrastre en coordenadas de pantalla con prueba anti-oscilación,
  papelera integrada junto al pincel, zonas de clic no solapadas y selectores oscuros
  dibujados a medida en Configuración.
- Iteración 1.8: persistencia de escala integral, dimensiones 139 x 134, 174 x 168,
  218 x 210 y 261 x 252 para Compacta, Estándar, Grande y Muy grande; hit testing
  transformado, pestañas General/Atajos y guía explicada de combinaciones.
- Regresión 1.8.1: la paleta usa composición directa sin superficie de redirección
  heredada; una prueba aislada sobre fondo blanco comprueba por píxel que las cuatro
  escalas mantienen esquinas transparentes y contenido visible.
- Iteración 1.9: cursor de lápiz nativo para Lápiz/Resaltador, rasterizado 4x y
  adaptado por DPI; QA verifica que no sea la cruceta del sistema, que conserve un
  bitmap de color de al menos 32 px y que el hotspot permanezca sobre el grafito.
- Iteración 2.0: `P` congela y reanuda el zoom; QA dibuja sobre la superficie fija,
  comprueba que el lápiz se active, que la tinta sobreviva al reanudar y que Limpiar,
  Deshacer y Rehacer operen sólo sobre el documento del zoom.
- Regresiones 2.0: `Esc` sale del zoom y de ambas pizarras; la paleta entra en modo
  hibernación a 52 x 50 px desde el tamaño estándar de 174 x 168 y vuelve a sus
  dimensiones exactas al expandir.
- Regresión 2.1: el zoom vivo no muestra `ZoomInk` hasta congelar, eliminando la capa
  que podía cubrir de negro Magnifier. Se validan nuevamente seguimiento, tres vistas,
  inversión, vista general, congelación por clic y por `P`, reanudación y salida.
- Atajos 2.1: 32 acciones persistentes, siete filas visibles desplazables, lápiz de
  edición independiente del texto, asignación contextual de una tecla, desasignación,
  detección de conflictos por ámbito y restauración completa de fábrica.
- Compactación 2.1: un clic fuera del icono no expande; el área restante desplaza la
  mini paleta y el icono central conserva su acción exclusiva.
- Regresión 2.1.1: el clic se envía al hijo Magnifier real; la congelación solo se
  acepta cuando la instantánea contiene información visual y la prueba confirma el
  cambio a tinta. La vista Lente debe activar un cursor distinto de la flecha estándar.
- Regresión 2.1.2: dos ejecuciones consecutivas usan entrada física para congelar;
  el overlay normal no intercepta el gesto. Tras el primer trazo se verifican
  visibilidad y orden Z de la paleta, cambio a rojo y selección de Rectángulo.
- Lente 2.1.2: una superficie transparente `ZoomTarget` permanece visible, cambia de
  posición con el puntero, deja pasar entrada y se excluye del filtro de Magnifier.
- Regresión 2.1.3: después de completar el primer trazo se comprueba que `ZoomInk`
  permanezca sobre la raíz nativa `Zoom` y que la paleta permanezca sobre ambas.
  El mismo contrato se repite al cambiar color, elegir geometría y congelar Lente.
- Sincronización 2.1.3: el centro óptico de `ZoomTarget` se compara directamente con
  el centro de la región fuente enviada a Magnifier, incluido el ajuste en bordes.
- Regresión 2.1.4: se completa un trazo sobre pizarra blanca y se comprueba que la
  paleta siga visible, encima del overlay y receptora del input en su punto central.
- Transparencia 2.1.4: el editor de Texto exige `WS_EX_NOREDIRECTIONBITMAP` sin
  `WS_EX_LAYERED`; una prueba visual compara el color real de un punto vacío antes
  y después de abrir el caret para descartar cualquier rectángulo opaco.
- Familia Elite 2.2: los temas Oscuro y Claro comparten los tokens exactos de Elite
  Slides; una prueba real activa ambos desde Configuración y consulta el estado en
  paleta, mientras las pruebas de preferencias verifican su persistencia portable.
- QA visual 2.2: Configuración se inspecciona en grafito y marfil, con estados activos
  violetas, tipografía Segoe UI Variable, superficies frías y contraste legible.
- Atajos 2.4: 39 acciones persistentes; se validan `Q`, `A`, `Z`, `E`, `D`, los seis
  colores directos, ambos accesos al selector completo y la migración selectiva que
  conserva una combinación personalizada de una instalación 2.3.
- Gestos 2.4: pruebas puras comprueban Línea con `Shift`, Rectángulo con `Ctrl`,
  Elipse con `Tab`, Flecha con `Ctrl+Shift` y Flecha curva Bézier con `Ctrl+Alt`,
  además de prioridad y protección de Texto/Borrador/herramientas explícitas.
- Interfaz 2.4: QA real recorre colores por atajo, grosor por rueda, contracción y
  expansión por `Ctrl+Shift+D`, y repite zoom vivo, lente, congelación, tinta y
  orden Z. En escritorios restringidos usa una imagen sintética solo dentro del
  sandbox de pruebas; el binario normal continúa capturando Magnifier realmente.
- Presentaciones 2.5: QA inicia en Paleta, cambia en vivo a Lineal y vuelve sin
  reiniciar; valida 46 × 406 px Estándar, color morado, grosor 12 px y la píldora
  contraída de 46 × 49 px. El round trip portable conserva `ControlMode=1`.
- Revisión visual 2.5: capturas reales verifican Lineal oscuro, claro y contraído,
  además de Configuración con selector de presentación sin superposiciones.
- Armonización 2.5.1: capturas equivalentes verifican que Paleta use los mismos
  fondos, violeta activo, bordes y contraste de Lineal en Oscuro y Claro.
- Limpieza 2.5.2: el ojo conserva su geometría y hit zone sin el círculo exterior.
- Limpieza 2.6: el control de contraer conserva su hit zone, pero elimina el círculo
  decorativo y deja únicamente el glifo de contracción.
- Apariencia 2.6: una instalación portable aislada debe iniciar en tema Claro; el
  cambio a Oscuro y el retorno a Claro siguen siendo inmediatos y persistentes.
- Gestos 2.6: las pruebas unitarias fijan Línea con `Shift`, Rectángulo con `Ctrl`,
  Elipse con `Tab`, Flecha con `Ctrl+Shift` y Flecha curva con `Shift+Tab`.
- Captura 2.6: el zoom congelado compone directamente su instantánea y documento de
  tinta. La prueba UI exige un PNG adicional, presencia real de la línea roja y un
  bitmap de 210 × 130 en el portapapeles.
- Configuración 2.7.1: la prueba UI exige las pestañas `General`, `Atajos` y `Ayuda`,
  verifica el texto accesible de los cinco gestos, la versión 2.7.1, Apache License
  2.0, la autoría de Power Elite Studio y las acciones del sitio y código fuente.
  Capturas reales comprueban que encabezados, filas y contenido no se solapen ni
  queden recortados.
- Publicación 2.7: la revisión de archivos rastreados no encontró patrones comunes de
  secretos ni archivos mayores de 1 MiB. README, licencia, seguridad, contribución,
  plantillas y CI conforman la base de acceso anticipado; el script de Release crea
  ZIP portable, instalador y hashes desde la misma versión.
- Publicación 2.7.1: la licencia Apache 2.0 canónica se compara con la fuente oficial;
  portable e instalador deben contener `LICENSE.txt`, `NOTICE`, `TRADEMARKS.md` y
  metadatos 2.7.1 antes de publicar los hashes SHA-256.
- Zoom editable 2.8: `E` abre la barra Mano/Lápiz sin modificar `P`; QA consulta la
  máquina de estados, orden topmost, herramienta activa y retorno al zoom vivo
  original. Mano exige raíz transparente/no activable, hijo Magnifier deshabilitado
  para hit testing y `ZoomInk` oculto; Lápiz revierte esas condiciones y exige una
  instantánea válida. La tinta usa `WS_EX_NOREDIRECTIONBITMAP` y no `WS_EX_LAYERED`
  para no cubrir el zoom de negro.
- Grabación de zoom 2.8: una prueba gráfica aislada exige `WDA_NONE` en la raíz del
  zoom y en `ZoomInk`, captura el cuadro completo visible y rechaza una salida con
  menos de 3 % de muestras no negras. La validación Release obtuvo 100 %.
- Regresión OBS 2.8: se grabaron MP4 reales con OBS 32.2.1 sobre Windows 10 22H2,
  Intel HD 530 + NVIDIA GTX 960M. Magnifier fue negro tanto en DXGI como WGC;
  la ruta DWM nueva produjo texto ampliado visible con WGC. DXGI continuó negro y
  queda documentado como método no compatible para grabar Zoom en ese entorno.
- Anclaje 2.8: pruebas puras cubren round trip viewport/fuente, origen negativo,
  escalado de grosor y factor inválido seguro. QA real dibuja, panea y cambia el nivel,
  comprobando que el mismo punto proyectado se mueva con la fuente.
- Herramientas 2.8: la prueba UI combina Lápiz, Rectángulo, Texto transparente,
  Captura de 220 × 140, Deshacer, Rehacer, Limpiar y restaurar dentro del documento
  editable; también confirma un texto pendiente al volver a Mano y después repite la
  batería completa de congelación clásica.
- Rendimiento 2.8: 5.000 trazos fuente construyen una caché GPU dispersa de bloques.
  El benchmark mide por separado población, primer cuadro y 240 transformaciones de
  paneo sin volver a rasterizar el historial.
- Transición 2.8.1: pruebas puras fijan los extremos, la monotonía y el ease-out de
  la curva. QA real comprueba que `F` comienza por debajo del factor configurado,
  aterriza exactamente en él a los 180 ms y libera el estado transitorio.
- Cursor 2.8.2: la inspección nativa exige un bitmap de color de al menos 32 px y
  sitúa el hotspot de grafito en el cuadrante superior izquierdo. El cuerpo se
  prolonga hacia abajo y a la derecha para mantener libre el texto subrayado; la
  misma fábrica de cursor cubre overlays normales y tinta de Zoom.
- Lente 2.9: QA exige ancho y alto idénticos, centro incluido, esquinas excluidas por
  la región elíptica nativa y `ZoomLensFrame` visible por encima de la raíz sin
  bloquear entrada. `Shift+rueda` aumenta el diámetro, lo conserva en preferencias
  y puede devolverlo al tamaño anterior; la prueba portable valida el valor inicial
  de 520 px y el round trip de un valor de 760 px.
- Aislamiento QA 2.9: cada smoke UI usa un mutex propio y busca ventanas por PID. La
  limpieza final solo cierra la instancia de prueba, incluso si la copia portable del
  usuario permanece abierta con las mismas clases Win32.
- Figuras 2.10: QA abre el panel geométrico de 172 px, comprueba sus dos filas,
  captura el diseño real y selecciona Pentágono y Hexágono por sus zonas de clic.
  Luego dibuja ambos desde el panel completo sobre el escritorio; la misma ruta de
  documento y renderizado queda disponible en pizarra y Zoom congelado/editable.
- Atajos 2.10: el esquema conserva las 40 acciones y valores de fábrica anteriores,
  añade Pentágono y Hexágono como acciones personalizables y las entrega `Sin
  asignar`; ninguna combinación existente cambia.
- Iconos 2.10.1: Flecha debe mostrar sus dos alas simétricas en lados opuestos del
  eje; Flecha curva usa una Bézier de arco amplio y orienta el cabezal por la tangente
  final. La escala óptica mantiene cada ala por debajo de 7 px sin modificar las
  flechas dibujadas en las superficies reales.
- Orden Z 2.10.1: la barra de Zoom editable no puede adelantarse a la paleta durante
  su refresco de 16 ms; el smoke aislado e instalado verifica la secuencia Zoom,
  barra y paleta por PID y conserva diagnóstico completo ante cualquier fallo.
- Persistencia 2.0: una prueba portable aislada escribe y vuelve a leer posición con
  coordenadas negativas, escala, modo contraído, color, grosor, zoom y atajos
  personalizados; también comprueba el reemplazo atómico sin archivo `.tmp` residual.
- QA visual mediante capturas reales de Paleta, Herramientas, Colores y Configuración;
  se comprueban jerarquía, alineación, separación, legibilidad y estados activos.
- Captura: el gesto y el codificador PNG se prueban con una superficie determinista;
  la copia real del escritorio dispone de ruta GDI y respaldo DXGI Desktop Duplication.
- Empaquetado: inicio portable, integridad SHA-256 e instalacion/desinstalacion
  silenciosa en una carpeta aislada.

## Criterios de rendimiento

Las pruebas de estres agregan 5.000 trazos, ejecutan 250 borrados sobre 5.000 objetos,
limpian/restauran 5.000 elementos y simplifican un trazo de 100.000 muestras. Los
tiempos exactos se registran en cada compilacion Release; cualquier salida distinta
de cero invalida el paquete.

Resultado de referencia de 2.10.1 (2026-08-25), conservado como histórico:

| Prueba | Resultado | Presupuesto |
|---|---:|---:|
| Agregar 5.000 trazos | 21,20 ms | 250 ms |
| 250 borrados fallidos sobre 5.000 objetos | 39,63 ms | 400 ms |
| 10.000 ciclos de deshacer/rehacer el último objeto | 0,56 ms | 200 ms |
| Limpiar y restaurar 5.000 objetos | 1,44 ms | 100 ms |
| Simplificar 100.000 muestras | 57,37 ms | 500 ms |

Renderizado real con 5.000 trazos: 6,577 ms de media para cuadros en caché,
5,355 ms durante dibujo activo y 108,55 MiB de memoria de trabajo. Zoom editable
puebla el documento fuente en 127,655 ms, entra por primera vez en Lápiz en 461,073 ms,
recorre 240 cuadros de Mano a 0,449 ms de media y vuelve a Lápiz con caché caliente en
36,650 ms. Todos permanecen dentro de sus presupuestos respectivos.

## Compatibilidad y recuperacion

- GPU Direct3D 11 con WARP por software como respaldo.
- DPI por monitor V2, escritorio virtual y coordenadas negativas.
- Limite de 8.192 muestras vivas por gesto y simplificacion posterior.
- Historial acotado, instancia unica, `Esc` de emergencia y bandeja de sistema.
- Preferencias separadas para edicion portable e instalada, incluidos atajos y
  estado de hibernación.
- El zoom vivo permanece en Magnifier nativo sin OBS; al detectar OBS puede usar una
  miniatura DWM viva, también acelerada y sin bucle de capturas de CPU. La copia de
  imagen se realiza una sola vez al congelar y la tinta se compone por GPU.
- En sesiones automatizadas cuyo DC de pantalla omite las superficies
  DirectComposition, la regresión de transparencia valida dimensiones y orden de
  ventana y declara el muestreo de píxeles no disponible; en un escritorio
  interactivo conserva además las muestras alfa por píxel.

## Riesgos residuales conocidos

- Windows puede denegar la captura del escritorio en una sesion bloqueada, segura o
  no interactiva; Elite Pen informa el fallo y no genera un archivo corrupto.
- La presion depende del controlador del lapiz y de que Windows entregue WM_POINTER.
- El binario 2.10.3 no esta firmado digitalmente; los hashes del paquete permiten
  verificar integridad hasta incorporar el certificado de Power Elite Studio.
