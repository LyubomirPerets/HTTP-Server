# HTTP Server in C

A simple multi-process HTTP server written from scratch in C using POSIX sockets.

## Features
- TCP socket-based server (default port 8080)
- HTTP request line parsing (method, path, version) via bounded `sscanf`
- Path-based routing to static HTML files
- 404 handling for unknown routes
- Concurrent client handling via `fork()`
- `SO_REUSEADDR` for instant restart
- Automatic zombie process reaping via `SIGCHLD` / `SIG_IGN`

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
- Each accepted connection spawns a child process via `fork()` to handle the request.
- Child closes the listening socket and exits after responding.
- Parent immediately returns to `accept()` for the next client.
- `SIGCHLD` is ignored so the kernel auto-reaps exited children, preventing zombies.

## Roadmap
- [ ] Replace fork-per-connection with `epoll` event loop (single-thread, scalable)
- [ ] HTTP/1.1 keep-alive connections
- [ ] Chunked transfer encoding
- [ ] HTTPS via OpenSSL
- [ ] Request header parsing
- [ ] Benchmarking with `wrk`

## Built With
C, POSIX sockets, Linux (developed on WSL2 / Ubuntu)
