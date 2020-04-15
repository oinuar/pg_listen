FROM alpine:3.11.5 as base

RUN apk add --no-cache libpq


FROM base as build

RUN apk add --no-cache gcc make pkgconfig postgresql-dev musl-dev

WORKDIR /src

COPY . . 

RUN make


FROM base

COPY --from=build /src/pg_listen /usr/local/bin/

ENTRYPOINT [ "pg_listen" ]
