# Elite Pen

Elite Pen es la herramienta nativa de anotacion, pizarra y ampliacion de pantalla de
Power Elite Studio. Su interfaz principal toma la forma de una paleta de pintor y un
pincel funcional; no replica la barra vertical de otras aplicaciones.

## Objetivos del producto

- Dibujar encima de cualquier aplicacion sin interrumpir el flujo de trabajo.
- Ocultar y recuperar anotaciones sin perderlas.
- Crear texto, lineas, rectangulos, elipses, flechas rectas y flechas curvas.
- Capturar una region en PNG y copiarla al portapapeles.
- Usar pizarras blanca y negra instantaneas.
- Ampliar la pantalla en vista completa, lente o acoplada y seguir el puntero.
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
| Texto | `T` del mango | — |
| Figuras | Control central del mango | — |
| Configuracion | Control derecho del mango | — |
| Deshacer | Menu/historial | `Ctrl+Shift+Z` |
| Rehacer | Menu/historial | `Ctrl+Shift+Y` |
| Limpiar | Papelera | `Ctrl+Shift+C` |
| Zoom | Herramientas | `Ctrl+Shift+M` |
| Salir de modo/zoom | — | `Esc` |

En zoom: `F` pantalla completa, `L` lente, `D` acoplado, `I` invertir,
rueda/`+`/`-` para ampliar y `0` para vista general.

La especificacion funcional completa esta en
[`docs/PRODUCT_SPEC.md`](docs/PRODUCT_SPEC.md).
