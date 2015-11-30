AUTOMAKE_OPTIONS=foreign

RUNTIME_CPP= teacher_track/runtime.cpp

bin_PROGRAMS= teacher_detect test_ptz test_kvconfig test_udpsrv arm_track

if PTZSRV
bin_PROGRAMS+= ptzsrv
endif


teacher_detect_SOURCES=	teacher_track/teacher_track.cpp \
						teacher_track/cJSON.c \
						teacher_track/fsm.cpp \
						teacher_track/reg.cpp \
						teacher_track/policy.cpp \
						teacher_track/policy/p1.cpp \
						teacher_track/policy/p2.cpp \
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
test_detect_SOURCES= libteacher_detect/detect_dummy.cpp \
					 ${RUNTIME_CPP} \
					 teacher_track/log.cpp \
					 libteacher_detect/test/test_detect_dummy.cpp

teacher_detect_SOURCES+= libteacher_detect/detect_dummy.cpp

if OPENCV
test_himat_SOURCES=	libopencv_teacher_detect/test/test_himat.cpp libopencv_teacher_detect/hi_mat.cpp
test_himat_CXXFLAGS= ${OPENCV_CFLAGS}
test_himat_LDADD= ${OPENCV_LIBS}
endif

else

arm_track_SOURCES= teacher_track/teacher_track.cpp \
				   teacher_track/cJSON.c \
				   teacher_track/fsm.cpp \
				   teacher_track/reg.cpp \
				   teacher_track/policy.cpp \
				   teacher_track/policy/p1.cpp \
				   teacher_track/policy/p2.cpp \
				   teacher_track/udpsrv.cpp \
				   teacher_track/detector.cpp \
				   teacher_track/log.cpp \
				   ${RUNTIME_CPP} \
				   teacher_track/switch.cpp \
				   libptz/ptz.cpp \
				   libptz/tcp_util.cpp \
				   libptz/polyfit.cpp \
				   libkvconfig/kvconfig.cpp \
				   libteacher_detect/detect_dummy.cpp 

if OPENCV

HISLIBS=${top_srcdir}/../hi353x/lib/libmpi.a \
		${top_srcdir}/../hi353x/lib/libaec.a \
		${top_srcdir}/../hi353x/lib/libanr.a \
		${top_srcdir}/../hi353x/lib/libhdmi.a \
		${top_srcdir}/../hi353x/lib/libjpeg.a  \
		${top_srcdir}/../hi353x/lib/libpciv.a \
		${top_srcdir}/../hi353x/lib/libresampler.a \
		${top_srcdir}/../hi353x/lib/libtde.a \
		${top_srcdir}/../hi353x/lib/libVoiceEngine.a

bdir=	${top_srcdir}/libopencv_teacher_detect
	
libopencv_detect_a_CXXFLAGS=  -I${bdir}/common -I${bdir}/ive_include

noinst_LIBRARIES= libopencv_detect.a libzkvd.a  libzkvt.a

libopencv_detect_a_SOURCES= libopencv_teacher_detect/blackboard_detect.cpp \
							libopencv_teacher_detect/detect_t.cpp \
							libopencv_teacher_detect/KVConfig.cpp \
							libopencv_teacher_detect/detect_opencv.cpp \
							libopencv_teacher_detect/vi_src.cpp \
							libopencv_teacher_detect/hi_mat.cpp \
							libopencv_teacher_detect/common/loadbmp.c 

libopencv_detect_a_CFLAGS= $(libopencv_detect_a_CXXFLAGS)

teacher_detect_LDADD= libopencv_detect.a ${CCGNU2_LIBS} ${OPENCV_LIBS} ${HISLIBS}
teacher_detect_SOURCES+= libopencv_teacher_detect/detect_opencv.cpp
teacher_detect_CXXFLAGS= $(libopencv_detect_a_CXXFLAGS) ${OPENCV_CFLAGS}

bin_PROGRAMS += test_arm_opencv_detect test_visrc test_himat test_vt 
test_arm_opencv_detect_SOURCES= libopencv_teacher_detect/test/test_arm_opencv_detect.cpp
test_arm_opencv_detect_LDADD= libopencv_detect.a ${CCGNU2_LIBS} ${OPENCV_LIBS} ${HISLIBS}

