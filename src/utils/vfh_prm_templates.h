//
// Copyright (c) 2015-2017, Chaos Software Ltd
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
class PRMList;


/// Prepend the passed path with the UI root determined by VRAY_UI_DS_PATH env var
/// if resulting file path doesn't exist searches for the relPath inside UI root
/// does minimal validation if UI root and file path exist
/// @relPath - relative path to the VRAY_UI_DS_PATH env var
/// @return the full path
std::string expandUiPath(const std::string &relPath);
/// Load params from DS file for this plugin if the DS file exists
/// @pluginID - the plugin ID
/// @prmList - list to append params to
/// @return true - arguments were valid
///         false - otherwise
bool addPrmTemplateForPlugin(const std::string &pluginID, Parm::PRMList &prmList);
PRMList* generatePrmTemplate(const std::string &pluginID);
PRM_Template* getPrmTemplate(const std::string &pluginID);


/// List of PRM_Templates which is always terminated
class PRMList
{
public:
	/// Initialize empty but terminated list
	PRMList();
	~PRMList();

	/// Clear all added templated
	void clear();
	/// Check if the list is empty
	bool empty() const { return (m_prmVec.size() < 2); }
	/// Get the number of added templates
	int size() const { return std::max(static_cast<int>(m_prmVec.size())-1, 0); }

	/// Reserve space for templates without actually changin size
	/// @n - the number elements
	void reserve(int n) { return m_prmVec.reserve(n + 1); }

	/// Set the cook dependent flag for all templates in the list
	PRMList& setCookDependent(bool recook);

	/// Find the index for a template by it's token
	/// @token - the param template token to search for
	/// @return >= 0 - if template is found
	///         -1   - otherwise
	int findPRMTemplate(const char *token) const;

	/// Get template by index
	/// @i - the index
	/// @return pointer - to the template on index i
	///         nullptr - @i is out of bounds
	PRM_Template* getPRMTemplate(int i)
	{ return ((i < 0 || i >= size())? nullptr : (m_prmVec.data() + i)); }

	/// Get template by index
	/// @i - the index
	/// @return const pointer - to the template on index i
	///         nullptr - @i is out of bounds
	const PRM_Template* getPRMTemplate(int i) const
	{ return ((i < 0 || i >= size())? nullptr : (m_prmVec.data() + i)); }

	/// Note: Be careful when accessing internal PRM_Template data and passing it around
	/// adding additional parameters to the PRMList after calling PRMList::getPRMTemplate() might cause
	/// internal memory to be reallocated and invalidate all returened pointers

	/// Get raw pointer to the internal PRM_Template array
	PRM_Template* getPRMTemplate() { return m_prmVec.data(); }
	/// Get raw const pointer to the internal PRM_Template array
	const PRM_Template* getPRMTemplate() const { return m_prmVec.data(); }

	/// Get copy of the param template list
	/// NOTE: will copy internal template to heap and return the pointer
	/// this should not be called excessivily as these pointers are not freed
	std::shared_ptr<PRM_Template> getPRMTemplateCopy() const;

	/// Add param template to the end of the list
	/// @p - the template
	/// @return *this
	PRMList& addPrm(const PRM_Template &p);

	/// Add all params from PRMFactory to the end of the list
	/// @p - the template factory
	/// @return *this
	PRMList& addPrm(const PRMFactory &p);

	/// Begin a switcher
	/// NOTE: you must call addFolder() whenever you open a switcher with switcherBegin()
	/// adding parameter to an open switcher with no folders in it will result in runtime error
	/// @token - the switcher token
	/// @label - optional label
	/// @return *this
	PRMList& switcherBegin(const char *token, const char *label = nullptr);

	/// End previously begun switcher
	/// NOTE: you must call switcherEnd() to correctly terminate the switcher parameters scope
	/// otherwise switcher folders will not appear as expected in the UI
	/// @return *this
	PRMList& switcherEnd();

	/// Add folder to current switcher
	/// NOTE: you must call addFolder() whenever you open a switcher with switcherBegin()
	/// adding parameter to an open switcher with no folders in it will result in runtime error
	/// @label - the folder's display label
	/// @return *this
	PRMList& addFolder(const char *label);

