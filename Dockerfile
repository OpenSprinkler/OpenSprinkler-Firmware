FROM debian:bookworm-slim as base

########################################
## 1st stage compiles OpenSprinkler runtime dependency raspi-gpio
FROM base as raspi-gpio-build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y git gcc make automake && rm -rf /var/lib/apt/lists/*
RUN mkdir /raspi-gpio && cd /raspi-gpio && git clone --depth 1 https://github.com/RPi-Distro/raspi-gpio.git . && autoreconf -f -i && (./configure || cat config.log) && make

########################################
## 2nd stage compiles OpenSprinkler code
FROM base as os-build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y bash g++ make libmosquittopp-dev && rm -rf /var/lib/apt/lists/*
COPY . /OpenSprinkler
RUN cd /OpenSprinkler && make

########################################
## 3rd stage is minimal runtime + executable
FROM base

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y libstdc++6 libmosquittopp1 && rm -rf /var/lib/apt/lists/* \
    && \
    mkdir /OpenSprinkler && \
    mkdir -p /data/logs

COPY --from=os-build /OpenSprinkler/OpenSprinkler /OpenSprinkler/OpenSprinkler
COPY --from=raspi-gpio-build /raspi-gpio/raspi-gpio /usr/bin/raspi-gpio
WORKDIR /OpenSprinkler

#-- Logs and config information go into the volume on /data
VOLUME /data

#-- OpenSprinkler interface is available on 8080
EXPOSE 8080

#-- By default, start OS using /data for saving data/NVM/log files
CMD [ "/OpenSprinkler/OpenSprinkler", "-d", "/data" ]
