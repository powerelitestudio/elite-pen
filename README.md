# Elite Pen

![Marca de Elite Pen](assets/elite-pen-mark.svg)

[![Licencia: Apache 2.0](https://img.shields.io/badge/licencia-Apache%202.0-7C5CFC.svg)](LICENSE)
[![Windows CI](https://github.com/powerelitestudio/elite-pen/actions/workflows/ci.yml/badge.svg)](https://github.com/powerelitestudio/elite-pen/actions/workflows/ci.yml)

Elite Pen es una herramienta nativa de anotación, pizarra, captura y ampliación de
pantalla para Windows, desarrollada por [Power Elite Studio](https://powerelite.studio/).
Permite explicar sobre cualquier aplicación sin convertir la presentación en una
sucesión de ventanas y menús.

> **Estado:** acceso anticipado. La versión 2.8.2 es funcional y está validada para
> empezar a compartirse, pero el producto continúa en desarrollo activo. Los reportes
> de errores y casos de compatibilidad son bienvenidos.

Ofrece dos presentaciones del mismo motor: `Paleta`, inspirada en una paleta de
pintor, y `Lineal`, una barra vertical ultracompacta. `Paleta` es el modo
predeterminado. Ambas usan el sistema visual Elite en marfil o grafito y conservan
las preferencias tanto instaladas como portables.

El motor conserva la tinta terminada en una superficie GPU y solo rasteriza el
objeto recién añadido. El historial mueve los trazos sin duplicarlos, el borrador
usa distancias al cuadrado y el simplificador de trazos es iterativo. El zoom evita
reconfigurar su fuente cuando el puntero permanece quieto. El benchmark automatizado
valida 5.000 trazos, cuadros en caché, dibujo activo, paneo del Zoom editable,
memoria y rutas de compatibilidad.

## Descargar y ejecutar

Los paquetes oficiales listos para usar se publican en
[GitHub Releases](https://github.com/powerelitestudio/elite-pen/releases):

- `Elite Pen Portable`: descomprime la carpeta y ejecuta `Elite Pen.exe`.
- `Elite Pen Setup`: instala por usuario, sin privilegios de administrador.

Windows puede mostrar una advertencia de SmartScreen mientras los binarios no estén
firmados digitalmente. Verifica siempre que la descarga provenga del repositorio
oficial y compara el SHA-256 publicado con cada versión.

## Objetivos del producto

- Dibujar encima de cualquier aplicacion sin interrumpir el flujo de trabajo.
- Usar un cursor de lápiz nativo, preciso y visible en lugar de la cruz genérica;
  su cuerpo se inclina hacia abajo para no cubrir el texto durante el subrayado.
- Ocultar y recuperar anotaciones sin perderlas.
- Crear texto, lineas, rectangulos, elipses, flechas rectas y flechas curvas.
- Capturar una region en PNG y copiarla al portapapeles, incluido el zoom congelado
  con sus anotaciones visibles.
- Usar pizarras blanca y negra instantaneas.
- Ampliar la pantalla en vista completa, lente o acoplada y seguir el puntero.
- Congelar el zoom, anotar sobre la imagen fija y reanudar sin perder esa tinta.
- Entrar con `E` en Zoom editable, usar normalmente la aplicación ampliada en Mano y
  congelar con Lápiz para anotar sin perder el contexto.
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

Requisitos: Windows 10 u 11 x64, PowerShell 7 y acceso a GitHub para descargar la
cadena de compilación verificada por SHA-256.

```powershell
pwsh -File .\scripts\bootstrap-toolchain.ps1
pwsh -File .\scripts\build.ps1 -Configuration Release
pwsh -File .\scripts\test.ps1 -Configuration Release
pwsh -File .\scripts\render-performance-test.ps1 -Configuration Release
```

Los binarios quedan en `build/<configuracion>/`. La distribucion portable se genera
con `scripts/publish-portable.ps1`. Los tres activos listos para GitHub Releases —ZIP
portable, instalador y hashes— se generan desde la misma versión con:

```powershell
pwsh -File .\scripts\build-release-assets.ps1
```

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
| Zoom editable | Barra flotante de zoom | `E` dentro del zoom |
| Contraer/expandir | Control bajo el ojo | `Ctrl+Shift+D` |
| Salir de modo/zoom | — | `Esc` |

Los seis colores rápidos también son directos: `Ctrl+Shift+1` negro,
`Ctrl+Shift+2` amarillo, `Ctrl+Shift+3` azul, `Ctrl+Shift+4` rojo,
`Ctrl+Shift+5` verde y `Ctrl+Shift+6` morado. `Ctrl+Shift+7` o
`Ctrl+Shift++` abre el selector completo. En las superficies activas de Elite Pen,
`Ctrl+Shift+rueda` recorre los cinco grosores visuales sin abrir ningún panel.

Con Lápiz seleccionado, los modificadores convierten temporalmente el siguiente
arrastre sin cambiar de herramienta: `Shift` crea una línea, `Ctrl` un rectángulo,
`Tab` una elipse, `Ctrl+Shift` una flecha y `Shift+Tab` una flecha curva Bézier.
Al soltar las teclas, el siguiente gesto vuelve a ser dibujo libre.

Texto se escribe directamente desde el punto seleccionado: la superficie es
transparente por píxel y solo muestra el caret y los caracteres, nunca una caja.
Las pizarras blanca y negra restauran la paleta sobre el lienzo después de cada
trazo o borrado para conservar todos sus comandos disponibles.

En zoom: `F` pantalla completa con una transición de entrada de 180 ms, rápida y
progresiva hacia el punto de invocación; `L` lente con una lupa persistente
sincronizada con el centro real del área ampliada,
`D` acoplado, `I` invertir,
rueda/`+`/`-` para ampliar, `0` para vista general, `Espacio` o `M` para recorrer
vistas, y `P` o clic para congelar o reanudar. `Esc`, `F4` o clic derecho salen.
Para grabar el zoom con OBS en Windows 10, configura la fuente `Captura de pantalla`
con el método `Windows 10 (1903 y posteriores)`. Elite Pen detecta OBS y presenta la
aplicación enfocada mediante una composición DWM grabable; fuera de OBS conserva el
motor Magnifier nativo. `DXGI Desktop Duplication` no registra la composición del
zoom y puede producir un cuadro negro en equipos con gráficos híbridos.
La congelación toma la imagen ampliada que está visible, no una captura del área
original sin zoom, y rechaza una imagen vacía antes de abrir la capa de tinta.
Mientras la imagen está congelada, la paleta y sus paneles permanecen siempre por
encima de la tinta para cambiar color, grosor, texto o geometría y mover la unidad.
Al congelar se activa el lápiz y se admiten colores, texto y geometrías. La tinta del
zoom usa historial propio: permanece al reanudar y la papelera la limpia sin tocar
las anotaciones normales. Captura funciona también sobre esta imagen fija: aplana la
ampliación y la tinta visible en un PNG, copia el mismo resultado al portapapeles y
respeta la preferencia que incluye o excluye la interfaz de Elite Pen.

`E` activa adicionalmente `Zoom editable` sin sustituir el flujo anterior. La barra
flotante permite cambiar entre `MANO`, que entrega clics, rueda y teclado a la
aplicación real mientras conserva la ampliación, y `LÁPIZ`, que congela el cuadro
ampliado exacto para usar lápiz, colores, grosores, texto, figuras, borrador, captura e
historial. `Espacio` vuelve de forma segura a `MANO`. La tinta queda guardada en
coordenadas de la fuente y reaparece al entrar otra vez en `LÁPIZ`; durante `MANO` la
superficie de anotación permanece oculta para no cubrir ni bloquear la aplicación.
Como las teclas simples pertenecen a la aplicación en `MANO`, la ampliación se ajusta
con `+` y `−` de la barra y se sale con su cierre o con `Ctrl+Shift+Z`. `P` conserva la
congelación clásica fuera de este flujo.

Configuración permite elegir Paleta o Lineal y escalar toda la unidad al 80 %,
100 %, 125 % o 150 %; el
100 % corresponde al tamaño Estándar original. El icono bajo el ojo contrae la unidad
un 70 % y deja una paleta mínima. Solo el icono central la expande; el resto de la
superficie conserva el cursor de cuatro flechas y permite moverla. La pestaña
`Atajos` presenta todas las acciones globales y contextuales en una lista desplazable.
Cada fila tiene su propio lápiz de edición; `Supr` o `Retroceso` deja la acción sin
asignar y `Restablecer` recupera los valores de fábrica. Todo se conserva tanto
instalado como portable.

La edición 2.7 conserva el esquema de atajos 2.4 y migra únicamente las combinaciones
que todavía coincidan con los valores de
fábrica anteriores. Las combinaciones personalizadas se mantienen intactas;
`Restablecer` aplica el esquema 2.4 completo.

La especificacion funcional completa esta en
[`docs/PRODUCT_SPEC.md`](docs/PRODUCT_SPEC.md).

## Privacidad y seguridad

- No requiere cuenta, inicio de sesión ni servicio en segundo plano.
- No incorpora telemetría ni envía anotaciones o capturas a Power Elite Studio.
- Las preferencias se guardan en el equipo; los dibujos no se recuperan al cerrar.
- Captura y zoom procesan la imagen localmente. El botón del sitio oficial solo abre
  el navegador predeterminado.

No publiques capturas, volcados o diagnósticos con información privada. Para una
vulnerabilidad, sigue [SECURITY.md](SECURITY.md); para problemas funcionales, utiliza
las plantillas de GitHub.

## Colaborar

Consulta [CONTRIBUTING.md](CONTRIBUTING.md) antes de abrir una incidencia o proponer
un cambio y participa según [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). La lista de
pasos que debe completar la primera publicación está en
[docs/PUBLIC_RELEASE_CHECKLIST.md](docs/PUBLIC_RELEASE_CHECKLIST.md).

## Licencia

Elite Pen es software de código abierto bajo la
[Apache License 2.0](LICENSE). Puedes usar, estudiar, modificar y redistribuir el
código o los binarios, incluso con fines comerciales, sujeto a los términos de esa
licencia. Conserva también las atribuciones de [NOTICE](NOTICE).

La licencia del software no concede derechos sobre las marcas `Elite Pen`,
`Power Elite Studio`, sus logotipos ni su identidad visual. Consulta la
[política de marcas](TRADEMARKS.md) para distinguir forks y distribuciones derivadas
de las ediciones oficiales.
