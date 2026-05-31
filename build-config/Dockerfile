# Chromium Android Builder
# Based on official Chromium build instructions
# https://chromium.googlesource.com/chromium/src/+/main/docs/android_build_instructions.md

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="/depot_tools:${PATH}"

# Install base tooling required by Chromium's build system
RUN apt-get update && \
    apt-get install -y \
        curl \
        git \
        lsb-release \
        python3 \
        python3-distutils \
        file \
        sudo \
        vim \
        apt-transport-https \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Mark all directories as safe Git directories (needed for bind mounts)
RUN git config --global --add safe.directory '*'

# Set working directory to Chromium source root
WORKDIR /checkout/src
