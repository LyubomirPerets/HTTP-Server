# HTTP Server in C

A simple event-driven HTTP server written from scratch in C using POSIX sockets and `epoll`.

## Features
- TCP socket-based server (default port 8080)
- HTTP request line parsing (method, path, version) via bounded `sscanf`
- Path-based routing to static HTML files
- 404 handling for unknown routes
- Single-process client handling via `epoll`
- `SO_REUSEADDR` for instant restart
- Prometheus-style metrics at `/metrics`

## Build & Run

\`\`\`bash
gcc -Wall -Wextra -g server.c -o server
./server
\`\`\`

Then open `http://localhost:8080` in your browser.

## Testing

\`\`\`bash
curl http://localhost:8080/         # serves index.html
curl http://localhost:8080/about    # serves about.html
curl -i http://localhost:8080/nope  # returns 404
\`\`\`

## Architecture

- Single listening socket accepts connections in the main process loop.
- The listening socket and client sockets are registered with `epoll`.
- Ready client sockets are handled without spawning child processes.
- Each request is routed to a static HTML file, metrics output, or a 404 response.

## Roadmap
- [x] Replace fork-per-connection with `epoll` event loop (single-thread, scalable)
- [ ] HTTP/1.1 keep-alive connections
- [ ] Chunked transfer encoding
- [ ] HTTPS via OpenSSL
- [ ] Request header parsing
- [ ] Benchmarking with `wrk`

## Built With
C, POSIX sockets, Linux (developed on WSL2 / Ubuntu)
