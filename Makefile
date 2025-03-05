CC = clang

STATIC ?= 1

BUILD_DIR = build
BUILD_TESTS_DIR = $(BUILD_DIR)/tests

SRC_DIR = src
TESTS_DIR = tests
INC_DIR = include

PUB_LIB_DIR = /usr/lib
PUB_INC_DIR = /usr/include/pb/afxdp

# LibXDP and LibBPF directories.
XDP_TOOLS_DIR = modules/xdp-tools

LIBXDP_DIR = $(XDP_TOOLS_DIR)/lib/libxdp

# LibXDP shared object.
LIBXDP_OBJ = $(LIBXDP_DIR)/libxdp.so

# Main library.
AFXDP_OBJ = afxdp.o
SHARED_OBJ = libpbafxdp.so

# Includes.
INCS = -I $(INC_DIR)  -I /usr/include -I $(SRC_DIR)/common
LD_FLAGS = -lelf -lz

ifeq ($(STATIC), 1)
	OBJS = $(LIBXDP_OBJ)
	FLAGS = -D__STATIC__
else
	INCS += -I $(XDP_TOOLS_DIR)/headers
	LD_FLAGS += -lbpf -lxdp
endif

all: libpbafxdp

afxdp:
	$(CC) $(INCS) -fPIC -O2 -g -c -o $(BUILD_DIR)/$(AFXDP_OBJ) $(SRC_DIR)/afxdp.c

libpbafxdp: afxdp
	$(CC) $(INCS) $(LD_FLAGS) $(FLAGS) -fPIC -O2 -g -shared -o $(BUILD_DIR)/$(SHARED_OBJ) $(BUILD_DIR)/$(AFXDP_OBJ) $(OBJS) $(SRC_DIR)/main.c

tests:
	$(CC) $(INCS) $(LD_FLAGS) -lpbafxdp -O2 -g -o $(BUILD_TESTS_DIR)/tcp_syn_test $(TESTS_DIR)/tcp_syn_test.c

install:
	mkdir -p $(PUB_INC_DIR)

	cp -f $(BUILD_DIR)/$(SHARED_OBJ) $(PUB_LIB_DIR)
	cp -f $(INC_DIR)/api.h $(PUB_INC_DIR)

clean:
	find $(BUILD_DIR) -type f ! -name ".*" -exec rm -f {} +
	find $(BUILD_TESTS_DIR) -type f ! -name ".*" -exec rm -f {} +

# LibXDP chain. We need to install objects here since our program relies on installed object files and such.
libxdp:
	$(MAKE) -C $(XDP_TOOLS_DIR) libxdp

libxdp_install:
	$(MAKE) -C $(LIBBPF_SRC) install
	$(MAKE) -C $(LIBXDP_DIR) install

libxdp_clean:
	$(MAKE) -C $(XDP_TOOLS_DIR) clean
	$(MAKE) -C $(LIBBPF_SRC) clean

.PHONY: tests