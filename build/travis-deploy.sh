#!/bin/bash

rsync $TRAVIS_BUILD_DIR/dist/*.dmg cpow@162.243.126.83:/var/www/html/nesicide/
rsync $TRAVIS_BUILD_DIR/dist/*.AppImage cpow@162.243.126.83:/var/www/html/nesicide/

