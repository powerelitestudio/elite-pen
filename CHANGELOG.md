# Historial de cambios

## 2.9.0 — 2026-08-22

- La vista `L` deja atrás el cuadro rectangular: el contenido ampliado se recorta
  como una lente circular nativa, también al congelar o anotar.
- Un aro óptico premium, transparente y capturable añade profundidad, brillo violeta
  y menta y un borde difuminado de bajo costo sin copiar cuadros en CPU.
- `Shift+rueda` o `[` / `]` recorre cinco diámetros de 360 a 760 px sin modificar el
  factor de ampliación. La rueda normal y `+`/`-` continúan controlando el zoom.
- Configuración > General incorpora el tamaño de la lente y lo conserva en
  `settings.ini` tanto instalado como portable.
- QA comprueba geometría cuadrada, región elíptica, centro visible, esquinas
  recortadas, aro topmost, cambio de tamaño y round trip de preferencias.
- El smoke UI ahora aísla cada ejecución por proceso y no puede cerrar ni confundir
  una copia personal de Elite Pen que ya esté abierta durante las pruebas.

## 2.8.2 — 2026-08-20

- El cursor de Lápiz y Resaltador cambia de +45° a −45° cartesianos: la punta activa
  queda arriba a la izquierda y el cuerpo se extiende abajo a la derecha.
- Al subrayar de izquierda a derecha, el cuerpo del lápiz ya no cubre las letras de
  la línea que se está leyendo; el hotspot continúa exactamente sobre el grafito.
- La corrección se comparte entre el lienzo normal y la tinta de Zoom, conserva el
  rasterizado antialias 4x y el escalado nativo por DPI.
- QA inspecciona el bitmap del cursor y exige el hotspot en el cuadrante superior
  izquierdo, además de repetir la batería completa de interfaz.

## 2.8.1 — 2026-08-19

- La entrada a Zoom en vista completa deja de cortar de forma abrupta: progresa de
  1x al aumento configurado en 180 ms con una curva ease-out breve y perceptible.
- La transición apunta al lugar donde estaba el puntero al invocar `Ctrl+Shift+Z`;
  al terminar recupera inmediatamente el seguimiento en vivo.
- El factor configurado y guardado permanece intacto. Congelar con `P` o clic,
  entrar con `E`, usar la rueda o cambiar de vista completa la transición primero,
  evitando cuadros intermedios y manteniendo las rutas Magnifier y DWM/OBS.
- Las pruebas puras validan inicio, llegada, monotonía, aceleración y límites de la
  curva; el smoke UI comprueba el factor presentado durante y después de la entrada.

## 2.8.0 — 2026-08-11

- Se corrige la pantalla negra comprobada en grabaciones reales de OBS sobre
  Windows 10 híbrido Intel/NVIDIA. Cuando OBS está abierto, Zoom presenta la
  aplicación enfocada mediante composición DWM grabable y conserva Magnifier
  nativo como respaldo para ventanas no compatibles y uso sin OBS.
- OBS debe usar `Captura de pantalla > Método: Windows 10 (1903 y posteriores)`;
  DXGI Desktop Duplication omite tanto Magnifier como miniaturas DWM en este equipo.
- El zoom, la tinta congelada, el indicador de lupa y la barra de Zoom editable
  permanecen capturables por OBS y otros grabadores. Se elimina la afinidad de
  exclusión que hacía aparecer un rectángulo negro en grabaciones de pantalla.
- La prevención de recursión visual continúa a cargo del filtro nativo de Magnifier,
  sin ocultar a los grabadores el contenido ampliado que se está presentando.
- `E` incorpora Zoom editable como flujo adicional a la congelación clásica con
  `P`: una barra flotante alterna `MANO` para usar la aplicación ampliada y `LÁPIZ` para anotar sin
  modificar las vistas `F`, `L`, `D`, inversión, vista general ni salida existente.
- Las anotaciones de Zoom editable se guardan en coordenadas de la fuente ampliada.
  Trazos, grosores, texto, figuras y borrado se desplazan y escalan junto al contenido
  durante paneo o cambios de ampliación, incluso con monitores de coordenadas negativas.
- Una caché GPU dispersa de bloques de 512 px actualiza sólo los bloques tocados;
  navegar transforma únicamente los visibles. El contrato automatizado usa 5.000
  trazos y limita de forma independiente población, primer cuadro, paneo y memoria.
