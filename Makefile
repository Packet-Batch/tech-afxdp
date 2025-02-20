CC = clang

BUILD_DIR = build
BUILD_TESTS_DIR = $(BUILD_DIR)/tests

SRC_DIR = src
TESTS_DIR = tests
INC_DIR = include

XDP_TOOLS_DIR = modules/xdp-tools

AFXDP_OBJ = afxdp.o
SHARED_OBJ = libpbafxdp.so

INCS = -I $(INC_DIR) -I $(XDP_TOOLS_DIR)/headers -I /usr/include -I $(SRC_DIR)/common
LDFLAGS = -lelf -lz

PUB_LIB_DIR = /usr/lib
PUB_INC_DIR = /usr/include/pb/afxdp

all: afxdp libpbafxdp

afxdp:
	$(CC) $(INCS) -fPIC -O2 -g -c -o $(BUILD_DIR)/$(AFXDP_OBJ) $(SRC_DIR)/afxdp.c

libpbafxdp: afxdp
	$(CC) $(INCS) $(LDFLAGS) -fPIC -O2 -g -shared -o $(BUILD_DIR)/$(SHARED_OBJ) $(BUILD_DIR)/$(AFXDP_OBJ) $(SRC_DIR)/main.c

tests:
	$(CC) $(INCS) $(LDFLAGS) -lxdp -lbpf -lpbafxdp -O2 -g -o $(BUILD_TESTS_DIR)/tcp_syn_test $(TESTS_DIR)/tcp_syn_test.c

install:
	mkdir -p $(PUB_INC_DIR)

	cp -f $(BUILD_DIR)/$(SHARED_OBJ) $(PUB_LIB_DIR)
	cp -f $(INC_DIR)/api.h $(PUB_INC_DIR)

clean:
	find $(BUILD_DIR) -type f ! -name ".*" -exec rm -f {} +
	find $(BUILD_TESTS_DIR) -type f ! -name ".*" -exec rm -f {} +

.PHONY: tests