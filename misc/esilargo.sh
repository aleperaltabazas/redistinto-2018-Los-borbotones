#/bin/bash

cd ../ESI/Debug

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/utnso/tp-2018-1c-Los-borbotones/shared-library/Debug

valgrind ./ESI ESI_Largo