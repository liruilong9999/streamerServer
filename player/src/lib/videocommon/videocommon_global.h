/*!
 * \file  .\src\lib\videocommon\videocommon_global.h.
 *
 * Declares the videocommon global class
 */

#ifndef VIDEOCOMMON_GLOBAL_H
#define VIDEOCOMMON_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(VIDEOCOMMON_LIBRARY)

/*!
 * 宏定义 videocommon 导出
 *
 * \author bf
 * \date 2025/8/5
 */

#define VIDEOCOMMON_EXPORT Q_DECL_EXPORT
#else

/*!
 * 宏定义 videocommon 导出
 *
 * \author bf
 * \date 2025/8/5
 */

#define VIDEOCOMMON_EXPORT Q_DECL_IMPORT
#endif

#endif // !
