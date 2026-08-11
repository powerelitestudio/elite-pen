# Seguridad de Elite Pen

## Versiones con soporte

| Versión | Soporte de seguridad |
|---|---|
| 2.7.x | Sí |
| 2.6.x y anteriores | No; actualiza antes de reportar |

Mientras Elite Pen esté en acceso anticipado, las correcciones se publican sobre la
línea más reciente y no se mantienen ramas antiguas.

## Reportar una vulnerabilidad

No publiques una vulnerabilidad ni datos sensibles en Issues. Cuando el repositorio
sea público, usa **Security → Report a vulnerability** para enviar un informe privado.
Si esa opción todavía no está habilitada, abre una incidencia sin detalles técnicos
solicitando un canal privado o contacta a Power Elite Studio desde
https://powerelite.studio/.

Incluye, cuando sea posible:

- versión exacta y tipo de paquete (portable o instalado);
- versión de Windows y arquitectura;
- impacto y escenario realista;
- pasos mínimos o prueba de concepto segura;
- mitigaciones conocidas.

No accedas a información ajena, no interrumpas sistemas de terceros y concede tiempo
razonable para investigar y publicar una corrección coordinada. Power Elite Studio
confirmará la recepción cuando exista un canal de contacto y podrá solicitar datos
adicionales.

## Alcance especialmente relevante

Elite Pen usa superposiciones, atajos globales, captura de pantalla, portapapeles y
la API Magnifier de Windows. Son especialmente útiles los reportes sobre exposición
involuntaria de capturas, persistencia inesperada de datos, suplantación de interfaz,
escalada de privilegios, carga insegura de bibliotecas o escritura fuera de las rutas
de configuración documentadas.
