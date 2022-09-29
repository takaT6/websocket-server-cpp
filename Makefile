# コンパイラを指定
CC :=g++
# インクルードファイル等
#CFLAGS := -DPACKET_DUMP
#CFLAGS := 
LDFLAGS :=
LIB := -lpthread
# ディレクトリ内の全てのC++ファイルをコンパイル
SOURCES :=$(wildcard *.c)
# C++ファイルの.cppをとったものを実行ファイルの名前とする
EXECUTABLE :=$(SOURCES:.c=)

#all:$(EXECUTABLE)

#$(EXECUTABLE):$(SOURCES)


all: 
	g++ -o hoge websocket.cpp sc.cpp func.cpp

clean:
	    rm -rf $(EXECUTABLE)
