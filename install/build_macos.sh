#!/bin/bash
GOOS=darwin GOARCH=amd64 go build -o inst_amd64
GOOS=darwin GOARCH=arm64 go build -o inst_arm64
lipo -create -output ouroveon_install_macos inst_amd64 inst_arm64
