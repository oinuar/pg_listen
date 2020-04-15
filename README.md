## pg_listen: trigger a shell command on a Postgres event

![CI](https://github.com/oinuar/pg_listen/workflows/CI/badge.svg)

Super fast and lightweight. Written in C using libpq.

### Usage

You need to provide PostgreSQL connection string, `LISTEN` channel and a program that is executed when a notification is published to the channel.

```bash
pg_listen postgres://db-uri channel [/path/to/program] [args]
```

Providing the program is optional. If it is omitted, the payload is printed to STDOUT.

```bash
pg_listen postgres://localhost/postgres channel
```

### Building

Clone the repo and then run:

```bash
docker build -t oinuar/pg_listen:latest .
```

Or you can grab the already built Docker image for your architecture from [Docker hub](https://hub.docker.com/r/oinuar/pg_listen).
