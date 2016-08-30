//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PRM_TEMPLATES_H
#define VRAY_FOR_HOUDINI_PRM_TEMPLATES_H

#include "vfh_prm_def.h"

#include <PRM/PRM_Include.h>

#include <string>
#include <vector>
#include <list>
#include <memory>


class OP_Node;
class PRM_ScriptGroup;


namespace VRayForHoudini {
namespace Parm {


class PRMFactory;


/// @brief PRM_Template list that is always terminated.
class PRMList
{
public:
	PRMList();
	~PRMList();

	void                clear();
	bool                empty() const { return (m_prmVec.size() < 2); }
	int                 size() const { return std::max(static_cast<int>(m_prmVec.size())-1, 0); }
	void                reserve(int n) { return m_prmVec.reserve(n + 1); }

	PRMList&            setCookDependent(bool recook);

	int                 findPRMTemplate(const char *token) const;
	PRM_Template*       getPRMTemplate(int i)
	{ return ((i < 0 || i >= size())? nullptr : (m_prmVec.data() + i)); }
	const PRM_Template* getPRMTemplate(int i) const
	{ return ((i < 0 || i >= size())? nullptr : (m_prmVec.data() + i)); }
	// NOTE: use following 2 methods with causion
	// be careful when accessing internal PRM_Template data and passing it around
	// adding additional parameters to the PRMList after calling PRMList::getPRMTemplate() might cause
	// m_prmVec to be resized and invalidate the returened pointers
	PRM_Template*       getPRMTemplate() { return m_prmVec.data(); }
	const PRM_Template* getPRMTemplate() const { return m_prmVec.data(); }
	// NOTE: will copy internal template to heap and return the pointer
	// this should not be called excessivily as these pointers are not freed
	std::shared_ptr<PRM_Template> getPRMTemplateCopy() const;


	PRMList& addPrm(const PRM_Template &p);
	PRMList& addPrm(const PRMFactory &p);
	// NOTE: you must call addFolder() whenever you open a switcher with switcherBegin()
	// adding parameter to an open switcher with no folders in it will result in runtime error
	PRMList& switcherBegin(const char *token, const char *label = nullptr);
	// NOTE: you must call switcherEnd() to correctly terminate the switcher parameters scope
	// otherwise switcher folders will not appear as expected in the UI
	PRMList& switcherEnd();
	// NOTE: you must call addFolder() whenever you open a switcher with switcherBegin()
	// adding parameter to an open switcher with no folders in it will result in runtime error
	PRMList& addFolder(const char *label);
	// NOTE: when loading parameters from file you MUST keep
	// the PRMList instance alive as it holds internal references to
	// PRM_ScriptPages used by the loaded PRM_Templates
	PRMList& addFromFile(const char *filepath);
	// NOTE: tmpl should be list terminated array of parameters
	PRMList& addFromPRMTemplate(const PRM_Template tmpl[]);


	// prepends the passed path with the UI root determined by VRAY_UI_DS_PATH env var
	// if resulting file path doesn't exist searches for the relPath inside UI root
	// does minimal validation if UI root and file path exist
	static std::string   expandUiPath(const std::string &relPath);
	static std::string   getUIPluginPath(const char *pluginName);
	static PRM_Template* loadFromFile(const char *filepath, bool cookDependent = false);
	// NOTE: tmpl should be list terminated array of parameters
	static void          setCookDependent(PRM_Template tmpl[], bool recook);
	// NOTE: tmpl should be list terminated array of parameters
	static void          renamePRMTemplate(PRM_Template tmpl[], const char *prefix);

private:
	typedef std::vector<PRM_Template> PRMTemplVec;

	struct SwitcherInfo
	{
		SwitcherInfo(size_t idx):
			m_parmIdx(idx)
		{ }

		size_t m_parmIdx;
		std::vector<PRM_Default> m_folders;
	};

	typedef std::list<SwitcherInfo> SwitcherList;
	typedef std::vector<SwitcherInfo*> SwitcherStack;
	typedef std::list< std::shared_ptr<PRM_ScriptGroup> > ScriptGroupList;

