#

ifndef PD_ROOT
$(error "PD_ROOT is missing")
endif

NAME = $(notdir $(basename $(PWD)))
INSTALL_DIR = $(PD_LUA_LIB)

CC = gcc
LD = gcc
CCFLAGS = -c -fPIC -m64 -std=gnu99 -O2 -Wall -Werror
LDFLAGS = -shared
INCS = -I$(PD_INC)
LIBS = -L$(PD_LIB)

MAIN_T = $(NAME).so
MAIN_O = lua_$(NAME).o

build: $(MAIN_T)

clean:
	rm -f $(MAIN_T) $(MAIN_O)

install:
	cp $(MAIN_T) $(INSTALL_DIR)

$(MAIN_T): $(MAIN_O)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.c.o:
	$(CC) $(CCFLAGS) -o $@ $< $(INCS)

$(MAIN_O): ../lua_pd.h lua_$(NAME).h
