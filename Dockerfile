FROM gcc:14-bookworm AS build

WORKDIR /app
COPY server.c .
RUN gcc -Wall -Wextra -O2 server.c -o server

FROM debian:bookworm-slim

WORKDIR /app
COPY --from=build /app/server .
COPY index.html about.html contact.html ./

EXPOSE 8080

CMD ["./server"]
