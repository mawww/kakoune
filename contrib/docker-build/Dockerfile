##
## Dockerfile by lenormf
##
## Usage: docker build -t $USER/kakoune-build .
## Once the image has been created: docker run --privileged $USER/kakoune-build [compiler]...
## Privileges are needed in order to forward the host's /proc to the underlying chroot
## By default the container will update to the latest HEAD of the upstream repository,
## then start building in both release and debug mode the project with the following compilers:
## clang-3.4, clang-3.5, clang-3.8, g++-5, g++-6
## After the project has been sucessfully compiled, the test suite is run
##

FROM debian:stretch
MAINTAINER Frank LENORMAND <lenormf@gmail.com>

ENV LANG=en_US.UTF-8

RUN \
    apt-get update \
    && apt-get install -y sudo multistrap

RUN \
    useradd -m build \
    && echo build:build | chpasswd \
    && echo '%build ALL=NOPASSWD:ALL' > /etc/sudoers.d/build \
    && chmod 0440 /etc/sudoers.d/build

COPY multistrap-* /root/multistrap/
COPY entrypoint.sh /entrypoint.sh

RUN \
    chmod 755 /entrypoint.sh \
    && chmod -R 755 /root/

WORKDIR /home/build/
USER build

# clang-3.4, minimum supported version
RUN \
    rm -rf hooks conf \
    && mkdir hooks conf \
    && sed \
        -e 's/{{suite}}/jessie/g' \
        -e 's/{{pkg-compiler}}/clang-3.4/g' \
        /root/multistrap/multistrap-compiler.conf \
        > conf/clang-3.4.conf \
    && cp /root/multistrap/multistrap-hook-dev_random.sh hooks/download-dev_urandom.sh \
    && chmod 755 hooks/* \
    && sudo multistrap -d clang-3.4 -f conf/clang-3.4.conf \
    && sed \
        -e 's/{{compiler}}/clang++-3.4/g' \
        /root/multistrap/multistrap-config.sh \
        | sudo tee clang-3.4/root/config.sh \
    && sudo chmod 755 clang-3.4/root/config.sh \
    && sudo chroot clang-3.4 /root/config.sh

# clang-3.5, provided by the `clang` package on Debian:jessie
RUN \
    rm -rf hooks conf \
    && mkdir hooks conf \
    && sed \
        -e 's/{{suite}}/jessie/g' \
        -e 's/{{pkg-compiler}}/clang-3.5/g' \
        /root/multistrap/multistrap-compiler.conf \
        > conf/clang-3.5.conf \
    && cp /root/multistrap/multistrap-hook-dev_random.sh hooks/download-dev_urandom.sh \
    && chmod 755 hooks/* \
    && sudo multistrap -d clang-3.5 -f conf/clang-3.5.conf \
    && sed \
        -e 's/{{compiler}}/clang++-3.5/g' \
        /root/multistrap/multistrap-config.sh \
        | sudo tee clang-3.5/root/config.sh \
    && sudo chmod 755 clang-3.5/root/config.sh \
    && sudo chroot clang-3.5 /root/config.sh

# clang-3.8, provided by the `clang` package on Debian:stretch
RUN \
    rm -rf hooks conf \
    && mkdir hooks conf \
    && sed \
        -e 's/{{suite}}/stretch/g' \
        -e 's/{{pkg-compiler}}/clang-3.8/g' \
        /root/multistrap/multistrap-compiler.conf \
        > conf/clang-3.8.conf \
    && cp /root/multistrap/multistrap-hook-dev_random.sh hooks/download-dev_urandom.sh \
    && chmod 755 hooks/* \
    && sudo multistrap -d clang-3.8 -f conf/clang-3.8.conf \
    && sed \
        -e 's/{{compiler}}/clang++-3.8/g' \
        /root/multistrap/multistrap-config.sh \
        | sudo tee clang-3.8/root/config.sh \
    && sudo chmod 755 clang-3.8/root/config.sh \
    && sudo chroot clang-3.8 /root/config.sh

# g++-5, provided by the `c++-compiler` package on Debian:stretch
RUN \
    rm -rf hooks conf \
    && mkdir hooks conf \
    && sed \
        -e 's/{{suite}}/sid/g' \
        -e 's/{{pkg-compiler}}/g++-5/g' \
        /root/multistrap/multistrap-compiler.conf \
        > conf/g++-5.conf \
    && cp /root/multistrap/multistrap-hook-dev_random.sh hooks/download-dev_urandom.sh \
    && chmod 755 hooks/* \
    && sudo multistrap -d g++-5 -f conf/g++-5.conf \
    && sed \
        -e 's/{{compiler}}/g++-5/g' \
        /root/multistrap/multistrap-config.sh \
        | sudo tee g++-5/root/config.sh \
    && sudo chmod 755 g++-5/root/config.sh \
    && sudo chroot g++-5 /root/config.sh

# g++-6, provided by the `c++-compiler` package on Debian:stretch
RUN \
    rm -rf hooks conf \
    && mkdir hooks conf \
    && sed \
        -e 's/{{suite}}/stretch/g' \
        -e 's/{{pkg-compiler}}/g++-6/g' \
        /root/multistrap/multistrap-compiler.conf \
        > conf/g++-6.conf \
    && cp /root/multistrap/multistrap-hook-dev_random.sh hooks/download-dev_urandom.sh \
    && chmod 755 hooks/* \
    && sudo multistrap -d g++-6 -f conf/g++-6.conf \
    && sed \
        -e 's/{{compiler}}/g++-6/g' \
        /root/multistrap/multistrap-config.sh \
        | sudo tee g++-6/root/config.sh \
    && sudo chmod 755 g++-6/root/config.sh \
    && sudo chroot g++-6 /root/config.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["clang-3.4", "clang-3.5", "clang-3.8", "g++-5", "g++-6"]
