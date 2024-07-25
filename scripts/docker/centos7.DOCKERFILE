FROM centos:centos7.8.2003
ADD scripts/dependencies /dependencies
RUN set -eux \
    && sed -i s/mirror.centos.org/vault.centos.org/g /etc/yum.repos.d/*.repo \
    && sed -i s/^#.*baseurl=http/baseurl=http/g /etc/yum.repos.d/*.repo \
    && sed -i s/^mirrorlist=http/#mirrorlist=http/g /etc/yum.repos.d/*.repo \
    && DOCKER=true /dependencies/install.sh \
    && rm -rf /dependencies
