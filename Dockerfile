FROM debian:bookworm-slim AS base

# Set up prerequisites for adding the RPi repository
# We need ca-certificates, curl/gnupg to add the key, and lsb-release to get the Debian version name (e.g., "bookworm")
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    gnupg \
    lsb-release \
 && rm -rf /var/lib/apt/lists/*

# Add the Raspberry Pi repository GPG key
RUN curl -fsSL "https://archive.raspberrypi.org/debian/raspberrypi.gpg.key" | gpg --dearmor -o /usr/share/keyrings/raspberrypi-archive-keyring.gpg

# Add the Raspberry Pi repository to sources
RUN echo "deb [signed-by=/usr/share/keyrings/raspberrypi-archive-keyring.gpg] https://archive.raspberrypi.org/debian/ $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/raspberrypi.list > /dev/null

########################################
## 1st stage compiles OpenSprinkler code
FROM base AS os-build

ARG BUILD_VERSION="OSPI"

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y bash g++ make libmosquittopp-dev libssl-dev libi2c-dev liblgpio-dev
RUN rm -rf /var/lib/apt/lists/*
COPY . /OpenSprinkler
WORKDIR /OpenSprinkler
RUN make clean
RUN make VERSION=${BUILD_VERSION}

########################################
## 2nd stage is minimal runtime + executable
FROM base

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    libmosquittopp1 \
    libi2c0 \
    liblgpio-dev \
 && rm -rf /var/lib/apt/lists/*
RUN mkdir /OpenSprinkler
RUN mkdir -p /data/logs

COPY --from=os-build /OpenSprinkler/OpenSprinkler /OpenSprinkler/OpenSprinkler
WORKDIR /OpenSprinkler

#-- Logs and config information go into the volume on /data
VOLUME /data

#-- OpenSprinkler interface is available on 88 (the OSPi default HTTP port)
EXPOSE 88

#-- By default, start OS using /data for saving data/NVM/log files
CMD [ "/OpenSprinkler/OpenSprinkler", "-d", "/data" ]

