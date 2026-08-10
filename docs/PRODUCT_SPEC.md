# Elite Pen 2.5 — contrato de producto

Estado: version 2.5.0 implementada.

## 1. Identidad e interaccion principal

La ventana de control ofrece dos presentaciones intercambiables: `Paleta`, una
paleta de pintor compacta, y `Lineal`, una barra vertical estrecha. Ambas son
siempre visibles, movibles y escaladas por monitor. El fondo exterior es
transparente y el control no roba foco a la aplicacion sobre la que se presenta.
Paleta es el valor predeterminado; la selección se conserva entre sesiones.

### Lenguaje visual

- El sistema visual compartido con Elite Slides usa fondos grafito o marfil,
  superficies frías, violeta para selección y jerarquía, y menta para estados.
- Configuración ofrece temas `Oscuro` y `Claro`; el cambio es inmediato, abarca
  paleta, paneles, configuración, lupa e indicadores y se conserva entre sesiones.
- El volumen se comunica con gradientes breves, brillos controlados y sombras
  contenidas; no se emplean biseles, marcos blancos ni cromados clásicos.
- Las selecciones siempre disponen de forma, contorno o luminancia además del color.
- Herramientas, Colores y Configuración comparten radios, elevación, tipografía y
  comportamiento de foco/hover.
- Segoe UI Variable es la familia preferida en Windows 11; Windows 10 usa la
  sustitución nativa Segoe UI sin descargar fuentes ni perder rendimiento.

### Paleta

- Seis colores rapidos en este orden: negro, amarillo, azul, rojo, verde y morado.
- El septimo acceso es un `+` sin circulo exterior; abre una cuadricula de 42 colores
  y el selector RGB personalizado de Windows.
- El ojo central abierto muestra las anotaciones; al ocultarlas se transforma en un
  parpado cerrado con pestanas sutiles, sin alterar el historial.
- Un control menor bajo el ojo contrae la unidad completa. Desaparecen colores,
  grosores, ojo, pincel y papelera; queda únicamente una paleta oscura al 30 % del
  tamaño seleccionado con un icono de expansión. Solo ese icono expande la unidad;
  la superficie restante muestra el cursor de movimiento y permite reubicarla sin
  salir del modo compacto. Su estado se conserva entre sesiones.
- Los puntos de grosor seleccionan 2, 4, 7, 12 y 20 pixeles logicos; 4, el segundo
  punto de arriba hacia abajo, es el valor inicial y se configura desde Ajustes.
  `Ctrl+Shift+rueda` recorre esos mismos cinco puntos cuando el puntero está sobre
  una superficie activa de Elite Pen.
- La paleta se arrastra desde cualquier zona vacia y recuerda su posicion por monitor.
- El arrastre usa coordenadas absolutas derivadas del puntero y no realimenta la
  posición ya desplazada de la ventana; debe seguir cada delta sin rebote ni lastre.
- El componente principal mide 174 x 168 pixeles fisicos a escala 100 %, una
  reduccion uniforme del 40 % respecto del lienzo visual anterior de 290 x 280.
- Configuración ofrece tamaños Compacta 80 %, Estándar 100 %, Grande 125 % y Muy
  grande 150 %. El tamaño Estándar conserva exactamente los 174 x 168 píxeles.
- El escalado se aplica a la unidad completa: forma, puntos, colores, ojo, pincel,
  papelera, ayudas y zonas interactivas; conserva el centro visual y se limita al
  área útil del monitor.
- Todos los comandos permanecen por encima del lienzo y muestran cursor de mano, por
  lo que color, grosor y herramienta se seleccionan sin abandonar el modo de dibujo.

### Lineal

- Organiza en una columna visibilidad, Cursor/Lápiz, menú completo, resaltador,
  borrador, figuras, texto, pizarra, zoom, Deshacer, Rehacer, Limpiar y Configuración.
- Conserva acceso directo a los cinco grosores y a negro, amarillo, azul, rojo,
  verde, morado y el selector `+` en el pie de la barra.
- A escala Estándar mide 46 × 406 píxeles físicos. Los otros tres tamaños aplican
  el mismo factor uniforme usado por Paleta a dibujo, iconos y zonas de clic.
- El estado activo se comunica mediante superficie violeta, contorno y cambio del
  propio glifo. El hover solo invalida la barra cuando cambia de elemento.
- Al contraerse queda una píldora Elite de 46 × 49 píxeles a escala Estándar; el
  icono central expande y el espacio restante permite moverla.
- Clic en Pizarra abre el lienzo blanco y clic derecho abre el negro. El selector de
  herramientas y los paneles se anclan al costado disponible de la misma ventana.
- Paleta y Lineal son presentaciones del mismo controlador: no duplican documento,
  historial, zoom, preferencias, hotkeys ni superficies de renderizado.

### Pincel

- Punta del pincel: alterna entre Lapiz e Interactuar (cursor normal).
- Mango violeta limpio, unido y alineado con la ferula blanca, y un 10 % adicional mas
  corto que en la version 1.9.
- Cualquier punto del mango abre un unico panel con todas las herramientas y
  Configuracion; no contiene iconos ni comandos diminutos sobre su superficie.
