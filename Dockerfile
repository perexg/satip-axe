FROM jalle19/centos7-stlinux24

WORKDIR /build

RUN yum -y update && \
  yum -y install epel-release && \
  yum -y install wget make gcc git python wget tar \
                 bzip2 uboot-tools patch fakeroot \
                 svn file autoconf automake libtool

# Required for git operations when container is running as an UID that doesn't exist in /etc/passwd
ENV GIT_COMMITTER_NAME=nobody
ENV GIT_COMMITTER_EMAIL=nobody@example.com

ENTRYPOINT [ "make" ]
