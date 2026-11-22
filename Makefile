SRC=main.cpp
EXE=uf2merge.exe

CFLAGS=-Wall -c
vpath %.cpp src

mode=release
ifeq ($(mode),debug)
 BUILDDIR := debug
 CFLAGS += -g -O0 -DDEBUG
else
 ifeq ($(mode),release)
  BUILDDIR := release
  CFLAGS += -O3
 else
  $(error unknown mode: $(mode))
 endif
endif

OBJS=$(addprefix $(BUILDDIR)/,$(SRC:.cpp=.o))

all: folder $(OBJS)
	g++ -o $(BUILDDIR)/$(EXE) $(OBJS)

folder:
	-mkdir $(BUILDDIR)

$(BUILDDIR)/%.o: %.cpp
	g++ $(CFLAGS) -o $@ $<

clean:
	-rmdir /s /q debug
	-rmdir /s /q release
