FROM debian:bookworm-slim AS base

ARG BUILD_VERSION="OSPI"

########################################
## 1st stage compiles OpenSprinkler runtime dependency raspi-gpio
FROM base AS raspi-gpio-build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update 
RUN apt-get install -y git gcc make automake 
RUN rm -rf /var/lib/apt/lists/*
RUN mkdir /raspi-gpio 
WORKDIR /raspi-gpio 
RUN git clone --depth 1 https://github.com/RPi-Distro/raspi-gpio.git . 
RUN autoreconf -f -i
RUN (./configure || cat config.log) 
RUN make

########################################
## 2nd stage compiles OpenSprinkler code
FROM base AS os-build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y bash g++ make libmosquittopp-dev libssl-dev 
RUN rm -rf /var/lib/apt/lists/*
COPY . /OpenSprinkler
WORKDIR /OpenSprinkler
RUN make clean
RUN make VERSION=${BUILD_VERSION}

########################################
## 3rd stage is minimal runtime + executable
FROM base

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y libstdc++6 libmosquittopp1 
RUN rm -rf /var/lib/apt/lists/* 
RUN mkdir /OpenSprinkler
RUN mkdir -p /data/logs

COPY --from=os-build /OpenSprinkler/OpenSprinkler /OpenSprinkler/OpenSprinkler
COPY --from=raspi-gpio-build /raspi-gpio/raspi-gpio /usr/bin/raspi-gpio
WORKDIR /OpenSprinkler

#-- Logs and config information go into the volume on /data
VOLUME /data

#-- OpenSprinkler interface is available on 8080
EXPOSE 8080

#-- By default, start OS using /data for saving data/NVM/log files
CMD [ "/OpenSprinkler/OpenSprinkler", "-d", "/data" ]
