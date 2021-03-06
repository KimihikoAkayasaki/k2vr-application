﻿#pragma once
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>
#include <QApplication>
#include <Windows.h>

#if defined MAKE_KINECTTOVR_LIB
#define KINECTTOVR_LIB Q_DECL_EXPORT
#else
 #define KINECTTOVR_LIB Q_DECL_IMPORT
#endif

extern "C" KINECTTOVR_LIB void initLogging();
