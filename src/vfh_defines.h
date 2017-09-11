//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_UTIL_DEFINES_H
#define VRAY_FOR_HOUDINI_UTIL_DEFINES_H

// QT defines macro "foreach" to be the same as QT_FOREACH which is highly likely to collide
// openvdb/util/NodeMask has "foreach" method
// define this macro to stop QT from defining it
#ifndef QT_NO_KEYWORDS
#  define QT_NO_KEYWORDS
#endif
#ifdef Q_FOREACH
#  undef Q_FOREACH
#endif

#define STRINGIZE_NX(A) #A
#define STRINGIZE(A) STRINGIZE_NX(A)
#define NOT(x) !(x)

#define StrEq(nameA, nameB) (vutils_strcmp(nameA, nameB) == 0)

template<typename T>
void FreePtr(T* &p) {
	delete p;
	p = nullptr;
}

template<typename T>
void FreePtrArr(T* &p) {
	delete [] p;
	p = nullptr;
}

template <typename T, int N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define CountOf(array) (sizeof(ArraySizeHelper(array)))

#define IsFloatEq(a, b) VUtils::isZero(a - b, 1e-6f)

#define MemberEq(member) (member == other.member)
#define MemberFloatEq(member) (IsFloatEq(member, other.member))
#define MemberNotEq(member) (!(member == other.member))

/// Disables copy-constructor and assignment operator
#define VfhDisableCopy(cls) \
private: \
	cls(const cls&); \
	cls& operator=(const cls&);

#define FOR_IT(type, itName, var) int itName##Idx = 0; for (type::iterator itName = var.begin(); itName != var.end(); ++itName, ++itName##Idx)
#define FOR_CONST_IT(type, itName, var) int itName##Idx = 0; for (type::const_iterator itName = var.begin(); itName != var.end(); ++itName, ++itName##Idx)

#endif // VRAY_FOR_HOUDINI_UTIL_DEFINES_H
