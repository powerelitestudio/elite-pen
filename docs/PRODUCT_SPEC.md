# Elite Pen 1.8 — contrato de producto

Estado: version 1.8.0 implementada.

## 1. Identidad e interaccion principal

La ventana de control es una paleta de pintor compacta, siempre visible, movible y
escalada por monitor. El fondo exterior es transparente y el control no roba foco a
la aplicacion sobre la que se presenta.

### Lenguaje visual

- `Obsidian Atelier` combina grafito y obsidiana para las superficies, champaña
  satinado para selección y jerarquía, y azul eléctrico para el mango funcional.
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
- Los puntos de grosor seleccionan 2, 4, 7, 12 y 20 pixeles logicos; 4, el segundo
  punto de arriba hacia abajo, es el valor inicial y se configura desde Ajustes.
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

### Pincel

- Punta del pincel: alterna entre Lapiz e Interactuar (cursor normal).
- Mango azul limpio, unido y alineado con la ferula blanca, y un 10 % mas corto que
  en la version 1.4.
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
3. Resaltador: trazo semitransparente y ancho ampliado.
4. Borrador: elimina objetos tocados; la operacion se puede deshacer.
5. Linea: linea recta con vista previa.
6. Rectangulo: contorno perfecto; `Shift` conserva proporcion cuadrada.
7. Elipse: contorno perfecto; `Shift` conserva proporcion circular.
8. Flecha: linea recta con punta proporcional al grosor.
9. Flecha curva: curva cubica Bezier calculada entre inicio y fin, con arco uniforme,
   limites y hit testing sobre la trayectoria real, y punta tangencial.
10. Texto: al elegir `T`, el siguiente clic fija el punto de insercion y abre un editor
    transparente directamente sobre el escritorio. Admite multilinea, pegado,
    `Ctrl+Enter` para confirmar y `Esc` para cancelar, sin dialogo independiente.
11. Captura: seleccion rectangular, guardado PNG y copia al portapapeles.
12. Zoom: ampliacion en vivo centrada en el puntero, ajuste con rueda y salida con
    `Esc`.

El panel abierto desde el mango reune todas las herramientas en un solo lugar y
añade una fila independiente para Configuracion.

## 3. Documento e historial

- Cada monitor comparte un solo documento en coordenadas del escritorio virtual.
- Deshacer y rehacer conservan inserciones, borrados y limpiezas.
- Limpiar nunca destruye el historial inmediatamente.
- El cambio de visibilidad, color, grosor o herramienta no altera dibujos existentes.
- El historial se limita por cantidad y memoria, descartando primero lo mas antiguo.
- Al cerrar, las preferencias se guardan de forma atomica. Las anotaciones no se
  persisten por defecto para evitar recuperar informacion sensible inesperadamente.

## 4. Pizarra y zoom

- Las pizarras blanca y negra cubren el escritorio virtual, conservan las anotaciones
  y se activan sin recrear el documento. Al entrar en pizarra negra con color negro,
  Elite Pen cambia a amarillo para conservar contraste.
- Zoom usa la capacidad nativa de Windows en proceso x64. La ventana de control se
  excluye de la captura cuando el sistema lo permite para evitar recursion visual.
- Factores admitidos: 1.25x a 8x; valor inicial 2x. Las vistas son pantalla completa,
  lente y acoplada; admiten inversion de color y vista general 1x.
- Con varios monitores, el zoom se limita al monitor que contiene el puntero y migra
  al cruzar de monitor.

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
- Configuración separa `General` y `Atajos`; la segunda pestaña explica todas las
  combinaciones globales y contextuales sin depender de memorización previa.

## 7. Criterios de aceptacion 1.0

- Todas las herramientas crean el resultado esperado en DPI 100 %, 150 % y 200 %.
- La entrada no queda capturada al cambiar a Interactuar o pulsar `Esc`.
- Ocultar/mostrar, pizarra y zoom no pierden el historial.
- Limpiar, deshacer y rehacer funcionan con documento vacio y con miles de objetos.
- Reinicio de Explorer, desconexion de monitor y cambio de resolucion no cierran la
  aplicacion ni dejan una ventana inaccesible.
- No hay bloqueos de interfaz durante trazos largos ni crecimiento de memoria sin
  limite.
- El paquete portable inicia en Windows 10 22H2 y Windows 11 sin runtimes externos.
