# Dimension
# (c) LifesMinder
#
# Usage:
#   make                          LLVM (clang++ + lld), x64, Debug
#   make CONFIG=Release           Release build
#   make ARCH=x86                 32-bit build (needs a 32-bit capable MinGW)
#   make GCC=1                    use g++ instead of clang++ (TOOLCHAIN=gcc works too)
#   make MINGW=D:/MinGW           MinGW-w64 root, if it is not on PATH
#   make run | clean | rebuild | info | help
#

# Options
CONFIG ?= Debug
ARCH   ?= x64
GCC    ?=
MINGW  ?=
LLVM   ?=

ifeq ($(CONFIG),debug)
CONFIG := Debug
endif
ifeq ($(CONFIG),release)
CONFIG := Release
endif
ifeq ($(ARCH),x86_64)
ARCH := x64
endif
ifeq ($(ARCH),win32)
ARCH := x86
endif

ifneq ($(filter-out Debug Release,$(CONFIG)),)
$(error CONFIG must be Debug or Release (got '$(CONFIG)'))
endif
ifneq ($(filter-out x64 x86,$(ARCH)),)
$(error ARCH must be x64 or x86 (got '$(ARCH)'))
endif

# GCC=1 is the flag; TOOLCHAIN=gcc / TOOLCHAIN=llvm also works
ifneq ($(GCC),)
TOOLCHAIN := gcc
endif
TOOLCHAIN ?= llvm
ifneq ($(filter-out llvm gcc,$(TOOLCHAIN)),)
$(error TOOLCHAIN must be llvm or gcc (got '$(TOOLCHAIN)'))
endif

# Host shell helpers
ifeq ($(OS),Windows_NT)
SHELL    := cmd.exe
winpath   = $(subst /,\,$1)
MKDIR     = if not exist "$(call winpath,$1)" mkdir "$(call winpath,$1)"
RMDIR     = if exist "$(call winpath,$1)" rmdir /s /q "$(call winpath,$1)"
EXE_RUN   = $(call winpath,$1)
else
winpath   = $1
MKDIR     = mkdir -p "$1"
RMDIR     = rm -rf "$1"
EXE_RUN   = ./$1
endif

# Layout
TAG     := $(ARCH)_$(CONFIG)
BIN     := bin/$(TAG)
OBJ     := obj/make-$(TOOLCHAIN)/$(TAG)

