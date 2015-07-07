//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#ifndef VRAY_FOR_HOUDINI_UTIL_DEFINES_H
#define VRAY_FOR_HOUDINI_UTIL_DEFINES_H

#define CGR_PLUGIN_NAME V-Ray For Houdini

#ifdef DEBUG
#  define CGR_USE_DEBUG      1
#else
#  define CGR_USE_DEBUG      1
#endif

#define CGR_USE_CALL_DEBUG  (1 && CGR_USE_DEBUG)
#define CGR_USE_TIME_DEBUG  (1 && CGR_USE_DEBUG)
#define CGR_USE_DRAW_DEBUG  (0 && CGR_USE_DEBUG)
#define CGR_USE_DESTR_DEBUG (1 && CGR_USE_DEBUG)
#define CGR_DEBUG_SOCKETS   (0 && CGR_USE_DEBUG)
#define CGR_DEBUG_JSON_ATTR (0 && CGR_USE_DEBUG)

#ifndef WIN32
#  define COLOR_RED      "\033[0;31m"
#  define COLOR_GREEN    "\033[0;32m"
#  define COLOR_YELLOW   "\033[0;33m"
#  define COLOR_BLUE     "\033[0;34m"
#  define COLOR_MAGENTA  "\033[0;35m"
#  define COLOR_DEFAULT  "\033[0m"
#else
#  define COLOR_RED      ""
#  define COLOR_GREEN    ""
#  define COLOR_YELLOW   ""
#  define COLOR_BLUE     ""
#  define COLOR_MAGENTA  ""
#  define COLOR_DEFAULT  ""
#endif

#define NOT(x) !(x)

#define STRINGIZE_NX(A) #A
#define STRINGIZE(A) STRINGIZE_NX(A)

#ifdef CGR_PLUGIN_NAME
#  define _OUTPUT_PROMPT(P)         COLOR_MAGENTA STRINGIZE(P)            COLOR_DEFAULT ": "
#  define _OUTPUT_ERROR_PROMPT(P)   COLOR_RED     STRINGIZE(P) " Error"   COLOR_DEFAULT ": "
#  define _OUTPUT_WARNING_PROMPT(P) COLOR_YELLOW  STRINGIZE(P) " Warning" COLOR_DEFAULT ": "
#  define OUTPUT_PROMPT         _OUTPUT_PROMPT(CGR_PLUGIN_NAME)
#  define OUTPUT_ERROR_PROMPT   _OUTPUT_ERROR_PROMPT(CGR_PLUGIN_NAME)
#  define OUTPUT_WARNING_PROMPT _OUTPUT_WARNING_PROMPT(CGR_PLUGIN_NAME)
#else
#  define OUTPUT_PROMPT         COLOR_MAGENTA "Info"    COLOR_DEFAULT ": "
#  define OUTPUT_ERROR_PROMPT   COLOR_RED     "Error"   COLOR_DEFAULT ": "
#  define OUTPUT_WARNING_PROMPT COLOR_YELLOW  "Warning" COLOR_DEFAULT ": "
#endif

#if CGR_USE_DEBUG == 0
#define PRINT_INFO(...) {}
#define PRINT_WARN(...) {}
#else
#define PRINT_INFO(...) {\
	fprintf(stdout, OUTPUT_PROMPT); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	fflush(stdout); }

#define PRINT_WARN(...) { \
	fprintf(stdout, OUTPUT_WARNING_PROMPT); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	fflush(stdout); }
#endif

#define PRINT_ERROR(...) {\
	fprintf(stdout, OUTPUT_ERROR_PROMPT); \
	fprintf(stdout, __VA_ARGS__); \
	fprintf(stdout, "\n"); \
	fflush(stdout); }

#if CGR_USE_DEBUG == 0
#  define PRINT_TM4(label, tm) ()
#  define PRINT_TM3(label, tm) ()
#else
#  define PRINT_TM4(label, tm) \
	PRINT_INFO("%s:", label); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[0][0], tm[0][1], tm[0][2], tm[0][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[1][0], tm[1][1], tm[1][2], tm[1][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[2][0], tm[2][1], tm[2][2], tm[2][3]); \
	PRINT_INFO("  %.3f %.3f %.3f %.3f", tm[3][0], tm[3][1], tm[3][2], tm[3][3]);
#  define PRINT_TM3(label, tm) \
	PRINT_INFO("%s:", label); \
	PRINT_INFO("  %.3f %.3f %.3f", tm[0][0], tm[0][1], tm[0][2]); \
	PRINT_INFO("  %.3f %.3f %.3f", tm[1][0], tm[1][1], tm[1][2]); \
	PRINT_INFO("  %.3f %.3f %.3f", tm[2][0], tm[2][1], tm[2][2]);
#endif

#if CGR_USE_DEBUG == 0
#  define PRINT_APPSDK_TM(label, tm) {}
#  define PRINT_VRAYSDK_TM(label, tm) {}
#else
#  define PRINT_APPSDK_TM(label, tm) \
	PRINT_INFO("%s:", label); \
	PRINT_INFO("  %.3f %.3f %.3f", tm.matrix[0][0], tm.matrix[0][1], tm.matrix[0][2]); \
	PRINT_INFO("  %.3f %.3f %.3f", tm.matrix[1][0], tm.matrix[1][1], tm.matrix[1][2]); \
	PRINT_INFO("  %.3f %.3f %.3f", tm.matrix[2][0], tm.matrix[2][1], tm.matrix[2][2]); \
	PRINT_INFO("  %.3f %.3f %.3f", tm.offset[0], tm.offset[1], tm.offset[2]);
#  define PRINT_VRAYSDK_TM(label, tm) \
	PRINT_INFO("%s:", label); \
	PRINT_INFO("  %.3f %.3f %.3f", tm.m[0][0], tm.m[0][1], tm.m[0][2]); \
	PRINT_INFO("  %.3f %.3f %.3f", tm.m[1][0], tm.m[1][1], tm.m[1][2]); \
	PRINT_INFO("  %.3f %.3f %.3f", tm.m[2][0], tm.m[2][1], tm.m[2][2]); \
	PRINT_INFO("  %.3f %.3f %.3f", tm.offs[0], tm.offs[1], tm.offs[2]);
#endif

#if CGR_DEBUG_SOCKETS
#  define DEBUG_SOCKET(...) PRINT_INFO(__VA_ARGS__)
#else
#  define DEBUG_SOCKET(...) {}
#endif

#define StrEq(nameA, nameB) (vutils_strcmp(nameA, nameB) == 0)

#define FreePtr(p)    if (p) { delete    p; p = nullptr; }
#define FreePtrArr(p) if (p) { delete [] p; p = nullptr; }

template <typename T, int N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define CountOf(array) (sizeof(ArraySizeHelper(array)))

#endif // VRAY_FOR_HOUDINI_UTIL_DEFINES_H
