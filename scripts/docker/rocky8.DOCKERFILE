FROM rockylinux:8-minimal

ADD scripts/dependencies /dependencies
RUN set -eux \
    && /dependencies/install.sh \
    rm -rf /dependencies