- La barra flotante, la paleta y la tinta conservan un orden topmost determinista;
  `LÁPIZ` entra en anotación y `Espacio` siempre vuelve a Mano. En Mano, clics, rueda
  y teclado se entregan a la aplicación real, por lo que se puede escribir o navegar
  sin abandonar la ampliación.
- Zoom editable admite Lápiz, Resaltador, Borrador, Texto en línea, todas las figuras,
  Captura PNG/portapapeles, Limpiar, Deshacer y Rehacer con historial independiente.
  Un texto todavía abierto se confirma en ese mismo documento antes de volver a Mano.
- Las preferencias y la pestaña Atajos agregan la acción contextual configurable
  `Zoom: editar`, con `E` como valor inicial y conservación portable/local.
- QA añade transformaciones puras, persistencia del nuevo atajo, pruebas UI del flujo
  completo, anclaje durante paneo/zoom, orden Z, captura y regresión integral de `P`.
- La superficie de Zoom editable usa composición directa sin bitmap de redirección y
  queda completamente oculta en Mano, evitando el rectángulo negro que ciertos
  controladores mostraban al entrar con `E`.

## 2.7.1 — 2026-08-10

- Elite Pen pasa a ser software completamente abierto bajo Apache License 2.0.
  El repositorio, el ZIP portable y el instalador incluyen la licencia canónica,
  `NOTICE` y una política separada para las marcas de Power Elite Studio.
- Ayuda identifica la licencia Apache 2.0 y añade un acceso directo al código fuente
  público junto al sitio oficial.
- README, guía de contribución, especificación, QA y checklist público documentan
  de forma coherente los permisos, atribuciones y proceso de publicación abierta.
- Los paquetes oficiales se sincronizan en la versión 2.7.1 y publican hashes
  SHA-256 para el ejecutable portable, ZIP e instalador.
- Los nombres de los activos usan puntos en lugar de espacios para que GitHub los
  preserve literalmente y `SHA256SUMS.txt` pueda verificarse sin renombrar archivos.

## 2.7.0 — 2026-08-10

- Configuración incorpora una pestaña `Ayuda` con versión, propósito, compatibilidad,
  estado de acceso anticipado, autoría de Power Elite Studio y acceso al sitio oficial.
- `Atajos` documenta dentro de la aplicación los cinco gestos temporales del Lápiz:
  `Shift` Línea, `Ctrl` Rectángulo, `Tab` Elipse, `Ctrl+Shift` Flecha y `Shift+Tab`
  Flecha curva Bézier.
- La interfaz de Configuración y su smoke test validan las tres pestañas, el texto
  accesible de gestos y la información mínima de producto.
- El repositorio queda preparado para compartirse públicamente con README de acceso
  anticipado, licencia source-available y uso gratuito, CONTRIBUTING, SECURITY,
  código de conducta, plantillas de comunidad, CI de Windows y checklist de publicación.
- La auditoría local no detecta patrones comunes de secretos ni archivos rastreados
  mayores de 1 MiB; builds, toolchain, artefactos y preferencias permanecen ignorados.
- Un comando de Release crea el ZIP portable, el instalador y `SHA256SUMS.txt` desde
  la misma versión para evitar paquetes manuales divergentes.
- GitHub Actions compila y prueba en Windows; CodeQL analiza C++ en cada cambio a
  `main`, pull request y ejecución semanal.

## 2.6.0 — 2026-08-10

- Captura inteligente sobre zoom congelado: la selección combina la imagen ampliada
  exacta con tinta, texto y geometrías visibles, guarda PNG y copia el mismo bitmap
  al portapapeles sin depender del orden de ventanas de Windows.
- La preferencia de captura sigue decidiendo si la interfaz de Elite Pen queda fuera
  o aparece en el resultado presentado.
- Tema Claro como apariencia inicial para instalaciones y portables nuevos; Oscuro
  continúa disponible y ambas elecciones se conservan.
- Gestos temporales de Lápiz: `Shift` Línea, `Ctrl` Rectángulo, `Tab` Elipse,
  `Ctrl+Shift` Flecha y `Shift+Tab` Flecha curva Bézier.
- El control de contraer bajo el ojo elimina su círculo decorativo y conserva solo
  el glifo limpio, sin reducir la zona interactiva.

