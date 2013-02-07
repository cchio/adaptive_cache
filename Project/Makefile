
TARGET = ssarc_trace

# Compilation environment setup
CC = gcc
CFLAGS := $(shell pkg-config --cflags glib-2.0) -ISource/Include \
-std=c99 -g -Wall -pedantic
LDFLAGS := $(shell pkg-config --libs glib-2.0)

# Directory structure
OBJDIR = Output/Object
OUTDIR = Output
SRCDIR = Source

# Compilation file symbolic grouping names
# Each file in a group must be in the same directory
# But you can have multiple groups that both reside in the same directory
# Each compilation file should be in at most one group
GROUPS = DISK CACHE

# List of files to compile
# Two variables need to be defined for each group:
# <GROUP_NAME>_SRC_STEMS
# <GROUP_NAME>_DIR_NAME
# Needs to be updated whenever you add/remove files

CACHE_SRC_STEMS = mgmt ssarc_trace ssarc
CACHE_DIR_NAME = Cache

DISK_SRC_STEMS = disk-sync
DISK_DIR_NAME = Disk

# Generation of file lists

listgen = $(addsuffix $(2),$(addprefix $(1),$(3)))

SRCS := $(foreach group,$(GROUPS),$(call listgen,\
$(SRCDIR)/$($(group)_DIR_NAME)/,.c,$($(group)_SRC_STEMS)))

OBJS := $(foreach group,$(GROUPS),$(call listgen,\
$(OBJDIR)/$($(group)_DIR_NAME)/,.o,$($(group)_SRC_STEMS)))

DEPS := $(foreach group,$(GROUPS),$(call listgen,\
$(OBJDIR)/$($(group)_DIR_NAME)/,.d,$($(group)_SRC_STEMS)))

# Build rules

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
# Awk line is to make the dependency file itself depend on headers as well
# Double dollar sign is to escape the dollar sign within makefile
# awk and rm command display is suppressed to reduce noise
$(OBJDIR)/%.d : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) -MF $@.tmp $<
	@awk 'NR == 1 {print "$@",$$0} NR > 1' < $@.tmp > $@
	@rm -f $@.tmp
	
$(TARGET) : $(OBJS) $(DEPS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

# Standard clean functionality
.PHONY: clean
clean:
	@rm -f $(TARGET) $(OBJS) core $(DEPS)

# If you specify clean, make will try to rebuild the dependency files
# unless you use this switch
ifneq "$(MAKECMDGOALS)" "clean"
  -include $(DEPS)
endif


