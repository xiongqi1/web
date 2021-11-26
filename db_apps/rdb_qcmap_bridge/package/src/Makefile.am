if DEBUG
OPT_CXXFLAGS = -g -O0
else
OPT_CXXFLAGS = -Os -DNDEBUG
endif

AM_CXXFLAGS = -std=c++17 -Wall -Werror -Weffc++ -Wfatal-errors -rdynamic $(OPT_CXXFLAGS) \
	$(DATA_CFLAGS) \
	$(DSUTILS_CFLAGS) \
	$(QMI_CFLAGS) \
	$(QMIFRAMEWORK_CFLAGS) \
	$(ESTD_CFLAGS)


AM_LDFLAGS = -lstdc++ -ldl -lrdb -lqcmap_client \
	$(DSUTILS_LIBS) \
	$(QMI_LIBS) \
	$(QMIFRAMEWORK_LIBS) \
	$(ESTD_LIBS)

bin_PROGRAMS = rdb_qcmap_bridge
rdb_qcmap_bridge_SOURCES = \
	main.cpp \
	qmiclient.cpp \
	qmitraits.cpp \
	qmistrres.cpp \
	rdbqcmapbridge.cpp \
	serialization.cpp \
	setoperation.cpp
