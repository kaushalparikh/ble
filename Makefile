
# Compiler prefix based on architecture
ifneq ($(ARCH),)
        ARCH_PREFIX := $(join $(ARCH), -)
endif

# Compiler specifications
CC 	  := $(ARCH_PREFIX)gcc
CC_FLAGS  := -c -Wall -O3 -g -fno-strict-aliasing -Wno-unused-function -D_GNU_SOURCE
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
SRC_MASTER := sync.c device.c temperature.c profile.c ble.c main.c

# Object & dependency files
DEP_MASTER := $(patsubst %.c,$(DEP_DIR)/%.d, $(SRC_MASTER))
OBJ_MASTER := $(patsubst %.c,$(OBJ_DIR)/%.o, $(SRC_MASTER))

# System packages
SYSTEM_PACKAGES := libudev sqlite3

# System headers/libraries
ifneq ($(SYSTEM_PACKAGES),)
        SYSTEM_INCLUDE_DIRS := `pkg-config --cflags $(SYSTEM_PACKAGES)`
        SYSTEM_LIBS := `pkg-config --libs $(SYSTEM_PACKAGES)`
endif
SYSTEM_LIBS += -lrt -lpthread

# Local packages
LOCAL_PACKAGES := util

# Local headers/libraries
ifneq ($(LOCAL_PACKAGES),)
        LOCAL_INCLUDE_DIRS := $(foreach i, $(LOCAL_PACKAGES), -I $(i))
        LOCAL_LIBS := $(join $(LOCAL_PACKAGES), $(patsubst %, /$(BUILD_DIR)/lib%.a, $(LOCAL_PACKAGES)))
endif

# All header files
ALL_INCLUDE_DIRS := $(SYSTEM_INCLUDE_DIRS) $(LOCAL_INCLUDE_DIRS)

# Determine build targets
ifeq ($(MAKECMDGOALS),)
        TARGET_FILE := ble
else
        ifneq ($(MAKECMDGOALS),clean)
                TARGET_FILE := $(MAKECMDGOALS)
        endif
endif

ifneq ($(MAKECMDGOALS),clean)
        TARGET_GOAL := $(patsubst %, $(BUILD_DIR)/%, $(TARGET_FILE))
endif

# Default target, build all
$(TARGET_FILE) : $(TARGET_GOAL)
	echo

$(BUILD_DIR)/ble : $(LOCAL_LIBS) $(OBJ_MASTER)
	echo "\nBuilding $(patsubst %/$(BUILD_DIR)/, %, $(notdir $@))"
	$(CC) $(LD_FLAGS) $(DEFINES) $(OBJ_MASTER) $(LOCAL_LIBS) $(SYSTEM_LIBS) -o $@

%.a : .FORCE
	echo
	echo "---> Entering '$(patsubst %/build/,%, $(dir $@))' directory"
	make --no-print-directory -C $(patsubst %/build/, %, $(dir $@))
	echo
	echo "<--- Exiting '$(patsubst %/build/,%, $(dir $@))' directory"

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
        ifneq ($(findstring ble-master, $(TARGET_FILE)),)
                -include $(DEP_MASTER)
        endif
endif
 
clean :
	-$(foreach package, $(LOCAL_PACKAGES), \
	echo; \
	echo "---> $(package)"; \
	make --no-print-directory -C $(package) clean; \
	echo; \
	echo "<--- $(package)";)
	echo "\nCleaning all\n"
	-$(RM) $(RM_FLAGS) $(BUILD_DIR)

.PHONY : clean

.FORCE :

