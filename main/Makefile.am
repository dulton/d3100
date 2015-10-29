AUTOMAKE_OPTIONS=foreign

RUNTIME_CPP= teacher_track/runtime.cpp

bin_PROGRAMS= teacher_detect test_ptz test_kvconfig test_detect_dummy test_udpsrv test_arm_detect

if PTZSRV
bin_PROGRAMS+= ptzsrv
endif


teacher_detect_SOURCES=	teacher_track/teacher_track.cpp \
						teacher_track/cJSON.c \
						teacher_track/fsm.cpp \
						teacher_track/reg.cpp \
						teacher_track/policy.cpp \
						teacher_track/policy/p1.cpp \
						teacher_track/udpsrv.cpp \
						teacher_track/detector.cpp \
						teacher_track/log.cpp \
						${RUNTIME_CPP} \
						teacher_track/switch.cpp \
						libptz/ptz.cpp \
						libptz/tcp_util.cpp \
						libptz/polyfit.cpp \
						libkvconfig/kvconfig.cpp


if DUMMY
teacher_detect_SOURCES+= libteacher_detect/detect_dummy.cpp
else


HISLIBS=  ../hi353x/lib/libmpi.a

noinst_LIBRARIES = libteacher_detect.a
teacher_detect_SOURCES+= libteacher_detect/detect.cpp
teacher_detect_LDADD= libteacher_detect.a ${HISLIBS}


test_arm_detect_SOURCES=libteacher_detect/test/test_arm_detect.cpp  
test_arm_detect_LDADD= libteacher_detect.a ${HISLIBS}

TDPATH= ../hi353x/detect
libteacher_detect_a_SOURCES= ${TDPATH}/mpi_wrap.cpp
libteacher_detect_a_CXXFLAGS = -I${TDPATH} -I../hi353x/include
endif


test_ptz_SOURCES= libptz/test/test_ptz.cpp \
				  libptz/ptz.cpp \
				  libptz/polyfit.cpp \
				  ${RUNTIME_CPP} \
				  teacher_track/log.cpp \
				  libptz/tcp_util.cpp

test_kvconfig_SOURCES= libkvconfig/test/test_kvc.cpp \
					   libkvconfig/kvconfig.cpp

test_detect_dummy_SOURCES= libteacher_detect/detect_dummy.cpp \
					       ${RUNTIME_CPP} \
						   teacher_track/log.cpp \
						   libteacher_detect/test/test_detect_dummy.cpp

test_udpsrv_SOURCES= teacher_track/test/test_udpsrv.cpp \
					 teacher_track/udpsrv.cpp \
					 teacher_track/fsm.cpp teacher_track/cJSON.c \
					 ${RUNTIME_CPP} \
					 teacher_track/log.cpp

ptzsrv_SOURCES= libptz/ptzsrv/ptzsrv.cpp teacher_track/cJSON.c  ${RUNTIME_CPP}
ptzsrv_CFLAGS= $(CCGNU2_CFLAGS)
ptzsrv_LDADD= $(CCGNU2_LIBS)
