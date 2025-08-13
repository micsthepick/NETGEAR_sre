#!/bin/bash
# note for Ubuntu: dash does NOT work
must() {
  "$@"
  local status=$?
  if [ $status -ne 0 ]; then
    echo "Fatal: command failed with status $status: $*"
    exit $status
  fi
}