SRC := ./src
BIN := ./bin
OBJS := ./objs
INC := -I ./include
FLAGS := -c $(INC)
LINK := 
CC := gcc
SAN_FLAGS := -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer

TESTS := ./tests
TEST_TARGETS := 

OBJ := array_list diagnostic lexer parser LList token ast hashtab symtab analyzer ssair

lc: $(addprefix $(OBJS)/,$(addsuffix .o,$(OBJ))) | $(BIN)
	$(CC) $^ $(SRC)/lc.c -o $(BIN)/$@ $(LINK)

asan: | $(BIN)
	$(CC) $(SAN_FLAGS) $(INC) $(addprefix $(SRC)/,$(addsuffix .c,$(OBJ))) $(SRC)/lc.c -o $(BIN)/lc-asan $(LINK)

$(OBJS)/%.o: $(SRC)/%.c | $(OBJS)
	$(CC) $(FLAGS) $< -o $@

$(OBJS):
	mkdir $(OBJS)

$(BIN):
	mkdir $(BIN)

debug:
	$(CC) $(INC) $(addprefix $(SRC)/,$(addsuffix .c,$(OBJ))) $(SRC)/lc.c -pthread -g -o $(BIN)/db $(LINK)
	gdb -tui $(BIN)/db


#Testing
test: all_tests
	@for target in $(TEST_TARGETS); do \
		$(TESTS)/bin/$$target; \
	done

all_tests: $(addprefix $(TESTS)/bin/, $(TEST_TARGETS))

$(TESTS)/bin/%_test: ../testLibC/utestC.c $(TESTS)/%_test.c $(OBJ)
	$(CC) $(INC) $^ $(LINK) -g -o $@

clean:
	rm -rf $(OBJS)/*.o $(BIN)/*
	rm -rf $(TESTS)/bin/*

run:
	$(BIN)/lc parseTest.lep
