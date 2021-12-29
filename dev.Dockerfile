FROM ubuntu:20.04

ENV TZ=Europe/Paris
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update \
    && \
    apt install -y vim \
                   bash-completion \
                   wget \
                   curl \
                   less \
                   tree \
                   screen \
                   clang \
                   pip \
                   gdb \
                   git \
                   cmake \
                   make \
                   gdb \
                   gcc-10 gcc-10-base gcc-10-doc g++-10 \
                   libstdc++-10-dev libstdc++-10-doc \
                   libboost1.71-all-dev \
                   libgtest-dev \
                   libssl-dev \
    && \
    apt clean

# install pygments for gdb syntax highlighting
RUN pip install pygments

# install dotfiles
WORKDIR /root/
RUN git clone https://github.com/skeggib/dotfiles.git && \
    rm -f .bashrc .inputrc .vimrc .gdbinit .gitconfig && \
    ln -s dotfiles/.bashrc .bashrc && \
    ln -s dotfiles/.inputrc .inputrc && \
    ln -s dotfiles/.vimrc .vimrc && \
    ln -s dotfiles/.gdbinit .gdbinit && \
    ln -s dotfiles/.gitconfig .gitconfig

RUN apt install -y ninja-build