## 2.5.2 — 2026-08-10

- El ojo central de Paleta vuelve a descansar directamente sobre la superficie:
  se elimina el círculo violeta decorativo exterior para una lectura más limpia.
- Se conservan sin cambios el párpado cerrado, la pupila, la visibilidad de
  anotaciones y toda la zona de clic.

## 2.5.1 — 2026-08-10

- Paleta adopta las mismas superficies limpias de Lineal: grafito/marfil de baja
  variación, bordes fríos y selección violeta con el mismo contraste semántico.
- El ojo central y el control de hibernación usan las superficies activas de
  Lineal, mejorando jerarquía sin cambiar geometría ni zonas de clic.
- Los colores rápidos eliminan los halos negros pesados y usan una base integrada
  al panel; el anillo violeta conserva una selección inequívoca en ambos temas.
- El mango del pincel reduce su gradiente a los dos violetas canónicos, evitando el
  brillo aislado y armonizando con la barra vertical.
- QA visual compara Paleta y Lineal en Oscuro/Claro; las pruebas funcionales,
  transparencia y rendimiento se mantienen sin regresiones.

## 2.5.0 — 2026-08-10

- Nueva presentación `Lineal`: una barra vertical estrecha, inspirada en la rapidez
  de acceso de Epic Pen pero integrada al sistema premium de Elite Pen.
- `Paleta de pintor` continúa siendo la presentación predeterminada. Configuración
  permite cambiar en vivo entre ambas sin reiniciar y conserva la elección en
  instalaciones y ediciones portables.
- Lineal expone visibilidad, Cursor/Lápiz, herramientas, resaltador, borrador,
  geometría, texto, pizarras, zoom, historial, limpieza y configuración; sus cinco
  grosores y seis colores rápidos permanecen accesibles directamente.
- Los temas Oscuro y Claro, tamaños 80/100/125/150 %, hibernación, arrastre,
  tooltips, paneles y atajos usan el mismo motor en las dos presentaciones.
- La interfaz de control se organizó en un contrato de geometría independiente de
  la lógica de documentos. No se duplican estados, ventanas de dibujo ni rutas GPU.
- QA valida Paleta como valor inicial, cambio bidireccional en vivo, dimensiones,
  colores, grosor, contracción y persistencia de Lineal; se revisaron visualmente
  Lineal oscuro, claro, contraído y la nueva Configuración.

## 2.4.0 — 2026-08-10

- Nuevo esquema global: `Ctrl+Shift+Q` alterna Cursor/Lápiz, `Ctrl+Shift+A`
  visibilidad, `Ctrl+Shift+Z` Zoom, `Ctrl+Shift+E` Limpiar y `Ctrl+Shift+D`
  contraer/expandir. Deshacer pasa a `Ctrl+Alt+Z` para reservar `Z` al zoom.
- `Ctrl+Shift+1..6` selecciona negro, amarillo, azul, rojo, verde y morado;
  `Ctrl+Shift+7` y `Ctrl+Shift++` abren el selector completo de colores.
- `Ctrl+Shift+rueda` recorre los cinco grosores desde cualquier superficie activa
  de Elite Pen, con límites estables y persistencia inmediata de la selección.
- Con Lápiz activo, `Shift`, `Ctrl`, `Tab`, `Ctrl+Shift` y `Ctrl+Alt` convierten el
  arrastre temporalmente en Línea, Rectángulo, Elipse, Flecha o Flecha curva Bézier.
  La misma gramática funciona sobre el zoom congelado sin abandonar dibujo libre.
- Las preferencias incorporan una versión de esquema: las instalaciones existentes
  migran solo combinaciones que aún coinciden con los valores de fábrica 2.3 y
  preservan todos los atajos personalizados.
- Configuración documenta y permite editar las 39 acciones globales y contextuales,
  incluidos los seis colores directos y la combinación alternativa del selector.
- El zoom recupera un centro seguro del escritorio virtual si Windows deniega
  transitoriamente la lectura del puntero, evitando conservar la raíz Magnifier en
  su tamaño de inicialización de 1 × 1 píxel.
- QA cubre migración selectiva, gestos por modificadores, colores directos, selector
  dual, grosor por rueda, contracción por atajo y congelación/anotación de zoom.

## 2.3.0 — 2026-08-10

