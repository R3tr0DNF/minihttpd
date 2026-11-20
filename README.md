# MiniHTTPd - Jumbo Frank

Un servidor HTTP con concurrencia implementado con EPOLL en C, sin librerias externas, con procesamiento de solicitudes HTTP

## Características

- Servidor TCP usando socket.
- Implementacion de la concurrencia event-driven con epoll y sockets no bloqueantes en un solo hilo.
- Procesamiento de peticiones HTTP/1.1.
- Soporte para el método `GET`.
- Conexiones persistentes mediante HTTP Keep-Alive.
- Servido de archivos estáticos desde el directorio `www/`.
- Detección de tipos MIME según la extensión del archivo y soporte de los codigos 400, 403, 404, 405, 500.


## Requisitos

- Linux
- GCC

## Estructura del proyecto

```text
MiniHTTPd/
├── include/
│   ├── files.h
│   ├── http.h
│   ├── mime.h
│   └── server.h
├── src/
│   ├── files.c
│   ├── http.c
│   ├── main.c
│   ├── mime.c
│   └── server.c
├── www/
│   ├── index.html
│   ├── style.css
│   ├── app.js
│   ├── epn.png
│   └── epn.jpg
├── Makefile
└── README.md
```

### Archivos

| Archivo | ¿Qué hace? |
|---|---|
| `main.c` | Coordina epoll, recibe las solicitudes, respuesta y envio de datos. |
| `server.c` | Creación y configuración del socket del servidor. |
| `http.c` | Análisis de peticiones y construcción de respuestas HTTP. |
| `files.c` | Localización, validación y apertura de archivos estáticos. |
| `mime.c` | Identificación del tipo MIME de los archivos. |

Los encabezados de `include/` contienen las declaraciones y estructuras compartidas entre los módulos.

## Compilación

Clonar el repositorio:

```bash
git clone <https://github.com/R3tr0DNF/minihttpd>
cd MiniHTTPd
```

Compilar el proyecto utilizando Make:

```bash
make
```

Esto genera el ejecutable `minihttpd` en el directorio raíz del proyecto.

## Ejecución

Iniciar el servidor:

```bash
./minihttpd
```

El servidor escucha en el puerto **5000**, utilizando la dirección
