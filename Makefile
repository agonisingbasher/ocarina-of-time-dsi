#---------------------------------------------------------------------------------
# Ocarina of Time DSi - Phase 0 hardware probe
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPRO)),)
export DEVKITPRO := /opt/devkitpro
endif

ifeq ($(strip $(DEVKITARM)),)
export DEVKITARM := $(DEVKITPRO)/devkitARM
endif

LIBNDS      := $(DEVKITPRO)/libnds
LIBPATHS   := -L$(LIBNDS)/lib

include $(DEVKITARM)/ds_rules

TARGET      := OcarinaDsi
OUTPUT      := $(TARGET)
BUILD       := build
SOURCES     := source source/nds source/gfx
INCLUDES    := include source source/nds source/gfx source/oot

ARCH        := -march=armv5te -mtune=arm946e-s -mthumb
INCLUDE     := $(foreach dir,$(INCLUDES),-I$(dir)) -I$(LIBNDS)/include

CFLAGS      := -g -Wall -Wextra -O2 -ffunction-sections -fdata-sections -std=gnu11
CFLAGS      += $(ARCH) $(INCLUDE) -DARM9
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(TARGET).map

# -lm because the decomp's float math (Math_FAsinF, guNormalize) calls sqrtf,
# sinf and friends; the game's own float path is newlib's software library, not
# the N64's FPU.
LIBS        := -lnds9 -lfat -lm

GAME_TITLE  := Ocarina DSi Phase 3

LD          := $(CC)

#---------------------------------------------------------------------------------
# The decomp's include world
#
# game/oot/include/libc shadows the newlib headers: its stdint.h typedefs
# intptr_t as "signed long" where newlib says "int", so a translation unit that
# reads both gets a conflicting-typedef error. The flags below therefore go only
# to source/oot/oot_os.c, source/oot/oot_selftest.c and the vendored decomp
# files, none of which may read <nds.h>; source/oot/oot_plat.c is the seam and
# is built with the flags above. oot_plat.h is included by both sides and uses
# plain C types for exactly that reason.
#
# The defines are what the reference Makefile passes for an NTSC-U 1.0,
# non-matching, retail build: _LANGUAGE_C is what makes ultratypes.h declare
# u32/f32/s32 at all, OOT_VERSION picks line_numbers.h, PLATFORM_N64 selects the
# N64 shape of Arena and ArenaNode (the malloc in the link expects it) and
# F3DEX_GBI_2 is the ucode whose display-list macros the decoder in source/gfx
# translates.
#---------------------------------------------------------------------------------
OOT_DEFINES := -D_LANGUAGE_C -D_MIPS_SZPTR=32 -DNON_MATCHING
OOT_DEFINES += -DOOT_VERSION=NTSC_1_0 -DOOT_REVISION=0 -DDEBUG_FEATURES=0
OOT_DEFINES += -DPLATFORM_N64=1 -DF3DEX_GBI_2
# game/oot/include/libc/assert.h declares __assert(expression, file, line) and
# newlib's <assert.h> declares __assert(function, line, file). The two orders
# cannot share one symbol, and newlib's definition is already in libg.a, so the
# decomp's assertion entry point gets its own name and source/oot/oot_os.c
# supplies it. Without this the link stops with a multiple-definition error.
OOT_DEFINES += -D__assert=ootCompatAssert
OOT_INCLUDE := -I game/oot/include -I game/oot/include/libc -I game/oot
OOT_INCLUDE += -I source/oot

OOT_BASE    := -g -O2 -std=gnu17 $(ARCH) $(OOT_DEFINES) $(OOT_INCLUDE)
OOT_CFLAGS  := $(OOT_BASE) -Wall -Wextra
# One function per section so --gc-sections can drop the parts of z_bgcheck.c
# this port does not call: the collision queries need two functions and the
# rest of the file reaches for PlayState, the actor system and the dyna
# collision lists, none of which exist on this side of the seam.
DECOMP_CFLAGS := $(OOT_BASE) -ffunction-sections -fdata-sections -w

