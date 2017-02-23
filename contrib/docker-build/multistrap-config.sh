#!/bin/sh

set -e

export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true
export LC_ALL=C LANGUAGE=C LANG=C
/var/lib/dpkg/info/dash.preinst install

echo en_US UTF-8 > /etc/locale.gen
locale-gen
echo export LANG=en_US.UTF-8 > /etc/profile.d/utf8.sh

dpkg --configure -a

echo nameserver 8.8.8.8 > /etc/resolv.conf

(
    cd /root
    git clone https://github.com/mawww/kakoune.git
    cat >kakoune/run.sh <<"EOF"
#!/bin/sh

set -e

cd "$(dirname "$0")/src"
git pull
CXX="{{compiler}}" debug="${1:-no}" make clean all test
EOF

    chmod +x kakoune/run.sh
)
