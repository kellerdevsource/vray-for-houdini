//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAY_PRIMITIVES_H
#define VRAY_FOR_HOUDINI_VRAY_PRIMITIVES_H


#include <GU/GU_PackedImpl.h>
#include <GU/GU_PackedFactory.h>

#include <boost/preprocessor.hpp>


// UT_Options_setter, UT_Options_getter, PackedImplSetterCast and PackedImplGetterCast are template specialised wrappers over
// functions from HDK that have different names for functions with different types for arguments
// This is needed to have simpler VFH_MAKE_REGISTERS and VFH_MAKE_ACCESSORS
namespace VRayForHoudini {
// getters
template <typename T> inline void UT_Options_setter(UT_Options & opt, const char * name, const T & val);

template <> inline void UT_Options_setter<exint>          (UT_Options & opt, const char * name, const exint  & val)          { opt.setOptionI(name, val); }
template <> inline void UT_Options_setter<fpreal>         (UT_Options & opt, const char * name, const fpreal & val)          { opt.setOptionF(name, val); }
template <> inline void UT_Options_setter<UT_StringHolder>(UT_Options & opt, const char * name, const UT_StringHolder & val) { opt.setOptionS(name, val); }
template <> inline void UT_Options_setter<const char *>   (UT_Options & opt, const char * name, const char * const & val)    { opt.setOptionS(name, val); }
template <> inline void UT_Options_setter<bool>           (UT_Options & opt, const char * name, const bool & val)            { opt.setOptionB(name, val); }
template <> inline void UT_Options_setter<UT_StringArray> (UT_Options & opt, const char * name, const UT_StringArray & val)  { opt.setOptionSArray(name, val); }
 
// setters
template <typename T> T UT_Options_getter(const UT_Options & opt, const char * name);

template <> inline exint           UT_Options_getter<exint>          (const UT_Options & opt, const char * name) { return opt.getOptionI(name); }
template <> inline fpreal          UT_Options_getter<fpreal>         (const UT_Options & opt, const char * name) { return opt.getOptionF(name); }
template <> inline UT_StringHolder UT_Options_getter<UT_StringHolder>(const UT_Options & opt, const char * name) { return opt.getOptionS(name); }
template <> inline const char *    UT_Options_getter<const char *>   (const UT_Options & opt, const char * name) { return opt.getOptionS(name).nonNullBuffer(); }
template <> inline bool            UT_Options_getter<bool>           (const UT_Options & opt, const char * name) { return opt.getOptionB(name); }
template <> inline UT_StringArray  UT_Options_getter<UT_StringArray> (const UT_Options & opt, const char * name) { return opt.getOptionSArray(name); }

template <typename CLASS, typename T, typename Q>
inline Q PackedImplSetterCast(GU_PackedFactory * self, void (CLASS::*method)(T));

template <typename CLASS>
inline GU_PackedImpl::IntSetter PackedImplSetterCast(GU_PackedFactory * self, void (CLASS::*method)(GA_Size)) {
	return self->IntSetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::FloatSetter PackedImplSetterCast(GU_PackedFactory * self, void (CLASS::*method)(fpreal)) {
	return self->FloatSetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::StringSetter PackedImplSetterCast(GU_PackedFactory * self, void (CLASS::*method)(const char *)) {
	return self->StringSetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::BoolSetter PackedImplSetterCast(GU_PackedFactory * self, void (CLASS::*method)(bool)) {
	return self->BoolSetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::StringArraySetter PackedImplSetterCast(GU_PackedFactory * self, void (CLASS::*method)(const UT_StringArray &)) {
	return self->StringArraySetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::F64VectorSetter PackedImplSetterCast(GU_PackedFactory * self, void (CLASS::*method)(const UT_Vector3D &)) {
	return self->F64VectorSetterCast(method);
}

template <typename CLASS, typename T, typename Q>
inline Q PackedImplGetterCast(GU_PackedFactory * self, void (CLASS::*method)(T));

template <typename CLASS>
inline GU_PackedImpl::IntGetter PackedImplGetterCast(GU_PackedFactory * self, GA_Size (CLASS::*method)() const) {
	return self->IntGetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::FloatGetter PackedImplGetterCast(GU_PackedFactory * self, fpreal (CLASS::*method)() const) {
	return self->FloatGetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::StringGetter PackedImplGetterCast(GU_PackedFactory * self, const char * (CLASS::*method)() const) {
	return self->StringGetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::BoolGetter PackedImplGetterCast(GU_PackedFactory * self, bool (CLASS::*method)() const) {
	return self->BoolGetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::StringArrayGetter PackedImplGetterCast(GU_PackedFactory * self, void (CLASS::*method)(UT_StringArray &) const) {
	return self->StringArrayGetterCast(method);
}

template <typename CLASS>
inline GU_PackedImpl::F64VectorGetter PackedImplGetterCast(GU_PackedFactory * self, void (CLASS::*method)(UT_Vector3D &) const) {
	return self->F64VectorGetterCast(method);
}

}

#define VFH_STRINGIZE_HELPER(name) #name
#define VFH_STRINGIZE(name) VFH_STRINGIZE_HELPER(name)
#define VFH_TOKENIZE2_HELPER(a, b) a ## b
#define VFH_TOKENIZE2(a, b) VFH_TOKENIZE2_HELPER(a, b)
#define VFH_TOKENIZE3_HELPER(a, b, c) VFH_TOKENIZE2(VFH_TOKENIZE2(a, b), c)
#define VFH_TOKENIZE3(a, b, c) VFH_TOKENIZE3_HELPER(a, b, c)

// extracts the current iteration step from the state
#define VFH_CURRENT_ITER(state) BOOST_PP_TUPLE_ELEM(4, 0, state)
// extracts the paramters tuple from the state
#define VFH_PARAMS(state)       BOOST_PP_TUPLE_ELEM(4, 1, state)
// extracts the parameters count from the state
#define VFH_PARAMS_COUNT(state) BOOST_PP_TUPLE_ELEM(4, 2, state)
// extracts the class name you passed to VFH_MAKE_REGISTERS as 3rd argument
#define VFH_PARAMS_CLASS(state) BOOST_PP_TUPLE_ELEM(4, 3, state)

// constructs the new state, with incremented iter
#define VFH_OP(r, state) (BOOST_PP_INC(VFH_CURRENT_ITER(state)), VFH_PARAMS(state), VFH_PARAMS_COUNT(state), VFH_PARAMS_CLASS(state))
// tests if iteration should continue
#define VFH_PRED(r, state) BOOST_PP_NOT_EQUAL(VFH_CURRENT_ITER(state), VFH_PARAMS_COUNT(state))

// extracts the param info (type, name, default) for the current iteration
#define VFH_CURRENT_PARAM(state)   BOOST_PP_TUPLE_ELEM(VFH_PARAMS_COUNT(state), VFH_CURRENT_ITER(state), VFH_PARAMS(state))
// extracts current param type
#define VFH_CURRENT_TYPE(state)    BOOST_PP_TUPLE_ELEM(3, 0, VFH_CURRENT_PARAM(state))
// extracts current param name
#define VFH_CURRENT_NAME(state)    BOOST_PP_TUPLE_ELEM(3, 1, VFH_CURRENT_PARAM(state))
// extracts current param default
#define VFH_CURRENT_DEFAULT(state) BOOST_PP_TUPLE_ELEM(3, 2, VFH_CURRENT_PARAM(state))

// generates two methods for the successor of GU_PackedImpl from param of the form (type, name, default)
// void set_$name(const $type & val) { ... }
// $type get_$name() const { ... } // returns the default if $name is not previously set
#define VFH_ACCESSORS(r, state) \
	VFH_CURRENT_TYPE(state) VFH_TOKENIZE2(get_, VFH_CURRENT_NAME(state))() const {\
		const char * name = VFH_STRINGIZE(VFH_CURRENT_NAME(state));\
		return m_options.hasOption(name) ? VRayForHoudini::UT_Options_getter<VFH_CURRENT_TYPE(state)>(m_options, name) : VFH_CURRENT_DEFAULT(state);\
	} \
	void VFH_TOKENIZE2(set_, VFH_CURRENT_NAME(state))(VFH_CURRENT_TYPE(state) val) {\
		VRayForHoudini::UT_Options_setter<VFH_CURRENT_TYPE(state)>(m_options, VFH_STRINGIZE(VFH_CURRENT_NAME(state)), val);\
	}

// generates getter that returns by value, getter that returns by input argument, size of tuple getter and setter
#define VFH_ACCESSORS_TUPLE(r, state) \
	VFH_CURRENT_TYPE(state) VFH_TOKENIZE2(get_, VFH_CURRENT_NAME(state))() const {\
		VFH_CURRENT_TYPE(state) val;\
		VFH_TOKENIZE2(_get_, VFH_CURRENT_NAME(state))(val);\
		return val;\
	}\
	void VFH_TOKENIZE2(_get_, VFH_CURRENT_NAME(state))(VFH_CURRENT_TYPE(state) & val) const {\
		const char * name = VFH_STRINGIZE(VFH_CURRENT_NAME(state));\
		val =  m_options.hasOption(name) ? VRayForHoudini::UT_Options_getter<VFH_CURRENT_TYPE(state)>(m_options, name) : VFH_CURRENT_DEFAULT(state);\
	}\
	void VFH_TOKENIZE2(set_, VFH_CURRENT_NAME(state))(const VFH_CURRENT_TYPE(state) & val) {\
		VRayForHoudini::UT_Options_setter<VFH_CURRENT_TYPE(state)>(m_options, VFH_STRINGIZE(VFH_CURRENT_NAME(state)), val);\
	}\
	exint VFH_TOKENIZE3(get_, VFH_CURRENT_NAME(state), _size)() const {\
		const char * name = VFH_STRINGIZE(VFH_CURRENT_NAME(state));\
		return m_options.hasOption(name) ? VRayForHoudini::UT_Options_getter<VFH_CURRENT_TYPE(state)>(m_options, name).size() : 0;\
	}

// generates register call for the current paramter
// use the passed class_name to find the appropriate setter and getter
#define VFH_REGISTERS(r, state)\
	registerIntrinsic(\
		VFH_STRINGIZE(VFH_CURRENT_NAME(state)),\
		PackedImplGetterCast<VFH_PARAMS_CLASS(state)>(this, & VFH_PARAMS_CLASS(state) :: VFH_TOKENIZE2(get_, VFH_CURRENT_NAME(state))),\
		PackedImplSetterCast<VFH_PARAMS_CLASS(state)>(this, & VFH_PARAMS_CLASS(state) :: VFH_TOKENIZE2(set_, VFH_CURRENT_NAME(state))));

#define VFH_REGISTERS_TUPLE(r, state)\
	registerTupleIntrinsic(\
		VFH_STRINGIZE(VFH_CURRENT_NAME(state)),\
		PackedImplGetterCast<VFH_PARAMS_CLASS(state)>(this, & VFH_PARAMS_CLASS(state) :: VFH_TOKENIZE3(get_, VFH_CURRENT_NAME(state), _size)),\
		PackedImplGetterCast<VFH_PARAMS_CLASS(state)>(this, & VFH_PARAMS_CLASS(state) :: VFH_TOKENIZE2(_get_, VFH_CURRENT_NAME(state))),\
		PackedImplSetterCast<VFH_PARAMS_CLASS(state)>(this, & VFH_PARAMS_CLASS(state) :: VFH_TOKENIZE2(set_, VFH_CURRENT_NAME(state))));

#define VFH_MAKE_REGISTERS(params, params_count, class_name) BOOST_PP_FOR((0, params, params_count, class_name), VFH_PRED, VFH_OP, VFH_REGISTERS)
#define VFH_MAKE_REGISTERS_TUPLE(params, params_count, class_name) BOOST_PP_FOR((0, params, params_count, class_name), VFH_PRED, VFH_OP, VFH_REGISTERS_TUPLE)
#define VFH_MAKE_ACCESSORS(params, params_count) BOOST_PP_FOR((0, params, params_count),  VFH_PRED, VFH_OP, VFH_ACCESSORS)
#define VFH_MAKE_ACCESSORS_TUPLE(params, params_count) BOOST_PP_FOR((0, params, params_count),  VFH_PRED, VFH_OP, VFH_ACCESSORS_TUPLE)

// How to use:
// Define paramters type: #define params ( (type1, name1, default1), (type2, name2, default2)...)
// Currently supported types are (exint, fpreal, const char *)
// Define parameters count: #define params_count 2

// Define your detail class C derived from GU_PackedImpl, declare UT_Options m_options, member and call VFH_MAKE_ACCESSORS(params, params_count) inside public part
// this will generate getters (returining default if not previously set) and setters for all params

// Define factory class derived from GU_PackedFactory, and call VFH_MAKE_REGISTERS(params, params_count, C) in factory's ctor
// this will register all parameters with the appropriate getters and setters from C


#endif // VRAY_FOR_HOUDINI_VRAY_PRIMITIVES_H