# Vendored translation units that compile clean for ARM9 today, and nothing
# else: every other file in the decomp needs a generated asset header, which
# needs the NTSC-U 1.0 ROM. tools/oot_compile_scan.sh measures that split.
OOT_SRC     := game/oot/src/libc64/__osMalloc_n64.c game/oot/src/libc64/malloc.c
OOT_SRC     += game/oot/src/code/listalloc.c game/oot/src/code/gfxalloc.c
OOT_SRC     += game/oot/src/libc64/math64.c game/oot/src/code/sys_math_atan.c
OOT_SRC     += game/oot/src/libultra/gu/sins.c game/oot/src/libultra/gu/coss.c
OOT_SRC     += game/oot/src/libultra/gu/normalize.c
OOT_SRC     += game/oot/src/libultra/gu/mtxutil.c game/oot/src/libultra/gu/perspective.c
# The game's own matrix stack. sys_matrix.c is what an actor's modelview is
# built from, z_skin_matrix.c supplies the concatenation it calls, and
# TwoHeadArena.c supplies the allocator Matrix_Init takes its 20 slots from.
# Their only non-libc externals are Math_SinS/Math_CosS/Math_FAtan2F (already
# above), guMtxF2L (mtxutil.c) and __assert, which source/oot/oot_plat.c owns.
OOT_SRC     += game/oot/src/code/TwoHeadArena.c
OOT_SRC     += game/oot/src/code/z_skin_matrix.c game/oot/src/code/sys_matrix.c
# The scene collision mesh: sys_math3d.c is the point-in-triangle and
# line/plane arithmetic, z_bgcheck.c holds the two polygon queries and the
# floor/wall/ceiling thresholds. lookat.c is the walk camera (guLookAt), which
# needs guMtxIdentF and guMtxF2L that mtxutil.c already supplies.
OOT_SRC     += game/oot/src/code/sys_math3d.c game/oot/src/code/z_bgcheck.c
OOT_SRC     += game/oot/src/code/z_lib.c
OOT_SRC     += game/oot/src/libultra/gu/lookat.c

OOT_OBJS    := $(BUILD)/oot_plat.o $(BUILD)/oot_os.o $(BUILD)/oot_selftest.o
OOT_OBJS    += $(BUILD)/oot_dl.o $(BUILD)/oot_room.o $(BUILD)/oot_col.o
OOT_OBJS    += $(BUILD)/oot_play.o
OOT_OBJS    += $(patsubst game/oot/src/%.c,$(BUILD)/oot/%.o,$(OOT_SRC))

CFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
OFILES      := $(patsubst %.c,$(BUILD)/%.o,$(notdir $(CFILES)))
# ds_rules links $(OFILES), not $^, so the decomp objects have to join that list
# or they compile, sit in build/ and never reach the ELF.
OFILES      += $(OOT_OBJS)

.PHONY: all clean

all: $(TARGET).nds

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.o: source/%.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/$*.d $(_EXTRADEFS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: source/nds/%.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/$*.d $(_EXTRADEFS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: source/gfx/%.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/$*.d $(_EXTRADEFS) $(CFLAGS) -c $< -o $@

# oot_plat.c is the seam, so it is the only file in source/oot built with the
# project's flags.
$(BUILD)/oot_plat.o: source/oot/oot_plat.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/oot_plat.d $(_EXTRADEFS) $(CFLAGS) -c $< -o $@

$(BUILD)/oot_os.o: source/oot/oot_os.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/oot_os.d $(_EXTRADEFS) $(OOT_CFLAGS) -c $< -o $@

$(BUILD)/oot_selftest.o: source/oot/oot_selftest.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/oot_selftest.d $(_EXTRADEFS) $(OOT_CFLAGS) -c $< -o $@

$(BUILD)/oot_dl.o: source/oot/oot_dl.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/oot_dl.d $(_EXTRADEFS) $(OOT_CFLAGS) -c $< -o $@

# Reads a room's header with the decomp's own RoomShape structs, so it lives on
# that side of the seam and may not see <nds.h>.
$(BUILD)/oot_room.o: source/oot/oot_room.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/oot_room.d $(_EXTRADEFS) $(OOT_CFLAGS) -c $< -o $@

# The collision layer is plain C on the decomp's side of the seam: it reads the
# scene header with CollisionHeader's layout and calls the game's queries.
$(BUILD)/oot_col.o: source/oot/oot_col.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/oot_col.d $(_EXTRADEFS) $(OOT_CFLAGS) -c $< -o $@

# The walk slice: marker list in segment 4, camera and the 20 Hz movement that
# asks the collision layer where the floor and the walls are.
$(BUILD)/oot_play.o: source/oot/oot_play.c | $(BUILD)
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(BUILD)/oot_play.d $(_EXTRADEFS) $(OOT_CFLAGS) -c $< -o $@

# The stem keeps the decomp's own directory layout, so build/oot/libc64/malloc.o
# says which source it came from even when two of them share a basename.
$(BUILD)/oot/%.o: game/oot/src/%.c | $(BUILD)
	@echo $(notdir $<)
	@mkdir -p $(dir $@)
	@$(CC) -MMD -MP -MF $(BUILD)/oot/$*.d $(_EXTRADEFS) $(DECOMP_CFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)

-include $(OFILES:.o=.d)

clean:
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).map $(TARGET).lst
