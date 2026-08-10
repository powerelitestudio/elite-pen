# Elite Pen

Elite Pen es la herramienta nativa de anotacion, pizarra y ampliacion de pantalla de
Power Elite Studio. Su interfaz principal toma la forma de una paleta de pintor y un
pincel funcional; no replica la barra vertical de otras aplicaciones.

La edición 2.1 consolida el lenguaje visual **Obsidian Atelier**: obsidiana profunda,
metal champaña, azul eléctrico controlado, superficies elevadas y tipografía moderna.
La estética es propia de Power Elite Studio y mantiene la interfaz compacta, legible
y rápida tanto en Windows 10 como en Windows 11.

## Objetivos del producto

- Dibujar encima de cualquier aplicacion sin interrumpir el flujo de trabajo.
- Usar un cursor de lápiz nativo, preciso y visible en lugar de la cruz genérica.
- Ocultar y recuperar anotaciones sin perderlas.
- Crear texto, lineas, rectangulos, elipses, flechas rectas y flechas curvas.
- Capturar una region en PNG y copiarla al portapapeles.
- Usar pizarras blanca y negra instantaneas.
- Ampliar la pantalla en vista completa, lente o acoplada y seguir el puntero.
- Congelar el zoom, anotar sobre la imagen fija y reanudar sin perder esa tinta.
- Hibernar toda la interfaz en una paleta mínima que no estorba.
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
```

Los binarios quedan en `build/<configuracion>/`. La distribucion portable se genera
con `scripts/publish-portable.ps1`.

## Controles

| Accion | Control principal | Atajo global |
|---|---|---|
| Alternar dibujar/interactuar | Punta del pincel | `Ctrl+Shift+P` |
| Ocultar/mostrar trazos | Ojo | `Ctrl+Shift+H` |
| Pizarra blanca | Ferula blanca | `Ctrl+Shift+W` |
| Pizarra negra | Clic derecho en la ferula | `Ctrl+Shift+B` |
| Texto | Herramientas, pulsando el mango | — |
| Figuras | Herramientas, pulsando el mango | — |
| Configuracion | Herramientas, pulsando el mango | — |
| Deshacer | Menu/historial | `Ctrl+Shift+Z` |
| Rehacer | Menu/historial | `Ctrl+Shift+Y` |
| Limpiar | Papelera | `Ctrl+Shift+C` |
| Zoom | Herramientas | `Ctrl+Shift+M` |
| Salir de modo/zoom | — | `Esc` |

En zoom: `F` pantalla completa, `L` lente con una lupa persistente y centro de enfoque,
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

Configuración permite escalar toda la unidad al 80 %, 100 %, 125 % o 150 %; el
100 % corresponde al tamaño Estándar original. El icono bajo el ojo contrae la unidad
un 70 % y deja una paleta mínima. Solo el icono central la expande; el resto de la
superficie conserva el cursor de cuatro flechas y permite moverla. La pestaña
`Atajos` presenta todas las acciones globales y contextuales en una lista desplazable.
Cada fila tiene su propio lápiz de edición; `Supr` o `Retroceso` deja la acción sin
asignar y `Restablecer` recupera los valores de fábrica. Todo se conserva tanto
instalado como portable.

La especificacion funcional completa esta en
[`docs/PRODUCT_SPEC.md`](docs/PRODUCT_SPEC.md).
