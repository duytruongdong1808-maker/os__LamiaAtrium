INC = -Iinclude
LIB = -lpthread

SRC = src
OBJ = obj
INCLUDE = include

CC = gcc
DEBUG = -g

#CFLAGS = -Wall -c $(DEBUG) -pthread -DMLQ_SCHED
CFLAGS = -Wall -c $(DEBUG) -pthread -DMLQ_SCHED -DMM_PAGING

LFLAGS = -Wall $(DEBUG)

vpath %.c $(SRC)
vpath %.h $(INCLUDE)

MAKE = $(CC) $(INC) 

MEM_OBJ = $(addprefix $(OBJ)/, paging.o mem.o cpu.o loader.o)
SYSCALL_OBJ = $(addprefix $(OBJ)/, syscall.o  sys_mem.o sys_listsyscall.o)
OS_OBJ = $(addprefix $(OBJ)/, cpu.o mem.o loader.o queue.o os.o sched.o timer.o mm-vm.o mm64.o mm.o mm-memphy.o libstd.o libmem.o)
OS_OBJ += $(SYSCALL_OBJ)
SCHED_OBJ = $(addprefix $(OBJ)/, cpu.o loader.o queue.o sched.o timer.o mem.o libstd.o libmem.o)
HEADER = $(wildcard $(INCLUDE)/*.h)
 
all: os

mem: $(MEM_OBJ)
	$(MAKE) $(LFLAGS) $(MEM_OBJ) -o mem $(LIB)

sched: $(SCHED_OBJ)
	$(MAKE) $(LFLAGS) $(SCHED_OBJ) -o sched $(LIB)

syscalltbl.lst: $(SRC)/syscall.tbl
	@echo $(OS_OBJ)
	chmod +x $(SRC)/syscalltbl.sh
	$(SRC)/syscalltbl.sh $< $(SRC)/$@ 

os: $(OBJ) syscalltbl.lst $(OS_OBJ)
	$(MAKE) $(LFLAGS) $(OS_OBJ) -o os $(LIB)

$(OBJ)/%.o: $(SRC)/%.c $(HEADER)
	$(MAKE) $(CFLAGS) -o $@ $<

$(OBJ):
	mkdir -p $(OBJ)

clean:
	rm -f $(SRC)/*.lst
	rm -f $(OBJ)/*.o os sched mem pdg
	rm -rf $(OBJ)

