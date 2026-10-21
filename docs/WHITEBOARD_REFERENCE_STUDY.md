# Estudio de SQLBI Whiteboard y diseño de Elite Pen 2.16

Fecha: 2026-10-21. Referencia solicitada por el propietario del equipo:
`C:\Program Files\SQLBI\Whiteboard\SQLBI.Whiteboard.exe`.

## Evidencia examinada

Se utilizó REA MCP para inventariar el ejecutable y las bibliotecas administradas
instaladas. Versión observada: **1.7.6**, compilación
`4cf5355685345e7cb7b63786408d7c18e02f8d77`. El producto usa ensamblados .NET con
metadatos CIL y ReadyToRun. La investigación se limitó a tipos, miembros, campos
y cadenas de metadatos. No se copiaron código, recursos, iconos ni algoritmos del producto.

| Ensamblado | SHA-256 | Inventario |
| --- | --- | --- |
| SQLBI.Whiteboard.Core.dll | `8c37e75e987dfec1e0165b82b68d773a0df757a320a04cc544d8ae29d26b2739` | 257 tipos, 2.612 métodos, 1.233 campos |
| SQLBI.Whiteboard.dll | `4dc09e6922e92783cd2d80ff3f61457a1ca31dad33ff62807668c180d1a3ff3f` | 353 tipos, 2.623 métodos, 1.838 campos |

Los inventarios de tipos, métodos y campos finalizaron completos. El inventario
de cadenas tuvo advertencias de terminación del proveedor: no se considera
exhaustivo. Se profundizó en ocho tipos del núcleo y cuatro del host, con 206
métodos y 49 campos. REA en este host no ofrece observación de interfaz nativa
Windows: los resultados no prueban su interacción visual ni el comportamiento
interno de cada método. Se contrastó el diseño funcional con la
[guía oficial de SQLBI Whiteboard](https://whiteboard.sqlbi.com/guide.html).

## Hallazgos y decisiones

| Evidencia de metadatos | Lectura del diseño | Decisión propia en Elite Pen |
| --- | --- | --- |
| `Core.Viewport.Camera2D`: Center, Zoom, WorldToScreen, ScreenToWorld, PanByScreenDelta, ZoomAt, Frame, Restore | Cámara separada del contenido | Cámara en coordenadas del mundo; bitmap GPU del viewport, sin reservar un bitmap del tamaño del lienzo |
| `Core.Model.BoardDocument`: objetos, activos, Snapshot, Query, HitTestTopSelectable, ObjectsInArea, GrowSelection | Modelo de objetos seleccionables | Objetos inmutables compartidos, historial independiente, selección, transformaciones y orden de capas |
| `Core.Model.InkPoint`: Position, Pressure, Timestamp | La presión pertenece a cada muestra | Vector de presión alineado con los puntos, registrado por WM_POINTER y conservado en archivos |
| `Core.Model.ImageBoardObject`: AssetId | Los recursos gráficos son entidades reutilizables | PNG embebido una sola vez por recurso compartido y caché de imágenes GPU limitada |
| `Core.Model.AreaSelection`: PartlyInside y FullyInside | La selección tiene una geometría propia | En esta versión, el área selecciona los objetos no bloqueados que interseca |
| `Core.Persistence.BoardArchive`: versión, escena, previsualización, SaveAsync, LoadAsync | Persistencia independiente del render | Formato propio versionado, validación de tamaño, datos finitos y checksum; escritura temporal y reemplazo atómico |
| `Core.Persistence.SessionState`: BoardPath, Modified, ExitedCleanly, WrittenUtc, CameraCenterX/Y, CameraZoom | Recuperación separada del guardado explícito | Recuperación local con ruta informativa, documento y cámara; reabrir no sobrescribe por sí solo el archivo nombrado |
| Host `SessionStore`: SlotId, FolderPath, lockStream, Acquire, WriteAsync, FindAbandoned, Prune, Forget, Clear | Gestión de sesiones con propiedad | Elite Pen mantiene una sesión de pizarra por instancia; copia anterior y archivos de sesiones antes de cambiar de documento |

La separación en cámara, documento y sesión también coincide con los recorridos
descritos en la guía oficial: escribir, navegar, seleccionar y continuar después.
La implementación nativa de Elite Pen se diseñó desde sus propias abstracciones
Win32, Direct2D, WIC y modelo vectorial existente.

## Alcance de esta primera versión

Incluye lienzo amplio, tinta con presión, fondos blanco/negro, figuras y símbolos,
texto, emojis y favoritos existentes, imágenes, captura de región, selección y
transformación conjunta, capas de orden y bloqueo, guardado editable, exportación
de vista y recuperación local. El panel se amplía hasta el 300 % y admite macropad.

No se implementó compatibilidad con `.wboard`, importaciones de PowerPoint,
contenedores de código, LiveViews, sincronización en nube ni enlaces automáticos
entre una imagen y los trazos superpuestos. Para mover imagen y tinta juntas se
seleccionan ambas. Es una primera versión propia, no una reproducción completa de SQLBI.

La presión física depende del controlador Windows Ink. Se documenta el ajuste
para Deco 01 V3 a partir del
[manual oficial de XPPen](https://www.xp-pen.com/user-manual/deco-01-v3.html).
Las pruebas de Windows con lápiz sintético se distinguen de una prueba manual
con la tableta física.
