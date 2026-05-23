CC ?= gcc
BIN ?= compiler

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=obj/%.o)
HDR := $(wildcard src/*.h)

CFLAGS += -std=gnu11
CFLAGS += -Wshadow -Wall -Wextra -Werror
CFLAGS += -fsanitize=undefined
CFLAGS += -Wswitch-default
CFLAGS += -Wredundant-decls
CFLAGS += -Wno-unused-parameter -Wno-unused-variable
CFLAGS += -Wformat-security
CFLAGS += -Wunreachable-code
CFLAGS += -O3
CFLAGS += -Wno-maybe-uninitialized
LDLIBS += -lm

# Optional build switches:
#   make debug=all
#   make debug=sym
#   make debug=tokenizer
#   make debug=parser
#   make debug=resolver
#   make debug=typechecker
#   make debug=labeler
#   make debug=ir
#   make debug=ir_opt
#   make debug=codegen
#   make debug=regalloc
#   make debug=emitter
# Multiple debug selectors can be combined, e.g.:
#   make debug="tokenizer,parser,regalloc"

ifeq ($(debug),all)
	CFLAGS += -DDEBUG_TOKENIZER
	CFLAGS += -DDEBUG_PARSER
	CFLAGS += -DDEBUG_RESOLVER
	CFLAGS += -DDEBUG_TYPECHECKER
	CFLAGS += -DDEBUG_LABELER
	CFLAGS += -DDEBUG_IR
	CFLAGS += -DDEBUG_IR_OPT
	CFLAGS += -DDEBUG_CODEGEN
	CFLAGS += -DDEBUG_REGALLOC
	CFLAGS += -DDEBUG_EMITTER
	CFLAGS += -DDEBUG_INTERVALS
	CFLAGS += -DDEBUG_SPILL_COSTS
	CFLAGS += -DDEBUG_INTERFERENCE
	CFLAGS += -DDEBUG_MOVES
	CFLAGS += -DDEBUG_BRIGGS
	CFLAGS += -DDEBUG_HOMES
	CFLAGS += -g3
endif

ifeq (sym,$(findstring sym,$(debug)))
	CFLAGS += -g3
endif

ifeq (tokenizer,$(findstring tokenizer,$(debug)))
	CFLAGS += -DDEBUG_TOKENIZER
endif

ifeq (parser,$(findstring parser,$(debug)))
	CFLAGS += -DDEBUG_PARSER
endif

ifeq (resolver,$(findstring resolver,$(debug)))
	CFLAGS += -DDEBUG_RESOLVER
endif

ifeq (typechecker,$(findstring typechecker,$(debug)))
	CFLAGS += -DDEBUG_TYPECHECKER
endif

ifeq (labeler,$(findstring labeler,$(debug)))
	CFLAGS += -DDEBUG_LABELER
endif

ifeq (ir_opt,$(findstring ir_opt,$(debug)))
	CFLAGS += -DDEBUG_IR_OPT
endif

ifeq (ir,$(findstring ir,$(debug)))
	CFLAGS += -DDEBUG_IR
endif

ifeq (codegen,$(findstring codegen,$(debug)))
	CFLAGS += -DDEBUG_CODEGEN
endif

ifeq (regalloc,$(findstring regalloc,$(debug)))
	CFLAGS += -DDEBUG_REGALLOC
	CFLAGS += -DDEBUG_INTERVALS
	CFLAGS += -DDEBUG_SPILL_COSTS
	CFLAGS += -DDEBUG_INTERFERENCE
	CFLAGS += -DDEBUG_MOVES
	CFLAGS += -DDEBUG_BRIGGS
	CFLAGS += -DDEBUG_HOMES
endif

ifeq (regalloc_dot,$(findstring regalloc_dot,$(debug)))
	CFLAGS += -DDEBUG_INTERFERENCE_DOT
	CFLAGS += -DDEBUG_MOVES_DOT
	CFLAGS += -DDEBUG_COALESCED_INTERFERENCE_DOT
endif

ifeq (coalesce,$(findstring coalesce,$(debug)))
	CFLAGS += -DDEBUG_COALESCE
	CFLAGS += -DDEBUG_COALESCED_INTERFERENCE
endif

ifeq (spills,$(findstring spills,$(debug)))
	CFLAGS += -DDEBUG_SPILLS
endif

ifeq (emitter,$(findstring emitter,$(debug)))
	CFLAGS += -DDEBUG_EMITTER
endif

all: $(BIN)

obj/%.o: src/%.c $(HDR)
	mkdir -vp obj
	$(CC) -c $(CFLAGS) $< -o $@

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -rvf obj $(BIN)
	rm -f graph.gv graph.png callgrind.out

# Profiling helpers:
#   sudo apt install valgrind graphviz
#   python3 -m pip install gprof2dot
#   make graph.png benchmark="path/to/input"

callgrind.out: $(BIN)
	valgrind --tool=callgrind --callgrind-out-file=$@ ./$(BIN) $(benchmark)

graph.gv: callgrind.out
	gprof2dot $< --format=callgrind --output=$@

graph.png: graph.gv
	dot -Tpng $< -o $@

format:
	clang-format src/*.c src/*.h -style=file:.clang-format -i

.PHONY: all clean test format ruff
