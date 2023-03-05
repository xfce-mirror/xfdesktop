#!/bin/sh
#
# Copyright (c) 2002-2022
#         The Xfce development team. All rights reserved.
#
# Written for Xfce by Benedikt Meurer <benny@xfce.org>
#                 and Brian Tarricone <brian@tarricone.org>.
#

export XDT_AUTOGEN_REQUIRED_VERSION="4.19.0"

(type xdt-autogen) >/dev/null 2>&1 || {
  cat >&2 <<EOF
autogen.sh: You don't seem to have the Xfce development tools installed on your system,
            which are required to build this software.
            Please install the xfce4-dev-tools package first; it is available
            from your distribution or https://www.xfce.org/.
EOF
  exit 1
}

exec xdt-autogen "$@"

# vi:set ts=2 sw=2 et ai:
