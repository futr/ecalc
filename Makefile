# 名前
SOURCES = main.c ecalc.c ecalc_jit.c
TARGET  = main

LINK = -lm

# 環境定数
CC = gcc

# ルール
.PHONY: clean

all :
	$(MAKE) $(TARGET)

$(TARGET) : $(SOURCES)
	$(CC) -O2 -o $(TARGET) $(SOURCES) -Wall $(LINK)

clean :
	-del *.o
	-del $(TARGET)

