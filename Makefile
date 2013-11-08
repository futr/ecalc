# 名前
SOURCES	= main.c ecalc.c
TARGET	= main

LINK = -lm

# 環境定数
CC = clang

# ルール
.PHONY: clean

all :
	$(MAKE) $(TARGET)

$(TARGET) : $(SOURCES) Makefile
	$(CC) -O2 -o $(TARGET) $(SOURCES) -Wall $(LINK)

clean :
	-del *.o
	-del $(TARGET)

