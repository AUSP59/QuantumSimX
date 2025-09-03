
# Reproducible multi-stage build
FROM debian:stable-slim AS build
ARG DEBIAN_FRONTEND=noninteractive
ARG SOURCE_DATE_EPOCH=1700000000
RUN apt-get update && apt-get install -y --no-install-recommends     build-essential cmake ninja-build git && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release &&     cmake --build build -j &&     cmake --install build --prefix /opt/quantum-simx
FROM debian:stable-slim
COPY --from=build /opt/quantum-simx /usr/local
ENTRYPOINT ["quantum-simx"]
