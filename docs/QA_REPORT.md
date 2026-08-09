# Informe de calidad — Elite Pen 1.7.0

Fecha: 2026-08-08
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
| Agregar 5.000 trazos | 18,53 ms | 2.500 ms |
| 250 borrados fallidos sobre 5.000 objetos | 23,99 ms | 1.500 ms |
| Limpiar y restaurar 5.000 objetos | 8,91 ms | 2.500 ms |
| Simplificar 100.000 muestras | 158,14 ms | 1.500 ms |

## Compatibilidad y recuperacion

- GPU Direct3D 11 con WARP por software como respaldo.
- DPI por monitor V2, escritorio virtual y coordenadas negativas.
- Limite de 8.192 muestras vivas por gesto y simplificacion posterior.
- Historial acotado, instancia unica, `Esc` de emergencia y bandeja de sistema.
- Preferencias separadas para edicion portable e instalada.

## Riesgos residuales conocidos

- Windows puede denegar la captura del escritorio en una sesion bloqueada, segura o
  no interactiva; Elite Pen informa el fallo y no genera un archivo corrupto.
- La presion depende del controlador del lapiz y de que Windows entregue WM_POINTER.
- El binario 1.7.0 no esta firmado digitalmente; los hashes del paquete permiten
  verificar integridad hasta incorporar el certificado de Power Elite Studio.
