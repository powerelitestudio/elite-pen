# Preparación del repositorio público

Auditoría local: 10 de agosto de 2026. Objetivo: publicar Elite Pen como acceso
anticipado, con código fuente visible y paquetes gratuitos listos para usar.

## Ya preparado en el repositorio

- [x] README público con estado, instalación, compilación, privacidad y licencia.
- [x] Licencia source-available coherente con acceso al código y uso gratuito.
- [x] Guía de contribución, código de conducta y política de seguridad.
- [x] Plantillas para errores, mejoras y pull requests.
- [x] CI de Windows con permisos de solo lectura, pruebas y compilación Release.
- [x] CodeQL para C++ con permisos mínimos y compilación manual reproducible.
- [x] Binarios, builds, toolchain, artefactos y preferencias locales ignorados por Git.
- [x] Revisión de nombres sensibles y patrones comunes de secretos en archivos
  rastreados: sin coincidencias.
- [x] Revisión de tamaño: ningún archivo rastreado supera 1 MiB.
- [x] El toolchain descargado está fijado por versión y SHA-256.
- [x] Un comando genera ZIP portable, instalador y `SHA256SUMS.txt` desde la misma
  versión.

## Antes de cambiar la visibilidad en GitHub

- [ ] Decidir si se publica todo el historial o una rama limpia. El historial actual
  contiene el correo del autor de los commits; usa un correo `noreply` o reescribe el
  historial si no debe quedar público.
- [ ] Confirmar el nombre y la organización propietarios del repositorio.
- [ ] Revisar una última vez `git diff`, el historial y los archivos rastreados.
- [ ] Crear la rama `main`, protección de rama y revisión obligatoria si habrá más
  mantenedores.
- [ ] Habilitar Secret scanning, Push protection, Code scanning y alertas de
  dependencias en **Settings → Security**.
- [ ] Habilitar **Private vulnerability reporting** para que `SECURITY.md` tenga un
  canal privado real.
- [ ] Definir Topics, descripción corta y enlace https://powerelite.studio/.

## Primera Release pública

- [x] Ejecutar toda la matriz local: unitarias, preferencias, rendimiento, interfaz,
  transparencia, portable e instalador.
- [ ] Publicar portable e instalador creados desde el mismo commit/tag.
- [ ] Adjuntar un archivo `SHA256SUMS.txt` y comprobar los hashes después de subir.
- [ ] Explicar que los binarios todavía no están firmados y cómo verificar el hash.
- [ ] Crear tag `v2.7.0`, notas basadas en `CHANGELOG.md` y marcar la versión como
  acceso anticipado si corresponde.
- [ ] Probar la descarga en un Windows 10 o 11 limpio, sin toolchain ni preferencias.

## Decisión posterior, no bloqueante

La licencia actual permite ver el código, compilarlo sin cambios y usar Elite Pen,
pero no permite forks ni redistribución. Si Power Elite Studio desea una comunidad
de código abierto, debe elegir expresamente otra licencia (por ejemplo MIT o Apache
2.0) y revisar antes las implicaciones de marca, patentes y contribuciones.