- Ferula blanca: clic alterna la pizarra blanca; clic derecho alterna la negra.
- Papelera: limpia el documento mediante una operacion reversible; se integra cerca
  del extremo del mango, pero mantiene una zona de clic independiente.
- No se dibujan etiquetas, nombres, muestras de color ni indicadores de grosor debajo
  del mango.

## 2. Herramientas

1. Interactuar: la superposicion deja pasar raton, lapiz y tacto.
2. Lapiz: trazo suavizado con raton o lapiz y presion cuando el hardware la informa.
   Usa un cursor nativo de lápiz inclinado en lugar de la cruz de Windows; la punta
   de grafito coincide con el punto exacto donde comienza el trazo y escala por DPI.
3. Resaltador: trazo semitransparente y ancho ampliado; comparte el cursor de lápiz.
4. Borrador: elimina objetos tocados; la operacion se puede deshacer.
5. Linea: linea recta con vista previa.
6. Rectangulo: contorno perfecto; `Shift` conserva proporcion cuadrada.
7. Elipse: contorno perfecto; `Shift` conserva proporcion circular.
8. Flecha: linea recta con punta proporcional al grosor.
9. Flecha curva: curva cubica Bezier calculada entre inicio y fin, con arco uniforme,
   limites y hit testing sobre la trayectoria real, y punta tangencial.
10. Texto: al elegir `T`, el siguiente clic fija el punto de insercion y abre un editor
    transparente directamente sobre el escritorio. Admite multilinea, pegado,
    `Ctrl+Enter` para confirmar y `Esc` para cancelar, sin dialogo independiente ni
    superficie opaca: DirectComposition muestra únicamente caracteres y caret.
11. Captura: seleccion rectangular, guardado PNG y copia al portapapeles.
12. Zoom: ampliacion en vivo centrada en el puntero, ajuste con rueda, congelación o
    reanudación con `P` y salida con `Esc`.

Con Lapiz activo, un modificador aplicado al comenzar el arrastre selecciona una
figura temporal sin cambiar la herramienta persistente: `Shift` Linea, `Ctrl`
Rectangulo, `Tab` Elipse, `Ctrl+Shift` Flecha y `Ctrl+Alt` Flecha curva Bezier.
Esta interpretación también funciona sobre el zoom congelado y nunca sustituye
los gestos propios de Texto, Borrador ni una figura elegida explícitamente.

El panel abierto desde el mango reune todas las herramientas en un solo lugar y
añade una fila independiente para Configuracion.

## 3. Documento e historial

- Cada monitor comparte un solo documento en coordenadas del escritorio virtual.
- Deshacer y rehacer conservan inserciones, borrados y limpiezas.
- Limpiar nunca destruye el historial inmediatamente.
- El cambio de visibilidad, color, grosor o herramienta no altera dibujos existentes.
- El historial se limita por cantidad y descarta primero lo más antiguo en tiempo
  constante. Los trazos se mueven entre documento e historial, sin duplicar sus
  vectores de puntos.
- Al cerrar, las preferencias se guardan de forma atomica. Las anotaciones no se
  persisten por defecto para evitar recuperar informacion sensible inesperadamente.

### Contrato de rendimiento

- La tinta terminada se rasteriza en una superficie Direct2D retenida por monitor;
  el repintado normal compone un bitmap, no vuelve a recorrer todos los trazos.
- Añadir un trazo actualiza únicamente ese objeto. Borrar, limpiar, deshacer o
  rehacer reconstruyen la superficie solo cuando la operación elimina o reordena
  píxeles existentes.
- La entrada nunca espera deliberadamente al compositor. Un cuadro ocupado se
  reintenta mediante el ciclo de mensajes conservando la interfaz receptiva.
- El benchmark de lanzamiento exige, con 5.000 trazos en el equipo de referencia,
  menos de 8 ms de media por cuadro en caché, menos de 12 ms mientras se dibuja y
  menos de 350 MiB de memoria de trabajo.
- El zoom no repite operaciones nativas si puntero, vista, aumento y modo general no
  han cambiado. Las vistas con geometría fija solo actualizan la región fuente.

## 4. Pizarra y zoom

- Las pizarras blanca y negra cubren el escritorio virtual, conservan las anotaciones
  y se activan sin recrear el documento. Al entrar en pizarra negra con color negro,
  Elite Pen cambia a amarillo para conservar contraste. Tras cada trazo o borrado,
  la paleta se recompone sobre el lienzo y continúa visible y seleccionable.
- Zoom usa la capacidad nativa de Windows en proceso x64. La ventana de control se
  excluye de la captura cuando el sistema lo permite para evitar recursion visual.
- El zoom vivo presenta directamente la salida de Magnifier, sin interponer la capa
  de tinta transparente antes de congelar. Esto evita superficies negras o sin
  inicializar al entrar con `Ctrl+Shift+Z`.
- La congelación copia la salida ampliada ya compuesta en pantalla, retirando durante
  ese instante la afinidad que la excluye de capturas. Una imagen completamente vacía
  se rechaza y el zoom continúa vivo en vez de mostrar una pizarra negra falsa.
