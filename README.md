# ExpressC

ExpressC is a lightweight HTTP server / routing library written in C, inspired by the simplicity of frameworks like Express.js.

The goal of the project is to make it easier to build small web servers in C without having to manually handle every low-level detail for routing and request/response handling.

## What is ExpressC?

ExpressC provides a simple way to:

- define routes
- handle incoming HTTP requests
- build HTTP responses
- organize server logic more cleanly

This project is still in progress, but the long-term idea is to provide a minimal and practical framework for:

- route registration
- request parsing
- response building
- middleware support
- basic server utilities

## Setup

### Requirements

- a C compiler such as `gcc` (which was used to test this project)
- POSIX-compatible environment recommended (Linux/macOS)

### Build

If you are compiling manually:

```bash
gcc -o app main.c ExpressC.c TCPServer/TCPServer.c
```

### Next Steps

* [ ] Cookie Management functions.
* [ ] Improved Header and param parsing.
* [ ] Colored logging messages in dev mode.
* [ ] Configurable modes for dev/prod.
* [ ] Error status codes.

### Future plans
* [ ] Support for TLS
* [ ] HTTP/2.0