CORE_SRC   := $(wildcard core/*.cpp)
CLIENT_SRC := $(wildcard *.cpp)
CORE_RC    := $(wildcard core/*.rc)
# Includes manifest.rc, which only these builds need (Visual Studio embeds the manifest
# through a #pragma instead, and doesn't list manifest.rc in its project)
CLIENT_RC  := $(wildcard *.rc)

CORE_OBJ   := $(patsubst %.cpp,$(OBJ)/%.o,$(CORE_SRC))
CLIENT_OBJ := $(patsubst %.cpp,$(OBJ)/client/%.o,$(CLIENT_SRC))
CORE_RES   := $(patsubst %.rc,$(OBJ)/%.res.o,$(CORE_RC))
CLIENT_RES := $(patsubst %.rc,$(OBJ)/client/%.res.o,$(CLIENT_RC))

CORE_DLL   := $(BIN)/dimcore.dll
CORE_LIB   := $(BIN)/dimcore.lib
CLIENT_EXE := $(BIN)/dimension.exe

# Toolchain (MinGW-w64)
# Locate the MinGW-w64 root from g++ on PATH unless MINGW= was given
ifeq ($(MINGW),)
ifeq ($(OS),Windows_NT)
GXX_PATH := $(subst \,/,$(firstword $(shell where g++ 2>nul)))
MINGW    := $(patsubst %/bin/g++.exe,%,$(filter %/bin/g++.exe,$(GXX_PATH)))
endif
endif
ifneq ($(MINGW),)
MINGW_BIN := $(MINGW)/bin/
endif

ifeq ($(ARCH),x64)
ARCHFLAG := -m64
TRIPLE   := x86_64-w64-windows-gnu
GNU_TRIPLE := x86_64-w64-mingw32
else
ARCHFLAG := -m32
TRIPLE   := i686-w64-windows-gnu
GNU_TRIPLE := i686-w64-mingw32
endif

ifeq ($(TOOLCHAIN),llvm)
CXX      := $(if $(LLVM),$(LLVM)/)clang++
TARGET   := --target=$(TRIPLE)
ifneq ($(MINGW),)
TARGET   += --sysroot=$(MINGW)
endif
# With libstdc++ linked statically, clang's inline type_info::operator== collides with
# the out-of-line one in libstdc++.a (duplicate symbol); use the out-of-line one only
CXXFLAGS := $(TARGET) -D__GXX_TYPEINFO_EQUALITY_INLINE=0
LDFLAGS  := $(TARGET) -fuse-ld=lld
# lld does not know where MinGW keeps libgcc & co: point it there
ifneq ($(MINGW),)
GCC_LIBDIR := $(firstword $(wildcard $(MINGW)/lib/gcc/$(GNU_TRIPLE)/*))
LDFLAGS  += -L$(GCC_LIBDIR) -L$(MINGW)/$(GNU_TRIPLE)/lib
# Some MinGW builds fold the unwinder into libgcc.a and ship no libgcc_eh.a/libgcc_s.a,
# which clang's driver asks for: provide them as copies of libgcc.a
SHIMDIR  := $(OBJ)/shim
LDFLAGS  += -L$(SHIMDIR)
LINK_DEPS := $(SHIMDIR)/libgcc_eh.a $(SHIMDIR)/libgcc_s.a
endif
else
CXX      := $(MINGW_BIN)g++
CXXFLAGS := $(ARCHFLAG)
LDFLAGS  := $(ARCHFLAG)
endif
RC := $(MINGW_BIN)windres

COMMON_DEFS := -DWIN32 -D_WINDOWS -DUNICODE -D_UNICODE
CXXFLAGS += -std=c++20 -Wall -Wno-unused-function -Wno-unknown-pragmas -MMD -MP
# Static runtime: no libgcc/libstdc++ DLLs to ship next to the binaries
LDFLAGS  += -municode -mwindows -static-libgcc -static-libstdc++
ifeq ($(CONFIG),Debug)
COMMON_DEFS += -D_DEBUG
CXXFLAGS += -O0 -g
else
COMMON_DEFS += -DNDEBUG
CXXFLAGS += -O2
LDFLAGS  += -s
endif

INCLUDES := -I. -Icore
LIBS     := -lgdiplus -ld2d1 -ldwrite -lwindowscodecs -lmfplat -lmfreadwrite -lmfuuid \
            -lole32 -luuid -lgdi32 -luser32 -ladvapi32 -lshell32 -lcomdlg32 -lcomctl32 \
            -ldwmapi -luxtheme

# libstdc++ of a POSIX-threads MinGW build needs winpthreads (g++ adds it by itself)
ifeq ($(TOOLCHAIN),llvm)
LIBS     += -Wl,-Bstatic -lpthread -Wl,-Bdynamic
endif

CORE_DEFS   := $(COMMON_DEFS) -DDIM_EXPORTS -DCORE_EXPORTS -D_USRDLL
CLIENT_DEFS := $(COMMON_DEFS)

# $(1)=input .rc  $(2)=output object
rc_cmd   = $(RC) -O coff --target=pe-$(if $(filter x64,$(ARCH)),x86-64,i386) $(INCLUDES) $(COMMON_DEFS) -i $(1) -o $(2)
# $(1)=output  $(2)=objects  $(3)=resources
link_dll = $(CXX) $(LDFLAGS) -shared -o $(1) $(2) $(3) -Wl,--out-implib,$(CORE_LIB) $(LIBS)
link_exe = $(CXX) $(LDFLAGS) -o $(1) $(2) $(3) $(CORE_DLL) $(LIBS)

# Targets
.PHONY: all core client run clean rebuild info help
all: core client

core: $(CORE_DLL)
client: $(CLIENT_EXE)

ifneq ($(LINK_DEPS),)
ifeq ($(OS),Windows_NT)
COPYFILE = copy /Y "$(call winpath,$1)" "$(call winpath,$2)" >nul
else
COPYFILE = cp "$1" "$2"
endif
$(LINK_DEPS): $(GCC_LIBDIR)/libgcc.a
	@$(call MKDIR,$(@D))
	@$(call COPYFILE,$<,$@)
endif

$(CORE_DLL) $(CORE_LIB) &: $(CORE_OBJ) $(CORE_RES) $(LINK_DEPS)
	@$(call MKDIR,$(BIN))
	$(call link_dll,$(CORE_DLL),$(CORE_OBJ),$(CORE_RES))

$(CLIENT_EXE): $(CLIENT_OBJ) $(CLIENT_RES) $(CORE_DLL)
	@$(call MKDIR,$(BIN))
	$(call link_exe,$@,$(CLIENT_OBJ),$(CLIENT_RES))

$(OBJ)/core/%.o: core/%.cpp
	@$(call MKDIR,$(@D))
	$(CXX) $(CXXFLAGS) $(CORE_DEFS) $(INCLUDES) -c $< -o $@

$(OBJ)/client/%.o: %.cpp
	@$(call MKDIR,$(@D))
	$(CXX) $(CXXFLAGS) $(CLIENT_DEFS) $(INCLUDES) -c $< -o $@

$(OBJ)/core/%.res.o: core/%.rc
	@$(call MKDIR,$(@D))
	$(call rc_cmd,$<,$@)

$(OBJ)/client/%.res.o: %.rc
	@$(call MKDIR,$(@D))
	$(call rc_cmd,$<,$@)

run: client
	$(call EXE_RUN,$(CLIENT_EXE))

clean:
	@$(call RMDIR,$(OBJ))
	@$(call RMDIR,$(BIN))

rebuild: clean all

info:
	@echo Toolchain : $(TOOLCHAIN) ($(CXX))
	@echo Config    : $(CONFIG)
	@echo Arch      : $(ARCH)
	@echo Output    : $(BIN)
	@echo Core src  : $(CORE_SRC)
	@echo Client src: $(CLIENT_SRC)

help:
	@echo make [CONFIG=Debug^|Release] [ARCH=x64^|x86] [GCC=1] [MINGW=path] [all^|core^|client^|run^|clean^|rebuild^|info]

-include $(CORE_OBJ:.o=.d) $(CLIENT_OBJ:.o=.d)
