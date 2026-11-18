.PHONY: all clean

include ../../config.make

CUR_DIR = $(shell pwd)
MODULE = $(shell basename $(CUR_DIR))

SRC_DIR = $(ROOT_DIR)/code/modules/$(MODULE)/src
SRC_FILE_DIRS = $(SRC_DIR)

LIB_DIR = $(ROOT_DIR)/code/modules/$(MODULE)/lib/$(PLATFORM)/$(CONFIG)

INTERMEDIATEDIR = $(CUR_DIR)/intermediate

#头文件目录
CFLAGS += -I $(ROOT_DIR)/code/modules/$(MODULE)/inc
CFLAGS += -I $(ROOT_DIR)/code/openbsd/vlc/inc
CFLAGS += -I $(ROOT_DIR)/code/openbsd/qt/inc

CPPFLAGS = $(CFLAGS)

FILES_C   = $(subst $(SRC_DIR)/, , $(foreach dir, $(SRC_FILE_DIRS) ,$(wildcard $(dir)/*.c)) )
FILES_CPP = $(subst $(SRC_DIR)/, , $(foreach dir, $(SRC_FILE_DIRS) ,$(wildcard $(dir)/*.cpp)) )
SRC_FILE = $(FILES_C) $(FILES_CPP)

#字符串替换获取.d文件
OBJS_TEMP = $(patsubst %.c,   $(INTERMEDIATEDIR)/%.o, $(SRC_FILE))
OBJS      = $(patsubst %.cpp, $(INTERMEDIATEDIR)/%.o, $(OBJS_TEMP))
OBJS_D    = $(patsubst %.o, %.d, $(OBJS))

LIBTARGET = $(LIB_DIR)/lib$(MODULE).a
LIBTARGET_SO = $(LIB_DIR)/lib$(MODULE).so

TARGET += $(LIBTARGET)
#TARGET += $(LIBTARGET_SO)

all: $(TARGET)

uic:
	#$(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/Release/uic $(ROOT_DIR)/code/modules/$(MODULE)/src/Ui.ui -o $(ROOT_DIR)/code/modules/$(MODULE)/inc/ui.h

moc:
	$(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/Release/moc $(ROOT_DIR)/code/modules/$(MODULE)/inc/dialogs.h -o $(ROOT_DIR)/code/modules/$(MODULE)/src/moc_dialogs.cpp
	$(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/Release/moc $(ROOT_DIR)/code/modules/$(MODULE)/inc/mediaapp.h -o $(ROOT_DIR)/code/modules/$(MODULE)/src/moc_mediaapp.cpp
	$(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/Release/moc $(ROOT_DIR)/code/modules/$(MODULE)/inc/viewers.h -o $(ROOT_DIR)/code/modules/$(MODULE)/src/moc_viewers.cpp

rcc:
	#$(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/Release/rcc -binary $(ROOT_DIR)/modules/$(MODULE)/code/src/Ui.qrc -o $(ROOT_DIR)/code/modules/$(MODULE)/src/Ui.rcc

ifneq ($(MAKECMDGOALS), clean)
sinclude $(OBJS_D)
endif

$(LIBTARGET): $(OBJS)
	@mkdir -p $(LIB_DIR)
	$(RM) -f $@;
	$(AR) -rv $@ $^
	ls -lh $@

$(LIBTARGET_SO): $(OBJS)
	@mkdir -p $(LIB_DIR)
	$(RM) -f $@;
	$(CC) -fPIC -shared -o $@ $(OBJS)
	ls -lh $@

$(INTERMEDIATEDIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(RM) -f $@
	$(CC) $(CFLAGS) -MM -MD -c $< -o $@
	@sed 's/.*\.o/$(subst /,\/,$(dir $@))&/g' $@ >$@.tmp
	@mv $@.tmp $@

$(INTERMEDIATEDIR)/%.d: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(RM) -f $@
	$(CPP) $(CPPFLAGS) -MM -MD -c $< -o $@
	@sed 's/.*\.o/$(subst /,\/,$(dir $@))&/g' $@ >$@.tmp
	@mv $@.tmp $@

$(INTERMEDIATEDIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC)  $(CFLAGS) -c -o $@ $<

$(INTERMEDIATEDIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CPP) $(CPPFLAGS) -c -o $@ $<

$(INTERMEDIATEDIR):
	@mkdir -p $(INTERMEDIATEDIR)

clean:
	@if [ -r "$(INTERMEDIATEDIR)" ] ; then rm -rf $(INTERMEDIATEDIR);fi
	rm $(TARGET) -rf
