#!/bin/bash
set -e
cd /home
sudo rm -r pi
sudo mkdir pi
sudo chown pi:pi pi
git clone --depth=1 --branch=master https://github.com/deguss/rpilogger pi