	/// Add all param templates from a .ds file
	/// NOTE: when loading parameters from file you MUST keep
	/// the PRMList instance alive as it holds internal references to
	/// PRM_ScriptPages used by the loaded PRM_Templates
	/// @filepath - full path to the .ds file
	/// @return *this
	PRMList& addFromFile(const char *filepath, const char *includePath=nullptr);

	/// Append all templates from a terminated list of templates
	/// NOTE: tmpl should be list terminated array of parameters
	/// @tmpl - array of templates
	/// @return *this
	PRMList& addFromPRMTemplate(const PRM_Template tmpl[]);

	/// Load param templates from ds file and optionally mark them cook dependent
	/// @filepath - absolute path to the .ds file
	/// @cookDependent - if true will set cook dependent to all loaded templates
	/// @return pointer - to array of templates which is terminated
	///         nullptr - on fail
	static PRM_Template* loadFromFile(const char *filepath, bool cookDependent = false);

	/// Set cook dependent flag to all templates in given list
	/// @tmpl - *terminated* list of templates
	/// @recook - value to set cook dependent flag to
	static void setCookDependent(PRM_Template tmpl[], bool recook);

	/// Add prefix to the names of all templates in the given list
	/// @tmpl - *terminated* list of templates
	/// @prefix - the prefix to prepend
	static void renamePRMTemplate(PRM_Template tmpl[], const char *prefix);

private:
	typedef std::vector<PRM_Template> PRMTemplVec;

	/// Descriptor for a switcher with all it's folders
	struct SwitcherInfo
	{
		SwitcherInfo(size_t idx):
			m_parmIdx(idx)
		{ }

		size_t m_parmIdx; ///< The index of this switcher in it's containing template
		std::vector<PRM_Default> m_folders; ///< All folders in this switcher
	};

	typedef std::list<SwitcherInfo> SwitcherList;
	typedef std::vector<SwitcherInfo*> SwitcherStack;
	typedef std::list< std::shared_ptr<PRM_ScriptGroup> > ScriptGroupList;

	/// Get the current switcher
	/// @return pointer - to the current switcher
	///         nullptr - if there is no current switcher
	SwitcherInfo* getCurrentSwitcher();

	/// Change the number of parameters in the current folder
	/// throws std::runtime_error if there no folders
	/// @cnt - the delta to apply to number of parameters (it can be negative)
	void          incCurrentFolderPrmCnt(int cnt);

private:
	PRMTemplVec    m_prmVec; ///< The array of the templates including the terminator
	SwitcherList   m_switcherList; ///< List of all switchers
	SwitcherStack  m_switcherStack; ///< Stack of the current active switchers

	/// Holds script pages, so params have valid references at all times
	/// container will free the pages on destruction
	ScriptGroupList m_scriptGroups;
};

/// Factory class for easier param template creation
/// set* methods return *this so they can be chain called
class PRMFactory
{
public:
	/// Static methods for convinient creation and saving of new templates

	static PRM_Name* createPRMName(const char *thetoken=0, const char *thelabel=0, int theflags=0);
	static PRM_Name* createPRMName(int nCnt);
	static PRM_Default* createPRMDefaut(fpreal thefloat=0, const char *thestring=0, CH_StringMeaning strmeaning=CH_STRING_LITERAL);
	static PRM_Default* createPRMDefaut(int nCnt);
	static PRM_ChoiceList* createPRMChoiceList(PRM_ChoiceListType thetype, PRM_Name *thechoicenames);
	static PRM_ChoiceList* createPRMChoiceList(PRM_ChoiceListType thetype, PRM_ChoiceGenFunc thefunc);
	static PRM_Range* createPRMRange(PRM_RangeFlag theminflag=PRM_RANGE_UI, fpreal themin=0, PRM_RangeFlag themaxflag=PRM_RANGE_UI, fpreal themax=1);
	static PRM_SpareData* createPRMSpareData();
	static PRM_SpareData* createPRMSpareData(const char *thetoken, const char *thevalue);

public:
	PRMFactory();
	explicit PRMFactory(const PRM_Type &type, const char *token, const char *label=0);
	explicit PRMFactory(const PRM_Type &type, const std::string &token, const std::string &label);
	explicit PRMFactory(const PRM_MultiType &multiType, const char *token, const char *label=0);
	explicit PRMFactory(const PRM_MultiType &multiType, const std::string &token, const std::string &label);

