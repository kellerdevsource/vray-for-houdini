//
// Copyright (c) 2015-2016, Chaos Software Ltd
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

#define VFH_STRINGIZE_HELPER(name) #name
#define VFH_STRINGIZE(name) VFH_STRINGIZE_HELPER(name)
#define VFH_TOKENIZE2_HELPER(a, b) a ## b
#define VFH_TOKENIZE2(a, b) VFH_TOKENIZE2_HELPER(a, b)

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
		const char * _name = VFH_STRINGIZE(VFH_CURRENT_NAME(state));\
		return m_options.hasOption(_name) ? _get_opt<VFH_CURRENT_TYPE(state)>(_name) : VFH_CURRENT_DEFAULT(state);\
	} \
	void VFH_TOKENIZE2(set_, VFH_CURRENT_NAME(state))(const VFH_CURRENT_TYPE(state) & val) {\
		_set_opt<VFH_CURRENT_TYPE(state)>(VFH_STRINGIZE(VFH_CURRENT_NAME(state)), val);\
	}

// generates register call for the current paramter
// usse the passed class_name to find the appropriate setter and getter
#define VFH_REGISTERS(r, state)\
	registerIntrinsic(\
			VFH_STRINGIZE(VFH_CURRENT_NAME(state)),\
			_GetterCast(& VFH_PARAMS_CLASS(state) :: VFH_TOKENIZE2(get_, VFH_CURRENT_NAME(state))),\
			_SetterCast(& VFH_PARAMS_CLASS(state) :: VFH_TOKENIZE2(set_, VFH_CURRENT_NAME(state))) \
			);

// defines member UT_Option m_options which is used to store all params, updating the value there will affect the intrinsics
class VRayPackedImplBase: public GU_PackedImpl {
protected:
	VRayPackedImplBase(): GU_PackedImpl() {}
	VRayPackedImplBase(const VRayPackedImplBase &src): GU_PackedImpl(src) {}

	UT_Options m_options;
protected:
	// getters
	template <typename T> void _set_opt(const char * name, const T & val) { static_assert(false, "UNIMPLEMENTED _set_opt in VRayPackedImplBase"); }

	template <> void _set_opt<exint>          (const char * name, const exint  & val)          { m_options.setOptionI(name, val); }
	template <> void _set_opt<fpreal>         (const char * name, const fpreal & val)          { m_options.setOptionF(name, val); }
	template <> void _set_opt<UT_StringHolder>(const char * name, const UT_StringHolder & val) { m_options.setOptionS(name, val); }

	// setters
	template <typename T> T _get_opt(const char * name) const { static_assert(false, "UNIMPLEMENTED _get_opt in VRayPackedImplBase"); }

	template <> exint           _get_opt<exint>          (const char * name) const { return m_options.getOptionI(name); }
	template <> fpreal          _get_opt<fpreal>         (const char * name) const { return m_options.getOptionF(name); }
	template <> UT_StringHolder _get_opt<UT_StringHolder>(const char * name) const { return m_options.getOptionS(name); }
};

#define VFH_MAKE_REGISTERS(params, params_count, class_name) BOOST_PP_FOR((0, params, params_count, class_name), VFH_PRED, VFH_OP, VFH_REGISTERS)

#define VFH_DEFINE_FACTORY_BASE(factory_name, prim_name, params, params_count)\
class factory_name: public GU_PackedFactory {\
public:\
	factory_name(const char *name, const char *label, const char *icon=NULL): GU_PackedFactory(name, label, icon) {\
		VFH_MAKE_REGISTERS(params, params_count, prim_name)\
	} \
protected:\
	/* if we call an overload that is not implemeted this will get called and print error */\
	template <typename T, typename Q>\
	Q _SetterCast(void (GU_PackedImpl::*method)(T)) {\
		static_assert(false, "VRAY UNIMPLEMENTED _SetterCast in VRayPackedFactoryBase");\
	}\
	/* setters */\
	GU_PackedImpl::IntSetter _SetterCast(void (prim_name::*method)(GA_Size)) {\
		return IntSetterCast(method);\
	}\
	\
	GU_PackedImpl::FloatSetter _SetterCast(void (prim_name::*method)(fpreal)) {\
		return FloatSetterCast(method);\
	}\
	\
	GU_PackedImpl::StringSetter _SetterCast(void (prim_name::*method)(const char *)) {\
		return StringSetterCast(method);\
	}\
	\
	/* getters */\
	GU_PackedImpl::IntGetter _GetterCast(GA_Size (prim_name::*method)() const) {\
		return IntGetterCast(method);\
	}\
	\
	GU_PackedImpl::FloatGetter _GetterCast(fpreal (prim_name::*method)() const) {\
		return FloatGetterCast(method);\
	}\
	\
	GU_PackedImpl::StringGetter _GetterCast(const char * (prim_name::*method)() const) {\
		return StringGetterCast(method);\
	}\
};\

#define VFH_MAKE_ACCESSORS(params, params_count) BOOST_PP_FOR((0, params, params_count),  VFH_PRED, VFH_OP, VFH_ACCESSORS)

// How to use:
// Define paramters tupe: #define params ( (type1, name1, default1), (tpye2, name2, default2)...)
// Currently supported types are (exint, fpreal, const char *)
// Define parameters count: #define params_count 2

// Make class C inherit VRayPackedImplBase and call VFH_MAKE_ACCESSORS(params, params_count) inside public part
// this will generate getters (returining default if not previously set) and setters for all params

// Call VFH_DEFINE_FACTORY_BASE(base_name, C, params, params_count) and inherit class base_name
// the base class will register all parameters with the appropriate getters and setters from C


#endif // VRAY_FOR_HOUDINI_VRAY_PRIMITIVES_H
