CC = gcc
CFLAGS = -Wall -O3 -mavx2 -Isrc
FLEX = flex
BISON = bison

SRCS = src/ast.c src/symbol_table.c src/semantic.c src/tac.c src/csv_parser.c src/simd_csv.c src/executor.c src/main.c
GEN_SRCS = src/query.tab.c src/lex.yy.c
TARGET = querylang

all: $(TARGET)

src/query.tab.c src/query.tab.h: src/query.y
	$(BISON) -d -o src/query.tab.c src/query.y

src/lex.yy.c: src/query.l src/query.tab.h
	$(FLEX) -o src/lex.yy.c src/query.l

$(TARGET): src/query.tab.c src/lex.yy.c $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(GEN_SRCS) $(SRCS)

test: $(TARGET)
	./$(TARGET) tests/test1_basic.ql
	./$(TARGET) --all tests/test2_aggregate.ql
	./$(TARGET) tests/test3_compound_filter.ql
	./$(TARGET) tests/test4_students.ql

clean:
	rm -f src/query.tab.c src/query.tab.h src/lex.yy.c $(TARGET) $(TARGET).exe phase1_prototype/phase1_proto.exe phase1_prototype/y.tab.c phase1_prototype/y.tab.h phase1_prototype/lex.yy.c

.PHONY: all test clean
