# Historial de cambios

## 1.4.0 — 2026-08-08

- Todo el componente principal se reduce de 290 x 280 a 174 x 168 px: 40 % menos
  en ambas dimensiones, conservando sus proporciones y comportamiento.
- La ferula blanca y el mango azul quedan unidos sobre el mismo eje, sin salto ni
  separacion visual.
- Los accesos `T`, Figuras y Configuracion pierden sus fondos circulares y descansan
  directamente sobre el mango azul.
- Las zonas de clic, el enrutamiento desde el lienzo y los tooltips se adaptan al
  nuevo escalado para mantener todos los comandos seleccionables mientras se dibuja.

## 1.3.0 — 2026-08-08

- Panel de Figuras reducido a cinco botones visuales sin nombres repetidos.
- Flecha curva reconstruida como una curva cubica Bezier con punta tangencial.
- Texto directo sobre el escritorio mediante un editor transparente con caret.
- La papelera reemplaza a la gota de agua como accion de limpieza.
- Grosor inicial de 4 px, correspondiente al segundo punto, configurable en Ajustes.
- Indicador activo inclinado, sin tarjeta de fondo, mostrando color, grosor y herramienta.
- El selector avanzado queda representado por un `+` limpio, sin circulo exterior.
- La altura total de la paleta baja de 320 a 280 px.

## 1.2.0 — 2026-08-08

- El estado oculto del control central ahora se representa con un ojo cerrado elegante.
- Se agrega verde como sexto color rapido directo.
- Los seis colores y el selector `+` se compactan alrededor del ojo sin reducir sus
  areas de clic.

## 1.1.1 — 2026-08-08

- El overlay detecta y enruta los clics sobre la paleta en modo dibujo.
- Color, grosor, ojo, punta, pizarra, Texto, Figuras, Configuracion y Limpiar ya no
  pueden convertirse accidentalmente en trazos.
- Prueba de regresion que reproduce el clic a traves del overlay.

## 1.1.0 — 2026-08-08

- La paleta permanece seleccionable por encima del lienzo en todos los modos.
- Nuevo orden rapido: negro, amarillo, azul, rojo, morado y selector `+`.
- Mango del pincel 40 % mas corto y gota de limpieza redisenada.
- Accesos directos en el mango para Texto, Figuras y Configuracion.
- Panel geometrico compacto con linea, rectangulo, elipse y flechas.
- Cursores de mano y ayudas emergentes en todas las zonas accionables.

## 1.0.0 — 2026-08-08

- Primera version publica de Elite Pen para Windows 10 y Windows 11 x64.
- Paleta de pintor acelerada por GPU, colores rapidos y selector de 42 colores.
- Punta del pincel como alternador directo entre lapiz y cursor normal.
- Lapiz, resaltador, borrador, linea, rectangulo, elipse, flechas, texto y captura.
- Pizarras blanca y negra, ocultar/mostrar, limpiar, deshacer y rehacer.
- Zoom completo, lente y acoplado con inversion y seguimiento del puntero.
- Tinta temporal, halo de cursor, entrada de lapiz con presion y varios monitores.
- Distribuciones portable e instalable sin privilegios de administrador.