- El documento terminado se conserva como una superficie GPU retenida: los cuadros
  normales reutilizan un único bitmap y cada trazo nuevo se incorpora de forma
  incremental, sin volver a geometrizar todas las anotaciones anteriores.
- El repintado deja de bloquear el hilo de entrada esperando al compositor; si la
  GPU todavía está ocupada, Elite Pen conserva el cuadro pendiente y lo presenta
  en cuanto queda disponible.
- El historial mueve la propiedad de los trazos entre documento, Deshacer y Rehacer,
  evitando copias profundas. Limpiar y restaurar miles de objetos deja de duplicar
  vectores completos y la cola de historial descarta en tiempo constante.
- Hit testing y borrador usan distancias al cuadrado; las elipses planas mantienen
  tolerancia en píxeles y las flechas incluyen ahora sus puntas tanto en límites
  como en selección y borrado.
- La simplificación Ramer-Douglas-Peucker es iterativa y resiste trazos adversariales
  extensos sin desbordar la pila. Se añadieron pruebas de 100.000 muestras.
- El zoom vivo deja de repetir posición, transformación y fuente 60 veces por
  segundo cuando no cambian puntero, vista ni aumento, y evita redimensionar la
  ventana completa al desplazarse en las vistas Fullscreen y Acoplada.
- QA incorpora un benchmark nativo con 5.000 trazos, cuadro frío, 240 cuadros en
  caché, trazo activo y memoria; también cubre 120 órdenes de borrado compuesto,
  puntas de flecha y trazos adversariales.
- Los scripts del instalador toman la versión desde `VERSION`, evitando artefactos
  con nombres o metadatos desincronizados.

## 2.2.0 — 2026-08-10

- Elite Pen adopta el sistema visual de la familia de software definido por Elite
  Slides: grafito/marfil, superficies frías, violeta principal y menta semántica.
- Configuración incorpora opciones directas `Oscuro` y `Claro`, con cambio inmediato
  sobre paleta, paneles, chrome, controles, zoom, lupa e indicadores.
- El tema se conserva en `settings.ini` tanto en instalaciones como en portables y
  se valida mediante round trip atómico y una consulta automatizada de interfaz.
- El mango pasa del azul aislado a un gradiente violeta de familia; selección,
  foco, cursor de lupa y halo del puntero comparten el mismo lenguaje.
- La arquitectura sigue siendo C++20/Win32/DirectComposition; no se incorporan
  runtimes web ni trabajo adicional por cuadro.

## 2.1.4 — 2026-08-10

- El editor directo de Texto abandona el respaldo heredado de ventana por capas y
  usa exclusivamente DirectComposition con transparencia por píxel. Al pulsar `T`
  solo aparecen el caret y los caracteres, sin rectángulo negro ni panel opaco.
- Finalizar un trazo o borrado normal recompone inmediatamente el orden de ventanas;
  la paleta permanece visible y seleccionable sobre las pizarras blanca y negra.
- El lienzo normal responde `MA_NOACTIVATE`, reforzando que nunca pueda activarse y
  ascender sobre la paleta al recibir el primer clic de dibujo.
- QA dibuja el primer trazo sobre pizarra blanca, verifica orden Z e interacción de
  la paleta y compara un punto vacío del editor de texto antes y después de abrirlo.

## 2.1.3 — 2026-08-10

- Corregido el orden de las superficies del zoom congelado: la paleta, el objetivo
  de lente y la tinta permanecen por encima de la imagen nativa Magnifier después
  de completar un trazo, cambiar color o elegir una geometría.
- El reposicionamiento y los cambios de estilo de `ZoomInk` ya no alteran por su
  cuenta el orden Z; una única rutina restablece toda la jerarquía de forma
  determinista y sin activar ventanas auxiliares.
- El objetivo de la lupa usa exactamente el centro de la región fuente entregada a
  Magnifier. Cuando la captura se ajusta a un borde del monitor, el indicador se
  ajusta al mismo punto y deja de mostrar una referencia desincronizada.
- QA comprueba ahora que `ZoomInk` esté encima de la raíz nativa, que la paleta esté
  encima de ambas y que el punto visual de la lupa coincida con el centro de captura.
  La regresión se ejecuta tanto en pantalla completa como en modo Lente.

## 2.1.2 — 2026-08-10

- La paleta y sus paneles conservan una capa superior estable durante el zoom
  congelado, incluso después de dibujar, cambiar color, grosor o geometría.