- `P` o clic congela la salida ampliada en una superficie GPU independiente y activa Lapiz.
  Sobre ella funcionan Lapiz, Resaltador, Borrador, Texto, Linea, Rectangulo, Elipse,
  Flecha y Flecha curva. `P` reanuda el zoom vivo sin perder esas anotaciones.
- El zoom mantiene documento e historial propios durante su sesión. Papelera,
  Deshacer y Rehacer actúan exclusivamente sobre ese documento mientras el zoom
  está abierto; salir del zoom descarta la sesión sin tocar el documento normal.
- Factores admitidos: 1.25x a 8x; valor inicial 2x. Las vistas son pantalla completa,
  lente y acoplada; admiten inversion de color y vista general 1x.
- La vista Lente usa un cursor propio con aro de lupa y retícula central para mostrar
  con precisión qué punto del escritorio alimenta el centro de la ampliación. La guía
  es una superficie transparente persistente que sigue al puntero, no recibe entrada
  y se excluye del contenido magnificado para evitar recursión.
- Mientras el zoom está vivo, un capturador de clic temporal y limitado a esa sesión
  congela al completar el clic izquierdo en cualquier monitor. Los clics dentro de la
  paleta se excluyen para permitir preparar color o herramienta sin congelar.
- En estado congelado, el orden Z contractual es Paleta/paneles sobre ZoomInk y
  ZoomInk sobre la salida nativa. Dibujar o activar la tinta no puede invertirlo.
- Con varios monitores, el zoom se limita al monitor que contiene el puntero y migra
  al cruzar de monitor.
- `Esc` sale del zoom y también abandona las pizarras blanca o negra, conservando las
  anotaciones vectoriales.

## 5. Rendimiento y compatibilidad

- Objetivo de entrada a presentacion: menos de 16.7 ms en el equipo de referencia.
- El dibujo continuo invalida solo mientras existe gesto o animacion.
- Geometria vectorial, sin capturas permanentes de pantalla para las anotaciones.
- Limite adaptable de puntos por trazo y simplificacion sin saltos visibles.
- Direct3D 11 con `D3D11_CREATE_DEVICE_BGRA_SUPPORT`; si falla la GPU se intenta WARP.
- DPI por monitor V2 y coordenadas negativas del escritorio virtual.
- Ninguna funcion esencial requiere instalador, servicio, driver o elevacion.

## 6. Accesibilidad y seguridad operativa

- Tooltips y nombres accesibles para todos los controles, no solo informacion por
  color.
- Contraste visible para seleccion, foco y modo oculto.
- Confirmacion configurable para limpiar; el valor inicial permite deshacer sin modal.
- Tinta temporal configurable en 3, 8 o 15 segundos sin contaminar el historial.
- Resaltado de cursor configurable para presentaciones.
- Atajo de emergencia `Esc` devuelve interaccion al escritorio.
- Icono de bandeja con acciones equivalentes y salida explicita.
- Configuración separa `General` y `Atajos`; la segunda pestaña permite capturar una
  combinación nueva para cada acción global o contextual mediante un lápiz explícito,
  detecta duplicados dentro de su ámbito o reservas de Windows, admite dejar acciones
  sin asignar con `Supr`/`Retroceso`, restablece los valores de fábrica y explica
  `P`/clic, rueda, `+`/`-`, `F`/`L`/`D`, `I`, `0`, `Espacio`/`M`, `Esc`, `F4` y clic derecho.
- El esquema global 2.4 usa `Ctrl+Shift+Q` para Cursor/Lapiz,
  `Ctrl+Shift+A` para visibilidad, `Ctrl+Shift+Z` para Zoom, `Ctrl+Shift+E` para
  Limpiar y `Ctrl+Shift+D` para contraer/expandir. Deshacer migra a `Ctrl+Alt+Z`.
  Los colores rápidos usan `Ctrl+Shift+1..6`; `Ctrl+Shift+7` y `Ctrl+Shift++`
  abren Colores. La migración reemplaza solo valores de fábrica antiguos y conserva
  cualquier combinación personalizada.
- Los atajos, la escala y el estado contraído se guardan atómicamente en LocalAppData
  para la instalación o en `data/settings.ini` junto al ejecutable portable.

## 7. Criterios de aceptacion 1.0

- Todas las herramientas crean el resultado esperado en DPI 100 %, 150 % y 200 %.
- La entrada no queda capturada al cambiar a Interactuar o pulsar `Esc`.
- Ocultar/mostrar, pizarra y zoom no pierden el historial.
- Congelar/reanudar no altera la imagen fija ni el documento del escritorio; limpiar
  y deshacer seleccionan el documento correcto según el contexto.
- Limpiar, deshacer y rehacer funcionan con documento vacio y con miles de objetos.
- Reinicio de Explorer, desconexion de monitor y cambio de resolucion no cierran la
  aplicacion ni dejan una ventana inaccesible.
- No hay bloqueos de interfaz durante trazos largos ni crecimiento de memoria sin
  limite.
- El paquete portable inicia en Windows 10 22H2 y Windows 11 sin runtimes externos.
