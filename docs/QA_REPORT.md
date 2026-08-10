# Informe de calidad — Elite Pen 2.1.3

Fecha: 2026-08-10
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
| Agregar 5.000 trazos | 23,16 ms | 2.500 ms |
| 250 borrados fallidos sobre 5.000 objetos | 17,23 ms | 1.500 ms |
| Limpiar y restaurar 5.000 objetos | 10,40 ms | 2.500 ms |
| Simplificar 100.000 muestras | 125,52 ms | 1.500 ms |

## Compatibilidad y recuperacion

- GPU Direct3D 11 con WARP por software como respaldo.
- DPI por monitor V2, escritorio virtual y coordenadas negativas.
- Limite de 8.192 muestras vivas por gesto y simplificacion posterior.
- Historial acotado, instancia unica, `Esc` de emergencia y bandeja de sistema.
- Preferencias separadas para edicion portable e instalada, incluidos atajos y
  estado de hibernación.
- El zoom vivo permanece en Magnifier nativo; la copia de imagen se realiza una sola
  vez al congelar y la tinta se compone por GPU, sin bucle de capturas de CPU.
- En sesiones automatizadas cuyo DC de pantalla omite las superficies
  DirectComposition, la regresión de transparencia valida dimensiones y orden de
  ventana y declara el muestreo de píxeles no disponible; en un escritorio
  interactivo conserva además las muestras alfa por píxel.

## Riesgos residuales conocidos

- Windows puede denegar la captura del escritorio en una sesion bloqueada, segura o
  no interactiva; Elite Pen informa el fallo y no genera un archivo corrupto.
- La presion depende del controlador del lapiz y de que Windows entregue WM_POINTER.
- El binario 2.1.3 no esta firmado digitalmente; los hashes del paquete permiten
  verificar integridad hasta incorporar el certificado de Power Elite Studio.
