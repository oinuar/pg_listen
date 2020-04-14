FROM haamutech/cmake-llvm:latest

RUN apt-get update && apt-get install --no-install-recommends -y libpq-dev pkg-config && apt-get clean

WORKDIR /app

COPY . .

RUN make

ENTRYPOINT ["/app/pg_listen"]
