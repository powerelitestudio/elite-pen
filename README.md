# Elite Pen

![Marca de Elite Pen](assets/elite-pen-mark.svg)

[![Licencia: Apache 2.0](https://img.shields.io/badge/licencia-Apache%202.0-7C5CFC.svg)](LICENSE)
[![Windows CI](https://github.com/powerelitestudio/elite-pen/actions/workflows/ci.yml/badge.svg)](https://github.com/powerelitestudio/elite-pen/actions/workflows/ci.yml)

Elite Pen es una herramienta nativa de anotación, pizarra, captura y ampliación de
pantalla para Windows, desarrollada por [Power Elite Studio](https://powerelite.studio/).
Permite explicar sobre cualquier aplicación sin convertir la presentación en una
sucesión de ventanas y menús.

> **Estado:** acceso anticipado. La versión 2.16.1 corrige los toques de lápiz
> en modo de compatibilidad y conserva la pizarra editable
> Whiteboard / Blackboard con lienzo navegable, presión del lápiz, imágenes,
> selección y recuperación local. Conserva Zoom de enfoque, emojis y macropad.

Ofrece dos presentaciones del mismo motor: `Paleta`, inspirada en una paleta de
pintor, y `Lineal`, una barra vertical ultracompacta. `Paleta` es el modo
predeterminado. Ambas usan el sistema visual Elite en marfil o grafito y conservan
las preferencias tanto instaladas como portables.

El motor conserva la tinta terminada en una superficie GPU y solo rasteriza el
objeto recién añadido. El historial mueve los trazos sin duplicarlos, el borrador
usa distancias al cuadrado y el simplificador de trazos es iterativo. El zoom conserva
la geometría cuando el encuadre no cambia. El benchmark automatizado
valida 5.000 trazos, cuadros en caché, dibujo activo, paneo del Zoom editable,
memoria y rutas de compatibilidad.

## Pizarra editable — Whiteboard / Blackboard

Abre **Herramientas > Pizarra editable**. Es una ventana independiente que puedes
mover a otro monitor, maximizar o minimizar. Tus anotaciones del escritorio y los
fondos rápidos blanco/negro se conservan. **Blackboard / Whiteboard** cambia el fondo
del documento; **Opciones** activa cuadrícula y presión y amplía el panel del 80 %
al 300 %. El panel permanece disponible y recuerda su escala; **Tab** lo oculta o
recupera. Si ocupa más de media ventana, desplaza el panel con la rueda.

- Dibuja con lápiz o resaltador, figuras, símbolos, texto y emojis. El lápiz de
  Windows Ink conserva la presión en cada punto; el ratón usa grosor constante.
- **Mano**, botón central o **Espacio + arrastrar** desplazan el lienzo. La rueda
  acerca/aleja alrededor del cursor entre 3,125 % y 3.200 %. **Encuadrar** muestra
  todo el contenido. El lienzo no tiene un tamaño fijo de página.
- **Selección** permite clic, área y selección múltiple con Shift. Arrastra para
  mover o usa su esquina inferior derecha para escalar. Clic derecho permite
  duplicar, traer al frente, enviar al fondo, bloquear, desbloquear o eliminar.
- **Captura** permite elegir una región del escritorio e insertarla como imagen.
  **Archivo** también permite insertar PNG/JPEG/BMP/GIF y pegar una imagen con Ctrl+V.
  Puedes dibujar encima y mover imagen y tinta con una selección conjunta.
- **Ctrl+S** guarda un `.epboard` editable con las imágenes incluidas; **Ctrl+O**
  lo abre y **Ctrl+Shift+S** guarda otra copia. **Archivo > Exportar vista como PNG**
  exporta el encuadre actual sin herramientas ni marcas de selección.
- La recuperación automática guarda los cambios tras una breve pausa y crea
  puntos de recuperación periódicos durante el dibujo continuo. Cerrar la
  pizarra vuelve al escritorio; minimizar conserva la sesión. Al abrirla de nuevo,
  incluso tras reiniciar Elite Pen, se recupera el último documento y su vista.
  No se muestra automáticamente al iniciar la aplicación.

**Ctrl+Z / Ctrl+Y** deshacen y rehacen; **Ctrl+A** selecciona todo, **Ctrl+D** duplica,
**Supr** elimina, **F11** alterna pantalla completa y **Esc** cancela, deselecciona
o vuelve al escritorio. El texto se confirma con **Ctrl+Enter**.

En **Configuración > Atajos**, asigna **Pizarra editable** a una tecla del macropad.
Llega sin asignar para respetar tus combinaciones. Las acciones globales de lápiz,
figuras, colores, deshacer y favoritos actúan sobre la pizarra mientras está abierta.
Los ajustes del escritorio se mantienen independientes.

Los archivos de recuperación se guardan en
`%LOCALAPPDATA%\Power Elite Studio\Elite Pen\boards` o `data\boards` en portable.
**Archivo > Recuperaciones locales** abre esa carpeta. Antes de Nueva/Abrir se
archiva la sesión anterior; cada guardado mantiene una copia `.previous`. Un error
de escritura no reemplaza el archivo original y se muestra en la interfaz.
Las recuperaciones contienen tu trabajo: puedes administrarlas desde esa carpeta.

Para la Deco 01 V3, activa **Windows Ink** en el controlador XPPen y asigna el área
de trabajo al monitor donde dibujas, conservando proporciones. Consulta el
[manual oficial de XPPen](https://www.xp-pen.com/user-manual/deco-01-v3.html).
Esta primera versión usa capturas estáticas y su formato propio `.epboard`;
no importa archivos `.wboard` ni integra ventanas de aplicaciones en vivo.
El [estudio de la referencia SQLBI](docs/WHITEBOARD_REFERENCE_STUDY.md) explica
qué se examinó y cómo se trasladó a una implementación propia.

## Zoom de enfoque

Pulsa **Ctrl+Shift+M** o abre **Herramientas > Zoom de enfoque**. La primera vez,
arrastra un rectángulo sobre el monitor que contiene el cursor; un clic crea un
acercamiento rápido de 2×. La imagen continúa viva, con el encuadre quieto aunque
muevas el ratón. El acercamiento conserva las proporciones y llega hasta 8×.

| Atajo inicial | Acción |
| --- | --- |
| Ctrl+Shift+M | Abrir el último encuadre o salir con transición |
| Ctrl+Alt+M | Volver a la vista completa y elegir otra zona |
| Ctrl+Alt+Shift+M | Arrastrar un área que se quiere ocultar |
| Ctrl+Alt+Shift+F1–F8 | Guardar encuadre y máscaras en la zona 1–8 |
| Ctrl+Alt+F1–F8 | Recuperar la zona 1–8 |
| Ctrl+Shift+PageDown / PageUp | Ir a la zona guardada siguiente / anterior |
| Esc | Cancelar la selección o salir del enfoque |

La barra inferior permanece disponible durante el enfoque. **Aislar** oscurece lo
que queda fuera de la selección; **Ocultar área** añade hasta ocho máscaras opacas
por encuadre. Clic derecho sobre una máscara la quita. **Máscaras** permite mostrar
u ocultar temporalmente todas y **Vista completa** alterna contexto y encuadre.
**Velocidad** elige transiciones entre 120 y 600 ms y permite borrar las máscaras.

Un espacio vacío **Guardar** conserva el encuadre actual. Clic derecho en una zona
guardada permite reemplazarla o vaciarla. Las zonas y sus máscaras sobreviven al
reinicio; cambiar de zona no sobrescribe las demás. Si falta un monitor, el área
se recupera dentro del monitor disponible más cercano.

En **Configuración > Atajos** puedes cambiar todas estas combinaciones y asignar
teclas a Máscaras, Aislar y Vista completa, que llegan sin asignar. Programa las
mismas combinaciones en el macropad. Las personalizaciones anteriores se conservan;
si otra aplicación ocupa un nuevo atajo, Elite Pen avisa para poder reasignarlo.

Este modo está pensado para presentar: sus clics eligen zonas y ocultaciones.
Para interactuar con una aplicación ampliada, usa **Zoom editable**. Las máscaras
son una ocultación visual reversible durante el enfoque; salir devuelve el escritorio.
La compatibilidad de grabación mantiene la ruta DWM existente cuando OBS está abierto.

## Emojis y macropad

1. Abre **Herramientas > Emojis** y elige un emoji. Un clic lo coloca; arrastrar
   define su tamaño. Sus colores son independientes del lápiz.
2. Haz clic derecho sobre cualquier emoji y elige **Guardar en favorito 1–8**.
   Puedes reemplazar o vaciar esos espacios sin perder sus atajos.
3. En **Configuración > Atajos**, asigna una combinación a **Emoji favorito 1–8**
   o a **Emojis** para abrir el panel. Programa esa combinación en tu macropad.
   Cada atajo selecciona el favorito; después lo colocas con clic o arrastre.
4. En la cabecera del panel elige **80 %–300 %**, pulsa **Fijar** y arrastra
   **EMOJIS** al monitor deseado. Recuerda tamaño y posición al volver a abrir.
   **X** lo cierra incluso fijado; **Fijar** desactivado recupera el cierre al elegir.

## Descargar y ejecutar

Los paquetes oficiales listos para usar se publican en
[GitHub Releases](https://github.com/powerelitestudio/elite-pen/releases):

La distribución publicada que se enlaza a continuación es 2.10.3. Los símbolos y
tamaños nuevos, emojis, Zoom de enfoque y pizarra editable están en el código 2.16.0 y pueden compilarse con los scripts del repositorio.

- [Descargar portable 2.10.3](https://github.com/powerelitestudio/elite-pen/releases/download/v2.10.3/Elite.Pen.Portable.2.10.3.zip):
  descomprime la carpeta y ejecuta `Elite Pen.exe`.
- [Descargar instalador 2.10.3](https://github.com/powerelitestudio/elite-pen/releases/download/v2.10.3/Elite.Pen.Setup.2.10.3.exe):
  instala por usuario, sin privilegios de administrador.
- [Hashes SHA-256 de 2.10.3](https://github.com/powerelitestudio/elite-pen/releases/download/v2.10.3/SHA256SUMS.txt).

Windows puede mostrar una advertencia de SmartScreen mientras los binarios no estén
firmados digitalmente. Verifica siempre que la descarga provenga del repositorio
oficial y compara el SHA-256 publicado con cada versión.

## Objetivos del producto

- Dibujar encima de cualquier aplicacion sin interrumpir el flujo de trabajo.
- Usar un cursor de lápiz nativo, preciso y visible en lugar de la cruz genérica;
  su cuerpo se inclina hacia abajo para no cubrir el texto durante el subrayado.
- Ocultar y recuperar anotaciones sin perderlas.
- Crear texto, lineas, rectangulos, elipses, pentagonos, hexagonos, flechas rectas,
  flechas curvas y símbolos de tacha, chulito, advertencia y prohibido.
- Capturar una region en PNG y copiarla al portapapeles, incluido el zoom congelado
  con sus anotaciones visibles.
- Usar pizarras blanca y negra instantaneas.
- Ampliar la pantalla en vista completa, lente circular redimensionable o acoplada y
  seguir el puntero.
- Congelar el zoom, anotar sobre la imagen fija y reanudar sin perder esa tinta.
- Entrar con `E` en Zoom editable, usar normalmente la aplicación ampliada en Mano y
  congelar con Lápiz para anotar sin perder el contexto.
- Presentar zonas fijas en vivo con transiciones, máscaras y atajos del macropad.
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

Al anotar, **`Ctrl+Z` deshace y `Ctrl+Y` rehace** en el escritorio, las pizarras y
el zoom congelado (incluido Lápiz del Zoom editable). En modo cursor, zoom vivo o
Mano, esas combinaciones quedan disponibles para la aplicación que estés usando.
No interfieren con la entrada de texto ni la edición de atajos. Los atajos globales
de la tabla y tus personalizaciones se conservan; una asignación personalizada
explícita de `Ctrl+Z` o `Ctrl+Y` tiene prioridad sobre este comportamiento contextual.

Los seis colores rápidos también son directos: `Ctrl+Shift+1` negro,
`Ctrl+Shift+2` amarillo, `Ctrl+Shift+3` azul, `Ctrl+Shift+4` rojo,
`Ctrl+Shift+5` verde y `Ctrl+Shift+6` morado. `Ctrl+Shift+7` o
`Ctrl+Shift++` abre el selector completo. En las superficies activas de Elite Pen,
`Ctrl+Shift+rueda` recorre los cinco grosores visuales sin abrir ningún panel.

Con Lápiz seleccionado, los modificadores convierten temporalmente el siguiente
arrastre sin cambiar de herramienta: `Shift` crea una línea, `Ctrl` un rectángulo,
`Tab` una elipse, `Ctrl+Shift` una flecha y `Shift+Tab` una flecha curva Bézier.
Al soltar las teclas, el siguiente gesto vuelve a ser dibujo libre.

El panel `Figuras` reúne once opciones visuales: Línea, Rectángulo, Elipse, Flecha,
Flecha curva, Pentágono, Hexágono, Tacha (X), Chulito, Advertencia y Prohibido.
Los símbolos son figuras vectoriales que usan el color y grosor seleccionados.
Un clic coloca un símbolo de 64 píxeles; arrastrar define su tamaño. `Shift`
durante el arrastre conserva ejes iguales en símbolos, polígonos, rectángulos y elipses.
Cada símbolo es un solo objeto: borrarlo, deshacerlo o rehacerlo requiere una acción.
Funcionan en el escritorio, las pizarras, el zoom congelado y el Zoom editable.

Abre `Figuras` desde el botón de la cabecera de `Herramientas`, con `Ctrl+Shift+G`
o desde la barra Lineal. El porcentaje de su cabecera permite elegir 80 %, 100 %,
125 %, 150 %, 200 % o 300 % para todo el panel, incluidos iconos y zonas de toque.
Su tamaño es independiente del de la paleta. `Fijar` lo mantiene abierto al elegir
figuras y usar otras herramientas; arrastra por el título `FIGURAS` para llevarlo
a otra pantalla. Se guardan tamaño, posición y estado fijo. Si lo dejas fijado y
visible, vuelve a aparecer al iniciar Elite Pen. La `X` lo oculta y respeta esa
decisión al reiniciar; el atajo de Figuras permite abrirlo y cerrarlo cuando quieras.
Al desactivar `Fijado`, vuelve a cerrarse después de elegir una figura.

Todas las figuras tienen su propia acción en `Configuración > Atajos`.
Los dos polígonos y los cuatro símbolos se entregan `Sin asignar` para conservar
las combinaciones del usuario. Para usar un macropad, pulsa el lápiz de una acción,
envía la combinación deseada desde el teclado o el dispositivo y programa esa
misma combinación en su botón. Por ejemplo, F13, F14, F15 y F16 para los cuatro
símbolos; se admiten F1–F24 sin modificadores y combinaciones con Ctrl, Alt, Shift o Win.
Los atajos de figuras son globales y activan directamente la herramienta incluso
desde modo cursor; después coloca el símbolo con un clic o un arrastre.
Las asignaciones se guardan entre sesiones y se rechazan combinaciones ocupadas.

Texto se escribe directamente desde el punto seleccionado: la superficie es
transparente por píxel y solo muestra el caret y los caracteres, nunca una caja.
Las pizarras blanca y negra restauran la paleta sobre el lienzo después de cada
trazo o borrado para conservar todos sus comandos disponibles.

En zoom: `F` pantalla completa con una transición de entrada de 180 ms, rápida y
progresiva hacia el punto de invocación; `L` lente circular con aro óptico y una lupa
persistente sincronizada con el centro real del área ampliada. En Lente,
el punto señalado permanece centrado también en los bordes y esquinas; el espacio
que queda fuera de la pantalla se muestra como margen neutro sin mover ni estirar
el contenido.
`Shift+rueda` o `[` / `]` recorre cinco diámetros sin cambiar cuánto se amplía;
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
100 %, 125 %, 150 %, 200 % (Extra grande) o 300 % (Gigante); el
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
