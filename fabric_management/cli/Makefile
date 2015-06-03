CC = $(CROSS_COMPILE)gcc
CCFLAGS = -c -g -ggdb -fPIC -Wall -Werror -I. -I.. \
	-I../librio_switch/inc \
	-I../libriocp_pe/inc \
	-I../librio_maint/inc \
	-I../Linux_Include \
	-O2 -I$(LINUX_INCLUDE_DIR) -rdynamic -DIDT_TSI721_SUPPORT

LKFLAGS= -shared -fPIC -g -ggdb

CLI_DEPS= cli_base_init.h cli_cmd_db.h cli_cmd_line.h cli_parse.h \
	dev_db_cli_cmds.h dev_db_rw_cmds.h dev_db_sm.h

CLI_OBJS= cli_base_init.o cli_cmd_db.o cli_cmd_line.o cli_parse.o \
	dev_db_cli_cmds.o dev_db_rw_cmds.o dev_db_sm.o

%.o: %.c $(CLI_DEPS)
	$(CC) -o $@ $< -c $(CCFLAGS)

clidb: $(CLI_OBJS)
	$(CC) -o libclidb.so $^  -rdynamic $(LKFLAGS)

all: clidb

clean:
	rm -f *.o *~ *.o *.so *.exe core 