	SwitcherInfo* getCurrentSwitcher();
	void          incCurrentFolderPrmCnt(int cnt);

private:
	PRMTemplVec    m_prmVec;
	SwitcherList   m_switcherList;
	SwitcherStack  m_switcherStack;
	// will hold script pages, so params have valid references at all times
	// container will free the pages on destruction
	ScriptGroupList m_scriptGroups;
};


class PRMFactory
{
public:
	static PRM_Name*       createPRMName(const char *thetoken=0, const char *thelabel=0, int theflags=0);
	static PRM_Name*       createPRMName(int nCnt);
	static PRM_Default*    createPRMDefaut(fpreal thefloat=0, const char *thestring=0, CH_StringMeaning strmeaning=CH_STRING_LITERAL);
	static PRM_Default*    createPRMDefaut(int nCnt);
	static PRM_ChoiceList* createPRMChoiceList(PRM_ChoiceListType thetype, PRM_Name *thechoicenames);
	static PRM_ChoiceList* createPRMChoiceList(PRM_ChoiceListType thetype, PRM_ChoiceGenFunc thefunc);
	static PRM_Range*      createPRMRange(PRM_RangeFlag theminflag=PRM_RANGE_UI, fpreal themin=0, PRM_RangeFlag themaxflag=PRM_RANGE_UI, fpreal themax=1);
	static PRM_SpareData*  createPRMSpareData();
	static PRM_SpareData*  createPRMSpareData(const char *thetoken, const char *thevalue);

public:
	PRMFactory();
	explicit PRMFactory(const PRM_Type &type, const char *token, const char *label=0);
	explicit PRMFactory(const PRM_Type &type, const std::string &token, const std::string &label);
	explicit PRMFactory(const PRM_MultiType &multiType, const char *token, const char *label=0);
	explicit PRMFactory(const PRM_MultiType &multiType, const std::string &token, const std::string &label);

	// Settings
	/// Specify type for this parameter.
	const PRM_Type& getType() const;
	PRMFactory& setType(const PRM_Type &type);
	PRMFactory& setType(const PRM_MultiType &multiType);

	/// Specify name and label for this parameter.
	PRMFactory& setName(const char *token, const char *label=0);
	PRMFactory& setName(const std::string &token, const std::string &label);

	/// Specify an extended type for this parameter.
	PRMFactory& setTypeExtended(const PRM_TypeExtended &type);

	/// Specify export level for this parameter.
	PRMFactory& setExportLevel(const PRM_Template::PRM_Export &exportLevel);

	/// @brief Specify the number of vector elements for this parameter.
	/// @details (The default vector size is one element.)
	PRMFactory& setVectorSize(int size);

	/// @brief Specify a default value for this parameter.
	/// @details If the string is null, the floating-point value will be used
	/// (but rounded if this parameter is integer-valued).
	/// @note The string pointer must not point to a temporary.
	PRMFactory& setDefault(fpreal f, const char* s=nullptr, CH_StringMeaning meaning=CH_STRING_LITERAL);
	PRMFactory& setDefault(const std::string&, CH_StringMeaning = CH_STRING_LITERAL);
	PRMFactory& setDefault(const PRM_Default *d);
	PRMFactory& setDefaults(const int items[], int nItems);
	PRMFactory& setDefaults(const fpreal items[], int nItems);

	PRMFactory& setCallbackFunc(const PRM_Callback&);

	/// @brief Specify a menu type and a list of token, label, token, label,... pairs
	/// for this parameter.
	/// @param type specifies the menu behavior (toggle, replace, etc.)
	/// @param items a list of token, label, token, label,... string pairs
	/// @param nItems size of the array
	PRMFactory& setChoiceListItems(PRM_ChoiceListType type, const char *items[], int nItems);
	PRMFactory& setChoiceList(const PRM_ChoiceList *choiceList);

	/// Specify a range for this parameter's values.
	PRMFactory& setRange(PRM_RangeFlag minFlag, fpreal minVal, PRM_RangeFlag maxFlag, fpreal maxVal);
	PRMFactory& setRange(const PRM_Range*);

	/// @brief Specify (@e key, @e value) pairs of spare data for this parameter.
	/// @param items a list of key, value, key, value,... string pairs
	/// @param nItems size of the array
	PRMFactory& addSpareData(const char *token, const char* value);
	PRMFactory& setSpareData(const PRM_SpareData*);

	/// @brief Specify the list of parameters for each instance of a multiparm.
	/// @note This setting is ignored for non-multiparm parameters.
	/// @note Parameter name tokens should include a '#' character.
	PRMFactory& setMultiparms(const PRM_Template *);

	PRMFactory& setParmGroup(int);

	PRMFactory& setInvisible(bool v);

	PRMFactory& addConditional(const char *conditional, PRM_ConditionalType type = PRM_CONDTYPE_DISABLE);

	PRMFactory& setHelpText(const char*);

	/// Construct and return the parameter template.
	PRM_Template getPRMTemplate() const;

private:
	struct PImplPRM;
	std::shared_ptr<PImplPRM> m_prm;
};


int isParmExist(const OP_Node &node, const std::string &attrName);
int isParmSwitcher(const OP_Node &node, const int index);

const PRM_Parm *getParm(const OP_Node &node, const int index);
const PRM_Parm *getParm(const OP_Node &node, const std::string &attrName);

int    getParmInt(const OP_Node &node, const std::string &attrName, fpreal t=0.0);
float  getParmFloat(const OP_Node &node, const std::string &attrName, fpreal t=0.0);

int    getParmEnumExt(const OP_Node &node, const AttrDesc &attrDesc, const std::string &attrName, fpreal t=0.0);

} // namespace Parm
} // namespace VRayForHoudini

#endif
