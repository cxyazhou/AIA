#! /bin/bash
ROOT_DIR=`cd ../../../../;pwd`
echo "ROOT_DIR=${ROOT_DIR}" >./config.make
echo "PLATFORM=rpi" >>./config.make
echo "CPP   =   g++" >>./config.make
echo "CC    =   gcc" >>./config.make
echo "LD    =   ld" >>./config.make
echo "AR    =   ar" >>./config.make
echo "STRIP =   strip" >>./config.make
echo "RM    =   rm" >>./config.make
echo "RANLIB=   ranlib" >>./config.make
echo "NM    =   nm" >>./config.make
echo "RC    =   windres" >>./config.make