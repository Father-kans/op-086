#!/usr/bin/bash

export PASSIVE="0"
sed -i -e 's/\r$//' /data/openpilot/launch_chffrplus.sh
exec ./launch_chffrplus.sh