	/// Settings

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

	/// Specify the number of vector elements for this parameter.
	/// The default vector size is one element.
	PRMFactory& setVectorSize(int size);

	/// Specify a default value for this parameter.
	/// If the string is null, the floating-point value will be used
	/// (but rounded if this parameter is integer-valued).
	/// NOTE: The string pointer must not point to a temporary.
	PRMFactory& setDefault(fpreal f, const char* s=nullptr, CH_StringMeaning meaning=CH_STRING_LITERAL);
	PRMFactory& setDefault(const std::string&, CH_StringMeaning = CH_STRING_LITERAL);
	PRMFactory& setDefault(const PRM_Default *d);
	PRMFactory& setDefaults(const int items[], int nItems);
	PRMFactory& setDefaults(const fpreal items[], int nItems);

	PRMFactory& setCallbackFunc(const PRM_Callback&);

	/// Specify a menu type and a list of token, label, token, label,... pairs
	/// for this parameter.
	/// @param type specifies the menu behavior (toggle, replace, etc.)
	/// @param items a list of token, label, token, label,... string pairs
	/// @param nItems size of the array
	PRMFactory& setChoiceListItems(PRM_ChoiceListType type, const char *items[], int nItems);
	PRMFactory& setChoiceList(const PRM_ChoiceList *choiceList);

	/// Specify a range for this parameter's values.
	PRMFactory& setRange(PRM_RangeFlag minFlag, fpreal minVal, PRM_RangeFlag maxFlag, fpreal maxVal);
	PRMFactory& setRange(const PRM_Range*);

	/// Specify (@e key, @e value) pairs of spare data for this parameter.
	/// @param items a list of key, value, key, value,... string pairs
	/// @param nItems size of the array
	PRMFactory& addSpareData(const char *token, const char* value);
	PRMFactory& setSpareData(const PRM_SpareData*);

	/// Specify the list of parameters for each instance of a multiparm.
	/// @note This setting is ignored for non-multiparm parameters.
	/// @note Parameter name tokens should include a '#' character.
	PRMFactory& setMultiparms(const PRM_Template tmpl[]);

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

/// Check if param exists in the node with the attrName
/// @node - the node which parameters will be searched
/// @attrName - the name of param we are searching for
/// return 1 - the param exists in this node
///        0 - otherwise
int isParmExist(const OP_Node &node, const std::string &attrName);

/// Check if the param at index is a switcher
/// @node - the node which parameters will be checked
/// @index - the index of the checked param
/// @return 1 - the param is switcher
///         0 - otherwise
int isParmSwitcher(const OP_Node &node, const int index);

/// Get param template from a node by index
/// @node - the node which parameters will be accessed
/// @index - the index of the param template
/// @return pointer - to the param template
///         nullptr - if invalid index provided
const PRM_Parm *getParm(const OP_Node &node, const int index);

/// Get param template from a node by name
/// @node - the node which parameters will be accessed
/// @index - the name of the param template
/// @return pointer - to the param template
///         nullptr - if template with that name is not found
const PRM_Parm *getParm(const OP_Node &node, const std::string &attrName);

/// Find and eval node's param as int
/// @node - the node which will be used for eval
/// @attrName - the name of the param
/// @t - the time at which to eval at
/// @return the value of the param
int getParmInt(const OP_Node &node, const std::string &attrName, fpreal t=0.0);

/// Find and eval node's param as float
/// @node - the node which will be used for eval
/// @attrName - the name of the param
/// @t - the time at which to eval at
/// @return the value of the param
float getParmFloat(const OP_Node &node, const std::string &attrName, fpreal t=0.0);

/// Find and eval node's param as enum
/// @node - the node which will be used for eval
/// @attrDesc - to check the enum value
/// @attrName - the name of the param
/// @t - the time at which to eval at
/// @return the value of the param
int getParmEnumExt(const OP_Node &node, const AttrDesc &attrDesc, const std::string &attrName, fpreal t=0.0);

} // namespace Parm
} // namespace VRayForHoudini

#endif
