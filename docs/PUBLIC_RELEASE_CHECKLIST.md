# Publicación abierta de Elite Pen

Última actualización: 19 de agosto de 2026. Elite Pen se mantiene en el repositorio
público <https://github.com/powerelitestudio/elite-pen>. La versión 2.8.1 continúa
el modelo íntegramente abierto bajo Apache License 2.0 inaugurado por 2.7.1; la
licencia source-available de 2.7.0 solo conserva valor histórico para aquel tag y
sus paquetes.

## Repositorio y gobierno

- [x] `LICENSE` contiene el texto canónico e íntegro de Apache License 2.0.
- [x] `NOTICE` acredita a Power Elite Studio sin modificar la licencia.
- [x] `TRADEMARKS.md` separa los derechos sobre el código de las marcas, nombres,
  logotipos e identidad visual.
- [x] README y CONTRIBUTING describen los permisos de uso, modificación,
  redistribución, contribución y las obligaciones de atribución.
- [x] Código de conducta, política de seguridad, plantillas de incidencias y pull
  requests disponibles.
- [x] Repositorio público con `main`, CI de Windows y CodeQL.
- [x] Secret scanning, Push protection, Code scanning, alertas de dependencias y
  Private vulnerability reporting habilitados.
- [x] Topics, descripción y enlace oficial configurados.
- [x] Binarios, builds, toolchain, artefactos y preferencias locales ignorados por
  Git; ningún secreto ni archivo rastreado mayor de 1 MiB detectado en la auditoría.

## Contrato de los paquetes oficiales

- El ZIP portable y el instalador nacen del mismo commit y la misma versión.
- Ambos incluyen `LICENSE.txt`, `NOTICE`, `TRADEMARKS.md` y la guía de uso.
- `SHA256SUMS.txt` publica los hashes del ZIP y del instalador.
- La edición portable incluye además hashes internos y `build-info.json`.
- Los binarios sin firma digital se anuncian expresamente; nunca se oculta la posible
  advertencia de SmartScreen.
- Al actualizar `D:\Aplicaciones\Elite Pen`, se preserva primero su carpeta `data`;
  la copia temporal anterior sólo se elimina después de verificar versión y hashes.

## Control por cada Release

- [ ] Ejecutar unitarias, preferencias, rendimiento, interfaz y transparencia.
- [ ] Inspeccionar visualmente Configuración > Ayuda y su acción de código fuente.
- [ ] Validar inicio portable e instalación/desinstalación silenciosa aislada.
- [ ] Confirmar versión, contenido de licencias y hashes del ZIP y del instalador.
- [ ] Crear el tag desde `main`, publicar notas basadas en `CHANGELOG.md` y adjuntar
  los tres activos generados por `scripts/build-release-assets.ps1`.
- [ ] Descargar los activos publicados, recalcular SHA-256 y comprobar que GitHub
  detecta `Apache-2.0` como licencia del repositorio.

Estos pasos se repiten para cada versión: las casillas no sustituyen la evidencia
concreta registrada en [QA_REPORT.md](QA_REPORT.md) y en GitHub Actions.
