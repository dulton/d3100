AUTOMAKE_OPTIONS=foreign


bin_PROGRAMS= teacher_detect test_ptz test_kvconfig

teacher_detect_SOURCES=	teacher_track/teacher_track.c \
						teacher_track/cJSON.c \
						libptz/ptz.cpp \
						libptz/tcp_util.cpp \
						libkvconfig/kvconfig.cpp
if DUMMY
teacher_detect_SOURCES+= libteacher_detect/detect_dummy.cpp
else
# @老刘，在这里添加你的教师探测代码
teacher_detect_SOURCES+= notfound.cpp
endif


test_ptz_SOURCES= libptz/test/test_ptz.cpp \
				  libptz/ptz.cpp \
				  libptz/tcp_util.cpp

test_kvconfig_SOURCES= libkvconfig/test/test_kvc.cpp \
					   libkvconfig/kvconfig.cpp
