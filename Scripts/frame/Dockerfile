FROM --platform=linux/arm64 ubuntu:22.04@sha256:b1066385161d28ddf6bc7e7b28a9170eec11484c821d1a5150d176cbde41d7f7

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      binutils=2.38-4ubuntu2.12 \
      build-essential=12.9ubuntu3 \
      gcc-11=11.4.0-1ubuntu1~22.04.3 \
      g++-11=11.4.0-1ubuntu1~22.04.3 \
      git=1:2.34.1-1ubuntu1.18 \
      libgl-dev=1.4.0-1 \
      libglew-dev=2.2.0-4 \
      libglu1-mesa-dev=9.0.2-1 \
      libsdl2-dev=2.0.20+dfsg-2ubuntu1.22.04.1 \
      libsdl2-net-dev=2.0.1+dfsg1-7 \
      libopenxr-dev=1.0.20~dfsg1-1build1 \
      libopenxr-loader1=1.0.20~dfsg1-1build1 \
      libjsoncpp25=1.9.5-3 \
      libx11-dev=2:1.7.5-1ubuntu0.4 \
      make=4.3-4.1build1 \
      mesa-utils=8.4.0-1ubuntu1 \
      python3=3.10.6-1~22.04.1 \
      zlib1g-dev=1:1.2.11.dfsg-2ubuntu9.2 \
      xvfb=2:21.1.4-2ubuntu1.7~22.04.16 \
      xauth=1:1.1-1build2 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