test_himat_SOURCES=	libopencv_teacher_detect/test/test_himat.cpp libopencv_teacher_detect/hi_mat.cpp
test_himat_CXXFLAGS= ${OPENCV_CFLAGS} ${libopencv_detect_a_CXXFLAGS} 
test_himat_LDADD= ${OPENCV_LIBS} ${HISLIBS}

test_visrc_SOURCES= libopencv_teacher_detect/test/test_visrc.cpp \
					libopencv_teacher_detect/vi_src.cpp \
					libopencv_teacher_detect/KVConfig.cpp 
test_visrc_CXXFLAGS= ${OPENCV_CFLAGS} ${libopencv_detect_a_CXXFLAGS} -DWITHOUT_OCV
test_visrc_LDADD=  libzkvd.a ${HISLIBS} ${CCGNU2_LIBS}

libzkvd_a_SOURCES=	${top_srcdir}/vd/vd.cpp \
					${top_srcdir}/libopencv_teacher_detect/hi_mat.cpp
libzkvd_a_CXXFLAGS= ${OPENCV_CFLAGS} ${libopencv_detect_a_CXXFLAGS} -DWITHOUT_OCV

libzkvt_a_SOURCES=	${top_srcdir}/vt/vt.cpp \
					libopencv_teacher_detect/blackboard_detect.cpp \
					libopencv_teacher_detect/detect_t.cpp \
					libopencv_teacher_detect/KVConfig.cpp \
					libopencv_teacher_detect/detect_opencv.cpp \
					libopencv_teacher_detect/hi_mat.cpp \
					libopencv_teacher_detect/common/loadbmp.c 

libzkvt_a_CXXFLAGS=	${OPENCV_CFLAGS} ${libopencv_detect_a_CXXFLAGS} 
libzkvt_a_CFLAGS=	${OPENCV_CFLAGS} ${libopencv_detect_a_CXXFLAGS} 
	


test_vt_SOURCES=		${top_srcdir}/vt/test/testvt.cpp \
						libopencv_teacher_detect/vi_src.cpp
test_vt_CXXFLAGS= ${OPENCV_CFLAGS} ${libopencv_detect_a_CXXFLAGS} 
test_vt_LDADD= 	libzkvt.a ${OPENCV_LIBS} ${HISLIBS} ${CCGNU2_LIBS}


else

HISLIBS= ../hi353x/lib/libmpi.a ../hi353x/lib/libaec.a ../hi353x/lib/libanr.a \
		 ../hi353x/lib/libhdmi.a ../hi353x/lib/libjpeg.a  ../hi353x/lib/libpciv.a \
		 ../hi353x/lib/libresampler.a ../hi353x/lib/libtde.a ../hi353x/lib/libVoiceEngine.a


noinst_LIBRARIES = libteacher_detect.a
teacher_detect_SOURCES+= libteacher_detect/detect.cpp
teacher_detect_LDADD= libteacher_detect.a ${HISLIBS}

bin_PROGRAMS+= test_arm_detect

test_arm_detect_SOURCES= libteacher_detect/test/test_arm_detect.cpp  libteacher_detect/detect.cpp 
test_arm_detect_LDADD= libteacher_detect.a ${HISLIBS}

TDPATH= ../hi353x/detect
libteacher_detect_a_SOURCES= ${TDPATH}/mpi_wrap.cpp
libteacher_detect_a_CXXFLAGS = -I${TDPATH} -I../hi353x/include
#test_detect_SOURCES= libteacher_detect/detect_dummy2.cpp
endif
endif


test_ptz_SOURCES= libptz/test/test_ptz.cpp \
				  libptz/ptz.cpp \
				  libptz/polyfit.cpp \
				  ${RUNTIME_CPP} \
				  teacher_track/log.cpp \
				  libptz/tcp_util.cpp

test_kvconfig_SOURCES= libkvconfig/test/test_kvc.cpp \
					   libkvconfig/kvconfig.cpp

test_udpsrv_SOURCES= teacher_track/test/test_udpsrv.cpp \
					 teacher_track/udpsrv.cpp \
					 teacher_track/fsm.cpp teacher_track/cJSON.c \
					 ${RUNTIME_CPP} \
					 teacher_track/log.cpp

ptzsrv_SOURCES= libptz/ptzsrv/ptzsrv.cpp teacher_track/cJSON.c  ${RUNTIME_CPP}
ptzsrv_CFLAGS= $(CCGNU2_CFLAGS)
ptzsrv_LDADD= $(CCGNU2_LIBS)
