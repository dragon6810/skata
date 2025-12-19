CC = clang
LDFLAGS = -fsanitize=address
CFLAGS = -MMD -Wall -Werror -fsanitize=address -O0 -g

BIN_DIR = bin
OBJ_DIR = obj
SRC_DIR = src

CFLAGS += -I$(SRC_DIR)

FRONT_DIR = front
MIDDLE_DIR = middle
BACK_DIR = back

FRONT_C_DIR = c
BACK_AARCH64_DIR = aarch64

LANG = c
PLATFORM = aarch64

SRC_FILE = $(wildcard $(SRC_DIR)/*.c)
SRC_FILE += $(wildcard $(SRC_DIR)/$(FRONT_DIR)/*.c)
SRC_FILE += $(wildcard $(SRC_DIR)/$(MIDDLE_DIR)/*.c)
SRC_FILE += $(wildcard $(SRC_DIR)/$(BACK_DIR)/*.c)
OBJ_FILE = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(SRC_FILE)))
DEP_FILE = $(OBJ_FILE:.o=.d)
BIN_FILE = $(BIN_DIR)/skata

ifeq ($(LANG), c)
	SRC_FILE += $(wildcard $(SRC_DIR)/$(FRONT_DIR)/$(FRONT_C_DIR)/*.c)
endif

ifeq ($(PLATFORM), aarch64)
	SRC_FILE += $(wildcard $(SRC_DIR)/$(BACK_DIR)/$(BACK_AARCH64_DIR)/*.c)
endif

.PHONY: all clean mkdirs

all: $(BIN_FILE)

clean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)

$(BIN_FILE): $(OBJ_FILE)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) $(OBJ_FILE) -o $(BIN_FILE)

$(OBJ_DIR)/$(SRC_DIR)/%.c.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP_FILE)