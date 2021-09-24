FROM jalle19/centos7-stlinux24

WORKDIR /build

RUN yum -y update && \
  yum -y install epel-release && \
  yum -y install wget make gcc git python wget tar \
                 bzip2 uboot-tools patch fakeroot \
                 svn file autoconf automake libtool

ENTRYPOINT [ "make" ]
