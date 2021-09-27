FROM ubuntu:20.04
ARG MIRROR=archive.ubuntu.com
ARG ROOTPW=1234

# 22        ssh
# 5050      gdbserver   
EXPOSE 22 5050

# configure apt
RUN apt-get update &&\
    sed s@archive.ubuntu.com@${MIRROR}@g /etc/apt/sources.list -i &&\
    apt-get update
    
RUN echo ">> installing packages ...";\
    # install required packages
    DEBIAN_FRONTEND="noninteractive" apt-get install -y\
        iputils-ping net-tools wget curl\
        vim tmux git\
        openssh-server openssh-client\
        gcc g++ gdb gdbserver\
        build-essential cmake ninja-build
        
# configure ssh
RUN echo ">> configuring ssh ...";\
    echo "Port 22" >> /etc/ssh/sshd_config &&\
    echo "PermitRootLogin yes" >> /etc/ssh/sshd_config &&\
    echo "root:${ROOTPW}" | chpasswd
    
# install required packages for development
WORKDIR /tmp
RUN wget https://github.com/gabime/spdlog/archive/refs/tags/v1.9.2.tar.gz &&\
    tar -xzvf v1.9.2.tar.gz && cd spdlog-1.9.2 &&\
    mkdir -p build && cd build &&\
    cmake ..\
        -DCMAKE_INSTALL_PREFIX=/\
        -DCMAKE_BUILD_TYPE=Debug\
        && make install -j$(lscpu -b -p=CPU | grep -v '^#' | wc -l) &&\
    cmake ..\
        -DCMAKE_BUILD_TYPE=Release\
        && make install -j$(lscpu -b -p=CPU | grep -v '^#' | wc -l) 

# setup startup command
WORKDIR /root
RUN echo "#!/usr/bin/env bash" > .entry.sh &&\
    chmod +x .entry.sh &&\
    echo   "service ssh start &&\
            /bin/bash $@" > .entry.sh

# docker run -dt -p<SSH>:22 -p<GDBSERV>:5050 <IMG>
CMD [ "/bin/bash", "/root/.entry.sh" ]