- La superficie de tinta ya no se reactiva ni asciende sobre la paleta al recibir el
  primer clic; los comandos siguen disponibles y la paleta se puede mover normalmente.
- El clic para congelar usa un hook de ratón de bajo nivel limitado al zoom vivo,
  ignora el área de la paleta y consume el gesto completo antes de congelar. Así no
  depende del hijo interno al que Windows enrute Magnifier.
- La vista `L` reemplaza el indicador efímero por una lupa transparente, persistente,
  no interactiva y excluida de la ampliación; sigue al puntero con retícula central.
- QA realiza clic físico, repite la congelación, dibuja, cambia a rojo, selecciona
  Rectángulo y comprueba por orden Z que la paleta permanezca sobre `ZoomInk`.

## 2.1.1 — 2026-08-10

- Corregida la instantánea negra al congelar: Elite Pen captura ahora la salida de
  Magnifier ya compuesta en el escritorio, desactiva temporalmente su exclusión de
  captura y rechaza imágenes completamente vacías antes de activar la tinta.
- El clic izquierdo se intercepta directamente en el control nativo Magnifier, por
  lo que congela igual que `P` en pantalla completa, lente y vista acoplada.
- El modo `L` incorpora un cursor de lupa propio, con aro champaña, vidrio sutil y
  retícula azul cuyo centro marca exactamente la zona que se está ampliando.
- QA comprueba clic real sobre el hijo Magnifier, contenido no vacío de la imagen
  congelada y activación del cursor específico de lente.

## 2.1.0 — 2026-08-10

- Corregida la regresión que podía cubrir de negro el zoom al abrirlo con
  `Ctrl+Shift+M`: la capa GPU de anotación permanece oculta durante la ampliación
  viva y se inicializa antes de mostrarse únicamente al congelar.
- El zoom vuelve a seguir el puntero y admite rueda, `+`/`-`, `F`, `L`, `D`, `I`,
  `0`, `Espacio`/`M`; clic o `P` congelan/reanudan y `Esc`, `F4` o clic derecho salen.
- Configuración > Atajos incorpora 32 acciones configurables: controles generales,
  herramientas directas, texto, captura, paneles, contracción y comandos del zoom.
- Cada atajo se edita exclusivamente desde un lápiz a la derecha; la lista es
  desplazable, admite combinaciones contextuales de una tecla y permite desasignar
  con `Supr` o `Retroceso` sin perder la restauración de fábrica.
- El selector `+` es más pequeño y sigue mejor el arco de colores al desplazarse
  ligeramente hacia abajo y a la derecha.
- En modo contraído solo el icono central expande; el resto de la mini paleta muestra
  el cursor de cuatro flechas y permite moverla directamente. El icono también reduce
  su presencia visual.
- La publicación portable conserva `data/settings.ini` y archiva cada versión previa
  en una carpeta versionada, sin sobrescribir respaldos anteriores.
- QA de interfaz comprueba que la capa `ZoomInk` esté oculta en vivo, congelación por
  clic, lista de atajos con lápices y scroll, y movimiento compacto sin expansión.

## 2.0.0 — 2026-08-10

- `P` congela o reanuda el zoom nativo; al congelar se activa automáticamente el
  lápiz sobre una instantánea estable de la salida ampliada.
- Nueva capa `ZoomInk` acelerada por GPU con lápiz, resaltador, borrador, texto,
  líneas, rectángulos, elipses y flechas rectas o Bézier, además de ratón, stylus,
  presión y tacto.
- Las anotaciones de zoom permanecen visibles al reanudar y tienen documento,
  papelera, deshacer y rehacer independientes de la tinta normal.
- `Esc` cierra el zoom y abandona las pizarras blanca o negra conservando el historial.
- Nuevo modo de hibernación: el control bajo el ojo oculta toda la herramienta y deja
  una paleta mínima al 30 % del tamaño normal con una única acción para expandir.
- Mango azul un 10 % adicional más corto y papelera reubicada junto a su extremo.
- Los ocho atajos globales se editan individualmente desde Configuración > Atajos,
  validan duplicados o combinaciones ocupadas y se restablecen con un clic.
- Atajos, escala, posición y modo contraído persisten de forma atómica tanto en
  LocalAppData como en `data/settings.ini` de la edición portable.
