# 名前
SOURCES = main.c ecalc.c
JITSOURCES = jitmain.c ecalc.c ecalc_jit.c ecalc_jit_32.c ecalc_jit_64.c ecalc_jit_aarch64.c
TARGET  = main
JITTARGET = ecalc

LINK = -lm

# 環境定数
CC = gcc

# ルール
.PHONY: clean install

all :
	$(MAKE) $(TARGET)

$(TARGET) : $(SOURCES)
	$(CC) -O2 -o $(TARGET) $(SOURCES) -Wall $(LINK)

$(JITTARGET) : $(JITSOURCES)
	$(CC) -O2 -o $(JITTARGET) $(JITSOURCES) -Wall $(LINK)

install : $(JITTARGET)
	sudo install -t /usr/local/bin ecalc

clean :
	-rm a.out
	-rm ecalc
	-rm jitmain
	-rm main
	-rm *.o
	-rm $(TARGET)
	-rm $(JITTARGET)

