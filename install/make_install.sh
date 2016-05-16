#!/bin/bash
cd $1
make -s clean
make -s all
doxygen doxyconfig
/bin/ldconfig $1/common/libs_so
git status
cd ..
chgrp -R $2 rapidio_sw
