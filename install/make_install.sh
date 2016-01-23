#!/bin/bash
cd $1
make -s clean
make -s all
cd utils/goodput
make -s clean
make -s all
doxygen doxyconfig
cd ../file_transfer
make -s clean
make -s all
cd ../..
doxygen doxyconfig
git status
cd ..
chgrp -R $2 rapidio_sw
