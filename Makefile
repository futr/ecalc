# 名前
SOURCES = main.c ecalc.c
JITSOURCES = jittest.c ecalc.c ecalc_jit.c ecalc_jit_32.c ecalc_jit_64.c ecalc_jit_aarch64.c
TARGET  = main
JITTARGET = jitmain

LINK = -lm

# 環境定数
CC = gcc

# ルール
.PHONY: clean

all :
	$(MAKE) $(TARGET)

$(TARGET) : $(SOURCES)
	$(CC) -O2 -o $(TARGET) $(SOURCES) -Wall $(LINK)

jitmain : $(JITSOURCES)
	$(CC) -O2 -o $(JITTARGET) $(JITSOURCES) -Wall $(LINK)

clean :
	-rm *.o
	-rm $(TARGET)
	-rm $(JITTARGET)

