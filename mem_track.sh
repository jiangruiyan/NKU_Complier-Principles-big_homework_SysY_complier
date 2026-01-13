#!/bin/bash
# 你可以使用这个脚本来检查内存泄漏

INPUT_FILE="${1:-test.sy}"
STAGE="${2:-S}"
OPT_LEVEL="${3:-0}"

if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file '$INPUT_FILE' not found"
    exit 1
fi

STAGE_FLAG=""
case "$STAGE" in
    lexer)
        STAGE_FLAG="-lexer"
        ;;
    parser)
        STAGE_FLAG="-parser"
        ;;
    llvm)
        STAGE_FLAG="-llvm"
        ;;
    S)
        STAGE_FLAG="-S"
        ;;
    *)
        echo "Error: Invalid stage '$STAGE'. Must be one of: lexer, parser, llvm, S"
        exit 1
        ;;
esac

OPT_FLAG=""
case "$OPT_LEVEL" in
    0)
        OPT_FLAG="-O0"
        ;;
    1)
        OPT_FLAG="-O1"
        ;;
    2)
        OPT_FLAG="-O2"
        ;;
    3)
        OPT_FLAG="-O3"
        ;;
    *)
        echo "Error: Invalid optimization level '$OPT_LEVEL'. Must be 0, 1, 2, or 3"
        exit 1
        ;;
esac

valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file="valgrind.log" \
         ./bin/compiler "$INPUT_FILE" $STAGE_FLAG -o valgrind.out $OPT_FLAG

VALGRIND_EXIT=$?

if [ -f "valgrind.out" ]; then
    rm "valgrind.out"
fi

exit $VALGRIND_EXIT
