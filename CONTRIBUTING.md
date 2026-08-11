# Contribuir a Elite Pen

Gracias por ayudar a mejorar Elite Pen. El proyecto está en acceso anticipado y
prioriza estabilidad, rendimiento en equipos modestos y una experiencia clara en
Windows 10 y Windows 11.

## Antes de abrir una incidencia

- Busca si el problema o la propuesta ya existe.
- Prueba la versión más reciente disponible en Releases.
- No adjuntes capturas, configuraciones ni volcados con información privada.
- Para vulnerabilidades, usa el canal descrito en [SECURITY.md](SECURITY.md); no
  publiques detalles explotables en una incidencia.

En un error incluye versión de Elite Pen, edición de Windows, número de monitores,
escalas DPI, GPU, pasos mínimos para reproducirlo, resultado esperado y resultado
real. Indica si ocurre en la edición portable, instalada o ambas.

## Preparar el entorno

Requisitos: Windows 10 u 11 x64, PowerShell 7 y Git. La cadena LLVM-MinGW se descarga
desde su publicación oficial y se verifica con el SHA-256 fijado en
`tools/toolchain.json`.

```powershell
pwsh -File .\scripts\bootstrap-toolchain.ps1
pwsh -File .\scripts\build.ps1 -Configuration Debug
pwsh -File .\scripts\test.ps1 -Configuration Debug
```

Antes de proponer un cambio ejecuta también la validación de Release:

```powershell
pwsh -File .\scripts\build.ps1 -Configuration Release
pwsh -File .\scripts\test.ps1 -Configuration Release
pwsh -File .\scripts\render-performance-test.ps1 -Configuration Release
pwsh -File .\scripts\ui-smoke-test.ps1 -Configuration Release
```

Los activos finales se crean con `scripts/build-release-assets.ps1`; no armes el ZIP
o el instalador a mano para una publicación oficial.

## Propuestas de cambio

- Mantén cada pull request enfocado en un problema concreto.
- Explica el comportamiento anterior, el nuevo y las pruebas realizadas.
- Añade o actualiza pruebas cuando cambie la lógica.
- Actualiza README, especificación, QA y changelog cuando cambie el contrato visible.
- No añadas telemetría, red, elevación, servicios, controladores ni dependencias
  persistentes sin una discusión y aprobación previas.
- Preserva la compatibilidad x64 con Windows 10 y el modo portable sin instalación.

Al enviar una contribución declaras que tienes derecho a aportarla y autorizas a
Power Elite Studio a incorporarla y distribuirla bajo la licencia vigente de Elite
Pen. Abrir un pull request no concede permiso para redistribuir forks o binarios.

## Conducta

Participa con respeto, critica el código y las decisiones —no a las personas— y
evita publicar datos personales. Power Elite Studio puede moderar o cerrar
interacciones abusivas, engañosas o fuera del alcance del proyecto.
