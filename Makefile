#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.

PROJECT_NAME := sntp
APP_VERSION = ""

include $(IDF_PATH)/make/project.mk
	
CPPFLAGS += -D ESP_APP_PROJECT_NAME=\"$(PROJECT_NAME)\"
