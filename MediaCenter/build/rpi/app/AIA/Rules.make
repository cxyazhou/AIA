.PHONY: all clean

include ./config.make

CUR_DIR = $(shell pwd)
TARGET_DIR:=$(CUR_DIR)/$(CONFIG)
TARGET:=$(TARGET_DIR)/AIA_MediaCenter

CFLAGS += -I $(ROOT_DIR)/code/modules/Ui/inc
CFLAGS += -I $(ROOT_DIR)/code/openbsd/qt/inc

SUBDIRS += $(ROOT_DIR)/build/$(PLATFORM)/modules/Ui

ALL_LIBS += $(ROOT_DIR)/code/modules/Ui/lib/$(PLATFORM)/$(CONFIG)/libUi.a

#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Multimedia.so.5
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5MultimediaWidgets.so.5
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Network.so.5
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5OpenGL.so.5
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Gui.so.5
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Widgets.so.5
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Core.so.5
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libicudata.so.56
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libicui18n.so.56
#ALL_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libicuuc.so.56

#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Multimedia.so.5
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5MultimediaWidgets.so.5
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Network.so.5
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5OpenGL.so.5
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Gui.so.5
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Widgets.so.5
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libQt5Core.so.5
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libicudata.so.56
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libicui18n.so.56
#SO_LIBS += $(ROOT_DIR)/code/openbsd/qt/lib/$(PLATFORM)/$(CONFIG)/libicuuc.so.56

RESOURCE += $(ROOT_DIR)/build/$(PLATFORM)/app/AIA/MediaConfig

LDFLAGS = $(CFLAGS)
LDFLAGS += $(ALL_LIBS) -lpthread -lm -ldl -lutil -lz -lasound -lQt5Core -lQt5Widgets -lQt5Gui -lvlc

all: rmtarget $(TARGET) cpresource

rmtarget:
	@rm -f $(TARGET)

cpsolibs:
	@cp $(SO_LIBS) $(TARGET_DIR)/ -rf

cpresource:
	@cp $(RESOURCE) $(TARGET_DIR)/ -rf
	@mkdir $(TARGET_DIR)/Config -p

$(TARGET): $(SUBDIRS) $(ROOT_DIR)/code/app/AIA/main.cpp
	@for i in $(SUBDIRS); do make -C $$i || exit; done
	@mkdir -p $(TARGET_DIR)
	$(CPP) $(ROOT_DIR)/code/app/AIA/main.cpp -o $(TARGET) $(LDFLAGS)
ifeq ($(CONFIG), Release)
	@$(STRIP) $(TARGET)
endif

clean: $(SUBDIRS)
	@for i in $(SUBDIRS); do make -C $$i clean || exit; done
	#@rm -f $(TARGET_DIR)/*
	#@if [ -f "$(TARGET)" ] ;	then  rm -rvf $(TARGET);fi 
