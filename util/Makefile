
# Compiler prefix based on architecture
ifneq ($(ARCH),)
        ARCH_PREFIX := $(join $(ARCH), -)
endif

# Compiler specifications
CC 	  := $(ARCH_PREFIX)gcc
CC_FLAGS  := -c -Wall -O3 -g -D_GNU_SOURCE
DEP_FLAGS := -MM -MT

# Compiler definitions
DEFINES :=

# Archiver specifications
AR 	 := $(ARCH_PREFIX)ar
AR_FLAGS := -rcs

# Linker specifications
LD 	 := $(ARCH_PREFIX)gcc
LD_DEBUG := -g
LD_FLAGS := -Wall -O3

# Cleaner specifications
RM 	 := rm
RM_FLAGS := -Rf

# Build directories
BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
DEP_DIR   := $(BUILD_DIR)/depend

# Input source files
SRC := usb.c

# Object & dependency files
DEP := $(patsubst %.c,$(DEP_DIR)/%.d, $(SRC))
OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o, $(SRC))


# System packages
SYSTEM_PACKAGES := libudev

# System headers/libraries
ifneq ($(SYSTEM_PACKAGES),)
        SYSTEM_INCLUDE_DIRS := `pkg-config --cflags $(SYSTEM_PACKAGES)`
        SYSTEM_LIBS := `pkg-config --libs $(SYSTEM_PACKAGES)`
endif

# All header files
ALL_INCLUDE_DIRS := $(LOCAL_INCLUDE_DIRS)

# Library output files
TARGET_FILE := libutil.a
TARGET_GOAL := $(BUILD_DIR)/$(TARGET_FILE)

# Build library
$(TARGET_GOAL) : $(OBJ)
	echo "\nBuilding $(notdir $@)"
	$(AR) $(AR_FLAGS) $@ $^

# Implicit rule for object files
$(OBJ_DIR)/%.o : %.c
	echo "\nCompiling $<"
	$(CC) $(CC_FLAGS) $(DEFINES) $(ALL_INCLUDE_DIRS) $< -o $@

# Implicit rule for dependency files
$(DEP_DIR)/%.d : %.c
	$(CC) $(DEP_FLAGS) $(patsubst %.c,$(OBJ_DIR)/%.o, $(notdir $<)) \
	$(DEFINES) $(LOCAL_INCLUDE_DIRS) $< > $@
	$(CC) $(DEP_FLAGS) $(patsubst %.c,$(DEP_DIR)/%.d, $(notdir $<)) \
	$(DEFINES) $(LOCAL_INCLUDE_DIRS) $< >> $@

ifneq ($(MAKECMDGOALS),clean)
        $(shell mkdir -p $(BUILD_DIR))
        $(shell mkdir -p $(OBJ_DIR))
        $(shell mkdir -p $(DEP_DIR))
        -include $(DEP)
endif
 
clean :
	-$(RM) $(RM_FLAGS) $(BUILD_DIR)

.PHONY : clean

