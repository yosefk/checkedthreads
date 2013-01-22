export CT_VALGRIND_SRC_DIR?=# write your valgrind source distribution root here
export VALGRIND_LIB?=/usr/lib/valgrind#you can change it to your library directory
export CT_VALGRIND_CP?=sudo cp#or plain cp if $(CT_VALGRIND_LIB_DIR) is writeable for you
