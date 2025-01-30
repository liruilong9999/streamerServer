#ifndef VIDEOCOMMON_GLOBAL_H
#define VIDEOCOMMON_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(VIDEOCOMMON_LIBRARY)
#define VIDEOCOMMON_EXPORT Q_DECL_EXPORT
#else
#define VIDEOCOMMON_EXPORT Q_DECL_IMPORT
#endif

#endif // !
