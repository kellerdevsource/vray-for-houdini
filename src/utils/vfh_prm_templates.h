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


#include <PRM/PRM_Include.h>

#include <string>
#include <vector>
#include <list>
#include <memory>


class OP_Node;


namespace VRayForHoudini {
namespace Parm {


class AttrDesc;
class PRMFactory;


/// @brief PRM_Template list that is always terminated.
class PRMList
{
public:
	PRMList();

	void                clear();
	bool                empty() const { return (m_prmVec.size() < 2); }
	size_t              size() const { return (m_prmVec.size() - 1); }
	void                reserve(size_t n) { return m_prmVec.reserve(n + 1); }
	PRM_Template*       getPRMTemplate() { return (m_prmVec.size())? m_prmVec.data() : nullptr; }
	const PRM_Template* getPRMTemplate() const { return (m_prmVec.size())? m_prmVec.data() : nullptr; }

	PRMList& addPrm(const PRM_Template &p);
	PRMList& addPrm(const PRMFactory &p);
	PRMList& switcherBegin(const char *token, const char *label = nullptr);
	PRMList& switcherEnd();
	PRMList& addFolder(const std::string &label);

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

	SwitcherInfo* getCurrentSwitcher();
	void          incCurrentFolderPrmCnt();

private:
	PRMTemplVec   m_prmVec;
	SwitcherList  m_switcherList;
	SwitcherStack m_switcherStack;
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
	PRMFactory& setSpareData(const char *items[], int nItems);
	PRMFactory& setSpareData(const PRM_SpareData*);

	/// @brief Specify the list of parameters for each instance of a multiparm.
	/// @note This setting is ignored for non-multiparm parameters.
	/// @note Parameter name tokens should include a '#' character.
	PRMFactory& setMultiparms(const PRMList&);

	PRMFactory& setParmGroup(int);

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
