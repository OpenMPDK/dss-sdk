FROM rockylinux:8-minimal

ADD scripts/dependencies /dependencies
RUN set -eux \
    && DOCKER=true /dependencies/install.sh \
    && rm -rf /dependencies
