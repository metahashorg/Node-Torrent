FROM ubuntu:14.04

COPY sniper-repo_1.0.0-sniper1_amd64.deb sniper-repo_1.0.0-sniper1_amd64.deb

RUN apt-get update && apt-get -f install && apt-get -q -y install unattended-upgrades

RUN dpkg -i ./sniper-repo_1.0.0-sniper1_amd64.deb && apt-get update

RUN apt-get -q -y install cmake libev-dev liburiparser-dev libboost-all-dev librtmp-dev libgnutls-dev libgoogle-perftools-dev libsecp256k1 libopenssl libcurl4-openssl-dev libgmp-dev libgcrypt11-dev

RUN apt-get -q -y install wget make git autoconf libtool texinfo

RUN apt-get -q -y install gcc-7

RUN update-alternatives --quiet --remove-all gcc \
    ; update-alternatives --quiet --remove-all g++ \
    ; update-alternatives --quiet --remove-all cc \
    ; update-alternatives --quiet --remove-all cpp \
    ; update-alternatives --quiet --install /usr/bin/gcc gcc /usr/local/bin/gcc-7.2 20 \
    ; update-alternatives --quiet --install /usr/bin/cc cc /usr/local/bin/gcc-7.2 20 \
    ; update-alternatives --quiet --install /usr/bin/g++ g++ /usr/local/bin/g++-7.2 20 \
    ; update-alternatives --quiet --install /usr/bin/cpp cpp /usr/local/bin/g++-7.2 20 \
    ; update-alternatives --quiet --config gcc \
    ; update-alternatives --quiet --config cc \
    ; update-alternatives --quiet --config g++ \
    ; update-alternatives --quiet --config cpp

RUN rm -f /usr/lib/x86_64-linux-gnu/libstdc++.so.6 && ln -s /usr/local/lib64/libstdc++.so.6 /usr/lib/x86_64-linux-gnu/libstdc++.so.6

RUN cd /tmp && \
wget https://github.com/libevent/libevent/releases/download/release-2.1.8-stable/libevent-2.1.8-stable.tar.gz && \
tar zxfv libevent-2.1.8-stable.tar.gz && \
cd libevent-2.1.8-stable && \
./configure && \
make -j$(nproc) && \
make install

RUN cd /tmp && \
git clone https://github.com/metahashorg/libmicrohttpd2 && \
cd libmicrohttpd2 && \
./bootstrap && \
./configure && \
make -j$(nproc) && \
make install

RUN cd /tmp && \
git clone https://github.com/metahashorg/libmhsupport && \
cd libmhsupport/build && \
./build.sh && \
make install


