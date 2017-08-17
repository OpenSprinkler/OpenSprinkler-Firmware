FROM resin/armhf-alpine:3.6 as base



########################################
## 1st stage builds OS for RPi
FROM base as build

RUN [ "cross-build-start" ]

RUN apk --no-cache add \
      bash \
      g++
COPY . /OpenSprinkler
RUN cd /OpenSprinkler && \
    ./build.sh -s ospi

RUN [ "cross-build-end" ]



########################################
## 2nd stage is minimal runtime + executable
FROM base

RUN [ "cross-build-start" ]

RUN apk --no-cache add \
      libstdc++ \
    && \
    mkdir /OpenSprinkler && \
    mkdir -p /data/logs

COPY --from=build /OpenSprinkler/OpenSprinkler /OpenSprinkler/OpenSprinkler
WORKDIR /OpenSprinkler

#-- Logs and config information go into the volume on /data
VOLUME /data

#-- OpenSprinkler interface is available on 8080
EXPOSE 8080

#-- By default, start OS using /data for saving data/NVM/log files
CMD [ "./OpenSprinkler", "-d", "/data" ]

RUN [ "cross-build-end" ]
