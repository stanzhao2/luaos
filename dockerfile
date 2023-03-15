FROM centos:latest
MAINTAINER stanzhao
ARG TARGET=/usr/local/luaos

RUN rm -f /etc/yum.repos.d/*
COPY centos8_base.repo /etc/yum.repos.d/CentOS-Base.repo
RUN yum clean all
RUN yum makecache

RUN yum install -y dmidecode
RUN yum install -y curl-devel
RUN yum install -y libuuid-devel
RUN yum install -y zlib-devel
RUN yum install -y openssl-devel

COPY bin     $TARGET/bin
COPY lib/lua $TARGET/lib/lua
COPY share   $TARGET/share
COPY doc     $TARGET/doc

RUN ln -s $TARGET/bin/luaos /usr/bin/luaos
CMD cd /home
CMD /bin/bash
