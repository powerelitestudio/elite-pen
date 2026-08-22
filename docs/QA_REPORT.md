# Informe de calidad — Elite Pen 2.9.0

Fecha: 2026-08-22
Equipo de referencia: Lenovo 80NV, Intel Core i7-6700HQ, 12 GB RAM, Intel HD 530,
GeForce GTX 960M, Windows 10 Pro 22H2 x64, dos monitores con escalado mixto.

## Cobertura automatizada

- Modelo vectorial: geometria, limites, hit testing, simplificacion, historial,
  borrado compuesto, limpiar, deshacer y rehacer.
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

Resultado final en el equipo de referencia:

| Prueba | Resultado | Presupuesto |
|---|---:|---:|
| Agregar 5.000 trazos | 11,11 ms | 250 ms |
| 250 borrados fallidos sobre 5.000 objetos | 16,92 ms | 400 ms |
| 10.000 ciclos de deshacer/rehacer el último objeto | 0,37 ms | 200 ms |
| Limpiar y restaurar 5.000 objetos | 0,49 ms | 100 ms |
| Simplificar 100.000 muestras | 27,39 ms | 500 ms |

Renderizado real con 5.000 trazos: 2,663 ms de media para cuadros en caché,
1,915 ms durante dibujo activo y 111,87 MiB de memoria de trabajo. Zoom editable
puebla el documento fuente en 5,504 ms, entra por primera vez en Lápiz en 172,339 ms,
recorre 240 cuadros de Mano a 0,203 ms de media y vuelve a Lápiz con caché caliente en
22,834 ms. Todos permanecen dentro de sus presupuestos respectivos.

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
- El binario 2.9.0 no esta firmado digitalmente; los hashes del paquete permiten
  verificar integridad hasta incorporar el certificado de Power Elite Studio.
