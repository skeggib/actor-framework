FROM debian:9

RUN apt update -y \
    && apt upgrade -y \
    && apt install -y \
         g++ \
         cmake \
         git \
         libssl-dev \
         make \
    && apt autoclean
