# Arquitectura

## Capas

- `core`: modelo vectorial, geometria, hit testing, historial y preferencias. No
  depende de ventanas y se prueba de forma determinista.
- `platform/windows`: ciclo Win32, DPI, atajos, bandeja, entrada y ampliacion.
- `graphics`: dispositivo D3D11/D2D compartido y superficies DirectComposition con
  alfa premultiplicado.
- `ui`: paleta-pincel, selector de herramientas, selector de color y configuracion.
- `capture`: seleccion de region, captura GDI con respaldo DXGI Desktop Duplication,
  transferencia al portapapeles y codificacion PNG mediante Windows Imaging Component.

## Flujo de datos

La entrada de la superposicion se transforma a coordenadas del escritorio virtual.
El controlador produce una operacion de documento. El historial la aplica y notifica
una invalidacion. El renderizador vuelve a componer pizarra, documento y vista previa;
la paleta conserva su propia superficie y nunca se mezcla con el documento.
Cuando cambia la herramienta, las superposiciones recuperan su modo de entrada y el
controlador restablece despues la paleta en la cima del grupo topmost. Asi sus zonas
accionables siguen recibiendo clics mientras el lienzo esta en modo de dibujo.

## Decisiones

### ADR-001: C++ nativo y DirectComposition

Se usa C++20/Win32 para minimizar tiempo de inicio, memoria, latencia y dependencias.
DirectComposition permite ventanas transparentes aceleradas sin copiar cada cuadro a
memoria de CPU.

### ADR-002: coordenadas virtuales en DIPs

El modelo guarda DIPs relativos al origen del escritorio virtual. La capa de ventana
convierte desde/hacia pixeles por monitor. Esto evita que el contenido cambie de
tamano al trasladar la paleta o modificar el escalado.

### ADR-003: historial por operaciones

El historial conserva operaciones reversibles, no imagenes completas. Una limpieza
guarda los objetos retirados y un borrado guarda indice y objeto, lo que mantiene
deshacer exacto con menor uso de memoria.

### ADR-004: herramienta portable reproducible

La compilacion se fija a LLVM-MinGW/UCRT x64 y se valida con SHA-256. La herramienta
queda ignorada por Git; su version y procedencia viven en `tools/toolchain.json`.

### ADR-005: configuracion portable o por usuario

La presencia de `portable.flag` mueve el archivo de preferencias a `data` junto al
ejecutable. La version instalada usa LocalAppData. Ambas rutas mantienen el mismo
formato y no requieren registro, servicio ni permisos de administrador.
