FROM debian:bookworm-slim AS base

ARG BUILD_VERSION="OSPI"

########################################
## 1st stage compiles OpenSprinkler code
FROM base AS os-build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y bash g++ make libmosquittopp-dev libssl-dev libi2c-dev
RUN rm -rf /var/lib/apt/lists/*
COPY . /OpenSprinkler
WORKDIR /OpenSprinkler
RUN make clean
RUN make VERSION=${BUILD_VERSION}

########################################
## 2nd stage is minimal runtime + executable
FROM base

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y libstdc++6 libmosquittopp1 libi2c0
RUN rm -rf /var/lib/apt/lists/* 
RUN mkdir /OpenSprinkler
RUN mkdir -p /data/logs

COPY --from=os-build /OpenSprinkler/OpenSprinkler /OpenSprinkler/OpenSprinkler
WORKDIR /OpenSprinkler

#-- Logs and config information go into the volume on /data
VOLUME /data

#-- OpenSprinkler interface is available on 8080
EXPOSE 8080

#-- By default, start OS using /data for saving data/NVM/log files
CMD [ "/OpenSprinkler/OpenSprinkler", "-d", "/data" ]
