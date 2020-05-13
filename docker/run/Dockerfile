FROM ubuntu:18.04

RUN    apt-get -q -y update && apt-get -q -y install apt-utils gnupg2 && apt-get -q -y install  libunwind-dev

RUN    apt-key adv --recv-key --keyserver keyserver.ubuntu.com C68C3704 \
    && echo "deb [arch=amd64] http://192.168.13.211/sniper_repo bionic sniper" | tee /etc/apt/sources.list.d/sniper-repo.list
RUN    apt-get -q -y update \
    && apt-get -q -y install libev-dev libconfig-dev libconfig++-dev libfmt-dev \
                             libgoogle-perftools-dev libspdlog-dev libsecp256k1 libopenssl libevent-2.1-6 liburiparser1 libcurl4-openssl-dev