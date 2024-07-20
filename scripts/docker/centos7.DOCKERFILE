FROM centos:centos7.8.2003
ADD scripts/dependencies /dependencies
RUN set -eux \
    && /dependencies/install.sh \
    rm -rf /dependencies