- QA automatizado cubre congelación, tinta de zoom, reanudación, limpieza contextual,
  historial, Escape, ambas pizarras, hibernación, expansión y persistencia portable.

## 1.9.0 — 2026-08-09

- El modo Lápiz reemplaza la cruz genérica de Windows por un cursor nativo de lápiz
  inclinado, compacto y reconocible, siguiendo el lenguaje funcional de Epic Pen.
- La punta de grafito es el hotspot real: el trazo comienza exactamente allí, sin
  desplazamiento visual respecto del ratón o stylus.
- El cursor se rasteriza con antialiasing 4x, contorno claro/oscuro y tamaño adaptado
  al DPI de cada monitor; Resaltador comparte el lápiz y las figuras conservan la
  cruceta de precisión.
- QA inspecciona el bitmap nativo, descarta el cursor `X` y verifica dimensiones,
  color y ubicación proporcional del hotspot.

## 1.8.1 — 2026-08-08

- Corregido el rectángulo negro que podía envolver la paleta después de cambiar
  su tamaño: DirectComposition conserva ahora la transparencia por píxel al
  redimensionar la cadena gráfica.
- Nueva regresión visual aislada que coloca la paleta sobre blanco, prueba las
  cuatro escalas y valida por píxeles tanto las zonas transparentes como el
  contenido visible.

## 1.8.0 — 2026-08-08

- Nuevo selector de tamaño integral con cuatro niveles: Compacta 80 %, Estándar
  100 %, Grande 125 % y Muy grande 150 %.
- El tamaño actual se conserva como Estándar y el escalado transforma conjuntamente
  paleta, colores, puntos de grosor, ojo, pincel, papelera, tooltips y zonas de clic.
- El cambio de tamaño mantiene el centro visual, respeta los límites del monitor y
  persiste entre sesiones para las ediciones portable e instalada.
- Configuración se reorganiza en pestañas premium `General` y `Atajos`.
- La pestaña Atajos documenta dibujo, visibilidad, edición, pizarras, zoom y entrada
  de texto con la acción exacta de cada combinación.
- QA automatizado para tamaños compacto, estándar y grande, incluyendo dimensiones
  reales y selección correcta de color y grosor después del escalado.

## 1.7.0 — 2026-08-08

- Arrastre reconstruido con coordenadas absolutas de pantalla: la paleta sigue al
  puntero sin oscilación, rebote ni sensación de lastre.
- La papelera se acerca ópticamente al extremo del pincel, reduce su peso visual y
  conserva una zona de clic separada para evitar limpiezas accidentales.
- Los selectores de Configuración reemplazan los últimos campos blancos nativos por
  superficies oscuras, cheurones champaña y foco coherente con Obsidian Atelier.
- Configuración adopta la misma firma vertical dorada de Herramientas y Colores.
- Nueva regresión automatizada que desplaza la paleta en dos tramos y verifica que
  cada delta se siga exactamente después de mover la propia ventana.

## 1.6.0 — 2026-08-08

- Nuevo sistema visual `Obsidian Atelier`: superficies grafito/obsidiana, metal
  champaña y azul eléctrico como acento funcional.
- Paleta principal reconstruida con volumen sutil, borde metálico, pozos de color
  pulidos, selección luminosa y una férula con acabado de metal satinado.
- Paneles de Herramientas y Colores unificados con tarjetas elevadas, estados hover,
  jerarquía tipográfica moderna y contraste más claro.
- Configuración adopta marco propio redondeado, controles oscuros, casillas y botones
  dibujados a medida, eliminando la apariencia clásica de Windows.
- La punta conserva visualmente la tinta activa y la papelera gana presencia sin
  competir con los controles principales.
- Compatibilidad visual progresiva: Segoe UI Variable y esquinas DWM en Windows 11,
  con sustitución tipográfica y marco propio en Windows 10.

## 1.5.0 — 2026-08-08

- Mango azul completamente limpio: se retiran los iconos de Texto, Figuras y
  Configuracion.
- Un clic en cualquier punto del mango abre el panel general con todas las
  herramientas y un nuevo acceso a Configuracion.
- El mango se acorta un 10 % adicional sin separarse de la ferula blanca.
- Se eliminan por completo la etiqueta de herramienta seleccionada y la muestra
  inferior de color y grosor activos.
- La zona interactiva del mango ahora sigue exactamente su segmento visible.

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
