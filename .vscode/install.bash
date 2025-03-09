#!/usr/bin/bash

cmakeCmd="${1:?'Missing cmake path.'}"
workingDir="${2:?'Missing working directory'}"

mkdir -p "${workingDir}/build_install"
${cmakeCmd} -DCMAKE_BUILD_TYPE=Release "-S${workingDir}" "-B${workingDir}/build_install"
${cmakeCmd} --build "${workingDir}/build_install"
pkexec "${cmakeCmd}" --install "${workingDir}/build_install" --config Release