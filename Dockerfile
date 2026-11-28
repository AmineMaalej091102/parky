FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's|http://archive.ubuntu.com/ubuntu|http://archive.ubuntu.com/ubuntu|g' /etc/apt/sources.list \
    && sed -i 's|http://security.ubuntu.com/ubuntu|http://archive.ubuntu.com/ubuntu|g' /etc/apt/sources.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        gcc \
        cppcheck \
        python3 \
        libgtk2.0-dev \
        libxml2-dev \
        libxml-parser-perl \
        scrollkeeper \
        file \
        pkg-config \
        gettext \
        wget \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN wget -q https://download.gnome.org/sources/glade/2.12/glade-2.12.2.tar.gz -O /tmp/glade-2.12.2.tar.gz \
    && tar -xzf /tmp/glade-2.12.2.tar.gz -C /tmp \
    && cd /tmp/glade-2.12.2 \
    && ./configure \
    && make -j"$(nproc)" \
    && make install \
    && ldconfig \
    && rm -rf /tmp/glade-2.12.2 /tmp/glade-2.12.2.tar.gz

ENV DISPLAY=:0

WORKDIR /app

CMD ["/bin/bash"]
