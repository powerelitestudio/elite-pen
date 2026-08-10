# Elite Pen

Elite Pen es la herramienta nativa de anotacion, pizarra y ampliacion de pantalla de
Power Elite Studio. Ofrece dos presentaciones del mismo motor: `Paleta`, con la
paleta de pintor y su pincel funcional, y `Lineal`, una barra vertical ultracompacta.
`Paleta` es el modo predeterminado.

La edición 2.5 integra el sistema visual de la familia Elite: grafito o marfil según
el tema, violeta como acento principal, menta para estados, divisores finos y
tipografía Segoe UI Variable. Configuración permite elegir `Oscuro` o `Claro`; la
preferencia queda guardada tanto al instalar como en la edición portable.

El motor 2.5 conserva la tinta terminada en una superficie GPU y solo rasteriza el
objeto recién añadido. El historial mueve los trazos sin duplicarlos, el borrador
usa distancias al cuadrado y el simplificador de trazos es iterativo. El zoom evita
reconfigurar su fuente cuando el puntero permanece quieto. El benchmark automatizado
valida 5.000 trazos, cuadros en caché, dibujo activo, memoria y rutas de compatibilidad.

## Objetivos del producto

- Dibujar encima de cualquier aplicacion sin interrumpir el flujo de trabajo.
- Usar un cursor de lápiz nativo, preciso y visible en lugar de la cruz genérica.
- Ocultar y recuperar anotaciones sin perderlas.
- Crear texto, lineas, rectangulos, elipses, flechas rectas y flechas curvas.
- Capturar una region en PNG y copiarla al portapapeles.
- Usar pizarras blanca y negra instantaneas.
- Ampliar la pantalla en vista completa, lente o acoplada y seguir el puntero.
- Congelar el zoom, anotar sobre la imagen fija y reanudar sin perder esa tinta.
- Hibernar toda la interfaz en una unidad mínima que no estorba.
- Usar tinta temporal y un halo de cursor para presentaciones.
- Mantener una respuesta fluida en equipos de 2017 con graficos integrados.
- Funcionar instalado o como aplicacion portable sin privilegios de administrador.

## Plataforma

- Windows 10 22H2 y Windows 11, x64.
- C++20 nativo, Win32, Direct3D 11, DXGI, Direct2D y DirectWrite.
- Renderizado acelerado por GPU con ruta de compatibilidad WARP.
- Binario autocontenido compilado contra UCRT.

## Compilar

El repositorio usa un LLVM-MinGW portable y scripts PowerShell, por lo que no exige
Visual Studio ni modifica la configuracion del equipo.

```powershell
pwsh -File .\scripts\bootstrap-toolchain.ps1
pwsh -File .\scripts\build.ps1 -Configuration Release
pwsh -File .\scripts\test.ps1 -Configuration Release
pwsh -File .\scripts\render-performance-test.ps1 -Configuration Release
```

Los binarios quedan en `build/<configuracion>/`. La distribucion portable se genera
con `scripts/publish-portable.ps1`.

## Controles

Configuración permite elegir `Paleta de pintor` o `Lineal vertical` sin reiniciar.
Lineal reúne en una columna los comandos principales, añade los cinco grosores y
una matriz inferior con los seis colores rápidos y `+`. Ambas presentaciones
comparten estado, paneles, atajos, tema, escala y posición guardada.

| Accion | Control principal | Atajo global |
|---|---|---|
| Alternar dibujar/interactuar | Punta del pincel | `Ctrl+Shift+Q` |
| Ocultar/mostrar trazos | Ojo | `Ctrl+Shift+A` |
| Pizarra blanca | Ferula blanca | `Ctrl+Shift+W` |
| Pizarra negra | Clic derecho en la ferula | `Ctrl+Shift+B` |
| Texto | Herramientas, pulsando el mango | — |
| Figuras | Herramientas, pulsando el mango | — |
| Configuracion | Herramientas, pulsando el mango | — |
| Deshacer | Menu/historial | `Ctrl+Alt+Z` |
| Rehacer | Menu/historial | `Ctrl+Shift+Y` |
| Limpiar | Papelera | `Ctrl+Shift+E` |
| Zoom | Herramientas | `Ctrl+Shift+Z` |
| Contraer/expandir | Control bajo el ojo | `Ctrl+Shift+D` |
| Salir de modo/zoom | — | `Esc` |

Los seis colores rápidos también son directos: `Ctrl+Shift+1` negro,
`Ctrl+Shift+2` amarillo, `Ctrl+Shift+3` azul, `Ctrl+Shift+4` rojo,
`Ctrl+Shift+5` verde y `Ctrl+Shift+6` morado. `Ctrl+Shift+7` o
`Ctrl+Shift++` abre el selector completo. En las superficies activas de Elite Pen,
`Ctrl+Shift+rueda` recorre los cinco grosores visuales sin abrir ningún panel.

Con Lápiz seleccionado, los modificadores convierten temporalmente el siguiente
arrastre sin cambiar de herramienta: `Shift` crea una línea, `Ctrl` un rectángulo,
`Tab` una elipse, `Ctrl+Shift` una flecha y `Ctrl+Alt` una flecha curva Bézier.
Al soltar las teclas, el siguiente gesto vuelve a ser dibujo libre.

Texto se escribe directamente desde el punto seleccionado: la superficie es
transparente por píxel y solo muestra el caret y los caracteres, nunca una caja.
Las pizarras blanca y negra restauran la paleta sobre el lienzo después de cada
trazo o borrado para conservar todos sus comandos disponibles.

En zoom: `F` pantalla completa, `L` lente con una lupa persistente sincronizada con
el centro real del área ampliada,
`D` acoplado, `I` invertir,
rueda/`+`/`-` para ampliar, `0` para vista general, `Espacio` o `M` para recorrer
vistas, y `P` o clic para congelar o reanudar. `Esc`, `F4` o clic derecho salen.
La congelación toma la imagen ampliada que está visible, no una captura del área
original sin zoom, y rechaza una imagen vacía antes de abrir la capa de tinta.
Mientras la imagen está congelada, la paleta y sus paneles permanecen siempre por
encima de la tinta para cambiar color, grosor, texto o geometría y mover la unidad.
Al congelar se activa el lápiz y se admiten colores, texto y geometrías. La tinta del
zoom usa historial propio: permanece al reanudar y la papelera la limpia sin tocar
las anotaciones normales.

Configuración permite elegir Paleta o Lineal y escalar toda la unidad al 80 %,
100 %, 125 % o 150 %; el
100 % corresponde al tamaño Estándar original. El icono bajo el ojo contrae la unidad
un 70 % y deja una paleta mínima. Solo el icono central la expande; el resto de la
superficie conserva el cursor de cuatro flechas y permite moverla. La pestaña
`Atajos` presenta todas las acciones globales y contextuales en una lista desplazable.
Cada fila tiene su propio lápiz de edición; `Supr` o `Retroceso` deja la acción sin
asignar y `Restablecer` recupera los valores de fábrica. Todo se conserva tanto
instalado como portable.

La edición 2.5 conserva el esquema de atajos 2.4 y migra únicamente las combinaciones
que todavía coincidan con los valores de
fábrica anteriores. Las combinaciones personalizadas se mantienen intactas;
`Restablecer` aplica el esquema 2.4 completo.

La especificacion funcional completa esta en
[`docs/PRODUCT_SPEC.md`](docs/PRODUCT_SPEC.md).
