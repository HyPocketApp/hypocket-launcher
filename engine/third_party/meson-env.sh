# Sourced by build scripts that use meson. Mesa needs meson >= 1.4; if the
# system one is older, install a pinned version in a virtualenv in the
# current directory.
# (awk exits with 1 when meson prints nothing, that is when it is missing.)
if ! meson --version 2>/dev/null | awk -F. '{ ok = $1 > 1 || ($1 == 1 && $2 >= 4) } END { exit !ok }'; then
    if [ ! -x venv/bin/meson ]; then
        python3 -m venv venv
        venv/bin/pip install -q 'meson==1.6.1' mako pyyaml packaging
    fi
    PATH="$PWD/venv/bin:$PATH"
    export PATH
fi
