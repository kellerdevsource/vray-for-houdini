//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_prm_templates.h"
#include "vfh_defines.h"
#include "vfh_prm_def.h"

#include <OP/OP_Node.h>


using namespace VRayForHoudini;


int VRayForHoudini::Parm::isParmExist(const OP_Node &node, const std::string &attrName)
{
	int parmExist = false;

	const PRM_ParmList *parmList = node.getParmList();
	if (parmList) {
		const PRM_Parm *param = parmList->getParmPtr(attrName.c_str());
		if (param) {
			parmExist = true;
		}
	}

	return parmExist;
}


int VRayForHoudini::Parm::isParmSwitcher(const OP_Node &node, const int index)
{
	int isSwitcher = false;

	const PRM_ParmList *parmList = node.getParmList();
	if (parmList) {
		const PRM_Parm *param = parmList->getParmPtr(index);
		if (param) {
			isSwitcher = param->getType().isSwitcher();
		}
	}

	return isSwitcher;
}


const PRM_Parm* VRayForHoudini::Parm::getParm(const OP_Node &node, const int index)
{
	const PRM_Parm *param = nullptr;

	const PRM_ParmList *parmList = node.getParmList();
	if (parmList) {
		param = parmList->getParmPtr(index);
	}

	return param;
}


const PRM_Parm *VRayForHoudini::Parm::getParm(const OP_Node &node, const std::string &attrName)
{
	const PRM_Parm *param = nullptr;

	const PRM_ParmList *parmList = node.getParmList();
	if (parmList) {
		param = parmList->getParmPtr(attrName.c_str());
	}

	return param;
}


int VRayForHoudini::Parm::getParmInt(const OP_Node &node, const std::string &attrName, fpreal t)
{
	int value = 0;

	if (isParmExist(node, attrName)) {
		value = node.evalInt(attrName.c_str(), 0, t);
	}

	return value;
}


float VRayForHoudini::Parm::getParmFloat(const OP_Node &node, const std::string &attrName, fpreal t)
{
	float value = 0.0f;

	if (isParmExist(node, attrName)) {
		value = node.evalFloat(attrName.c_str(), 0, t);
	}

	return value;
}


int VRayForHoudini::Parm::getParmEnumExt(const OP_Node &node, const VRayForHoudini::Parm::AttrDesc &attrDesc, const std::string &attrName, fpreal t)
{
	int value = node.evalInt(attrName.c_str(), 0, t);

	if (value < attrDesc.value.defEnumItems.size()) {
		const Parm::EnumItem &enumItem = attrDesc.value.defEnumItems[value];
		if (enumItem.valueType == Parm::EnumItem::EnumValueInt) {
			value = enumItem.value;
		}
	}

	return value;
}


/////////                         VfhPRMList definition
///
///
Parm::PRMList::PRMList():
	m_prmVec(1)
{
	// NOTE: extra item is list terminator
}


void Parm::PRMList::clear()
{
	m_switcherList.clear();
	m_switcherStack.clear();
	m_prmVec.clear();
	// NOTE: extra item is list terminator
	m_prmVec.emplace_back();
}


Parm::PRMList& Parm::PRMList::addPrm(const PRM_Template& p)
{
	m_prmVec.back() = p;
	m_prmVec.emplace_back();
	incCurrentFolderPrmCnt();
	return *this;
}


Parm::PRMList& Parm::PRMList::addPrm(const PRMFactory& p)
{
	m_prmVec.back() = p.getPRMTemplate();
	m_prmVec.emplace_back();
	incCurrentFolderPrmCnt();
	return *this;
}


Parm::PRMList& Parm::PRMList::switcherBegin(const char *token, const char *label)
{
	// add new switcher info to our list with NO default folders
	m_switcherList.emplace_back(m_prmVec.size() - 1);
	// add the switcher parameter
	addPrm(PRMFactory(PRM_SWITCHER, token, label));
	// push our new switcher onto the stack
	m_switcherStack.push_back(&m_switcherList.back());
	return *this;
}


Parm::PRMList& Parm::PRMList::switcherEnd()
{
	SwitcherInfo* info = getCurrentSwitcher();
	if (NOT(info)) {
		throw std::runtime_error("endSwitcher() called with no corresponding beginSwitcher()");
	}
	else {
		if (info->m_folders.empty()) {
			throw std::runtime_error("added switcher with no folders");
		}
		else {
			// NOTE: extra item is list terminator
			info->m_folders.emplace_back();
			// set correct folder count and folder info on
			// the current switcher parameter (i.e the one created with last beginSwitcher())
			PRM_Template& switcherParm = m_prmVec[info->m_parmIdx];
			switcherParm.assign(switcherParm, info->m_folders.size(), &info->m_folders.front());
		}
		m_switcherStack.pop_back();
	}

	return *this;
}


Parm::PRMList& Parm::PRMList::addFolder(const std::string& label)
{
	SwitcherInfo *info = getCurrentSwitcher();
	if (NOT(info)) {
		throw std::runtime_error("folder added to nonexistent switcher");
	}
	else {
		info->m_folders.emplace_back(/*numParms=*/0, ::strdup(label.c_str()));
	}

	return *this;
}


Parm::PRMList::SwitcherInfo* Parm::PRMList::getCurrentSwitcher()
{
	SwitcherInfo *info = nullptr;
	if (NOT(m_switcherStack.empty())) {
		info = m_switcherStack.back();
	}
	return info;
}


void Parm::PRMList::incCurrentFolderPrmCnt()
{
	SwitcherInfo *info = getCurrentSwitcher();
	if (NOT(info)) {
		return;
	}

	if (info->m_folders.empty()) {
		throw std::runtime_error("parameter added to switcher with no folders");
	} else {
		// If a parameter is added to this ParmList while a switcher with at least
		// one folder is active, increment the folder's parameter count.
		PRM_Default& def = info->m_folders.back();
		def.setOrdinal(def.getOrdinal() + 1);
	}

}


/////////                         ParmFactory definition
///

std::list< PRM_Name* >       PRMNameList;
std::list< PRM_Default* >    PRMDefaultList;
std::list< PRM_SpareData* >  PRMSpareDataList;
std::list< PRM_ChoiceList* > PRMChoiceListList;
std::list< PRM_Range* >      PRMRangeList;


PRM_Name* Parm::PRMFactory::createPRMName(const char *thetoken, const char *thelabel, int theflags)
{
	PRM_Name *val = new PRM_Name(thetoken, thelabel, theflags);
	PRMNameList.push_back(val);
	return val;
}


PRM_Name* Parm::PRMFactory::createPRMName(int nCnt)
{
	if (nCnt <= 0) {
		return nullptr;
	}

	PRM_Name *val = new PRM_Name[nCnt]();
	PRMNameList.push_back(val);
	return val;
}


PRM_Default* Parm::PRMFactory::createPRMDefaut(fpreal thefloat, const char *thestring, CH_StringMeaning strmeaning)
{
	PRM_Default *val = new PRM_Default(thefloat, thestring, strmeaning);
	PRMDefaultList.push_back(val);
	return val;
}


PRM_Default* Parm::PRMFactory::createPRMDefaut(int nCnt)
{
	if (nCnt <= 0) {
		return nullptr;
	}

	PRM_Default *val = new PRM_Default[nCnt]();
	PRMDefaultList.push_back(val);
	return val;
}


PRM_ChoiceList* Parm::PRMFactory::createPRMChoiceList(PRM_ChoiceListType thetype, PRM_Name *thechoicenames)
{
	PRM_ChoiceList *val = new PRM_ChoiceList(thetype, thechoicenames);
	PRMChoiceListList.push_back(val);
	return val;
}


PRM_ChoiceList* Parm::PRMFactory::createPRMChoiceList(PRM_ChoiceListType thetype, PRM_ChoiceGenFunc thefunc)
{
	PRM_ChoiceList *val = new PRM_ChoiceList(thetype, thefunc);
	PRMChoiceListList.push_back(val);
	return val;
}


PRM_Range* Parm::PRMFactory::createPRMRange(PRM_RangeFlag theminflag, fpreal themin, PRM_RangeFlag themaxflag, fpreal themax)
{
	PRM_Range *val = new PRM_Range(theminflag, themin, themaxflag, themax);
	PRMRangeList.push_back(val);
	return val;
}


PRM_SpareData* Parm::PRMFactory::createPRMSpareData()
{
	PRM_SpareData *val = new PRM_SpareData();
	PRMSpareDataList.push_back(val);
	return val;
}


PRM_SpareData* Parm::PRMFactory::createPRMSpareData(const char *thetoken, const char *thevalue)
{
	PRM_SpareData *val = new PRM_SpareData(PRM_SpareToken(thetoken, thevalue));
	PRMSpareDataList.push_back(val);
	return val;
}


struct Parm::PRMFactory::PImplPRM
{
	PImplPRM():
		type(PRM_LIST_TERMINATOR),
		typeExtended(PRM_TYPE_NONE),
		exportLevel(PRM_Template::PRM_EXPORT_MIN),
		multiType(PRM_MULTITYPE_NONE),
		multiparms(nullptr),
		name(nullptr),
		defaults(PRMzeroDefaults),
		vectorSize(1),
		parmGroup(0),
		range(nullptr),
		choicelist(nullptr),
		callbackFunc(0),
		spareData(nullptr),
		conditional(nullptr),
		helpText(nullptr)
	{ }


	PImplPRM(const char *token, const char *label):
		type(PRM_LIST_TERMINATOR),
		typeExtended(PRM_TYPE_NONE),
		exportLevel(PRM_Template::PRM_EXPORT_MIN),
		multiType(PRM_MULTITYPE_NONE),
		multiparms(nullptr),
		name(nullptr),
		defaults(PRMzeroDefaults),
		vectorSize(1),
		parmGroup(0),
		range(nullptr),
		choicelist(nullptr),
		callbackFunc(0),
		spareData(nullptr),
		conditional(nullptr),
		helpText(nullptr)
	{
		setName(token, label);
	}


	PImplPRM& setName(const char *token, const char *label)
	{
		PRM_Name *name = createPRMName(token, label);
		name->harden();
		this->name = name;

		return *this;
	}

	PRM_Type                   type;
	PRM_TypeExtended           typeExtended;
	PRM_Template::PRM_Export   exportLevel;
	PRM_MultiType              multiType;
	const PRM_Template*        multiparms;
	const PRM_Name*            name;
	const PRM_Default*         defaults;
	int                        vectorSize;
	int                        parmGroup;
	const PRM_Range*           range;
	const PRM_ChoiceList*      choicelist;
	PRM_Callback               callbackFunc;
	const PRM_SpareData*       spareData;
	PRM_ConditionalGroup*      conditional;
	const char*                helpText;
};


Parm::PRMFactory::PRMFactory():
	m_prm(new PImplPRM())
{ }


Parm::PRMFactory::PRMFactory(const PRM_Type &type, const char *token, const char *label):
	m_prm(new PImplPRM(token, label))
{
	m_prm->type = type;
}


Parm::PRMFactory::PRMFactory(const PRM_Type &type, const std::string &token, const std::string &label):
	m_prm(new PImplPRM(token.c_str(), label.c_str()))
{
	m_prm->type = type;
}


Parm::PRMFactory::PRMFactory(const PRM_MultiType &multiType, const char *token, const char *label):
	m_prm(new PImplPRM(token, label))
{
	m_prm->multiType = multiType;
}


Parm::PRMFactory::PRMFactory(const PRM_MultiType &multiType, const std::string &token, const std::string &label):
	m_prm(new PImplPRM(token.c_str(), label.c_str()))
{
	m_prm->multiType = multiType;
}


const PRM_Type& Parm::PRMFactory::getType() const
{
	return m_prm->type;
}


Parm::PRMFactory& Parm::PRMFactory::setType(const PRM_Type &type)
{
	m_prm->type = type;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setType(const PRM_MultiType &multiType)
{
	m_prm->multiType = multiType;
	m_prm->type = PRM_TYPE_BASIC_TYPE;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setName(const char *token, const char *label)
{
	m_prm->setName(token, label);
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setName(const std::string &token, const std::string &label)
{
	m_prm->setName(token.c_str(), label.c_str());
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setTypeExtended(const PRM_TypeExtended &type)
{
	m_prm->typeExtended = type;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setExportLevel(const PRM_Template::PRM_Export &exportLevel)
{
	m_prm->exportLevel = exportLevel;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setVectorSize(int size)
{
	m_prm->vectorSize = size;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setDefault(fpreal f, const char *s, CH_StringMeaning meaning)
{
	m_prm->defaults = createPRMDefaut(f, s, meaning);
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setDefault(const std::string &s, CH_StringMeaning meaning)
{
	m_prm->defaults = createPRMDefaut(0.0, ::strdup(s.c_str()), meaning);
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setDefault(const PRM_Default* d)
{
	m_prm->defaults = d;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setDefaults(const int items[], int nItems)
{
	if (nItems <= 0) {
		return *this;
	}

	PRM_Default *defaults = createPRMDefaut(nItems);
	for (int i = 0; i < nItems; ++i) {
		defaults[i].setOrdinal(items[i]);
	}

	m_prm->defaults = defaults;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setDefaults(const fpreal items[], int nItems)
{
	if (nItems <= 0) {
		return *this;
	}

	PRM_Default *defaults = createPRMDefaut(nItems);
	for (int i = 0; i < nItems; ++i) {
		defaults[i].setFloat(items[i]);
	}

	m_prm->defaults = defaults;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setCallbackFunc(const PRM_Callback& f)
{
	m_prm->callbackFunc = f;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setChoiceListItems(PRM_ChoiceListType type, const char *items[], int nItems)
{
	if (nItems <= 0) {
		return *this;
	}

	// items come in pairs { token1, label1, token2, label2, ...}
	const int nCnt = nItems >> 1;
	// NOTE: extra item is list terminator
	PRM_Name *copyOfItems = createPRMName(nCnt + 1);
	for (int i = 0, n = 0; i < nItems; ++n, i += 2) {
		copyOfItems[n].setToken(items[i]);
		copyOfItems[n].setLabel(items[i+1]);
		copyOfItems[n].harden();
	}

	m_prm->choicelist = createPRMChoiceList(type, copyOfItems);
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setChoiceList(const PRM_ChoiceList *choiceList)
{
	m_prm->choicelist = choiceList;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setRange(PRM_RangeFlag minFlag, fpreal minVal, PRM_RangeFlag maxFlag, fpreal maxVal)
{
	m_prm->range = createPRMRange(minFlag, minVal, maxFlag, maxVal);
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setRange(const PRM_Range* r)
{
	m_prm->range = r;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setSpareData(const char *items[], int nItems)
{
	if (nItems <= 0) {
		return *this;
	}

	PRM_SpareData* spareData = createPRMSpareData();
	for (int i = 0; i < nItems; i += 2) {
		spareData->addTokenValue(items[i],items[i+1]);
	}

	m_prm->spareData = spareData;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setSpareData(const PRM_SpareData* d)
{
	m_prm->spareData = d;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setMultiparms(const PRMList& p)
{
	m_prm->multiparms = p.getPRMTemplate();
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setParmGroup(int n)
{
	m_prm->parmGroup = n;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::addConditional(const char *conditional, PRM_ConditionalType type)
{
	if (NOT(m_prm->conditional)) {
		m_prm->conditional = new PRM_ConditionalGroup();
	}
	m_prm->conditional->addConditional(conditional, type);
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setHelpText(const char* t)
{
	m_prm->helpText = t;
	return *this;
}


PRM_Template Parm::PRMFactory::getPRMTemplate() const
{
	if (m_prm->multiType != PRM_MULTITYPE_NONE) {
		return PRM_Template(
			m_prm->multiType,
			const_cast<PRM_Template*>(m_prm->multiparms),
			m_prm->exportLevel,
			fpreal(m_prm->vectorSize),
			const_cast<PRM_Name*>(m_prm->name),
			const_cast<PRM_Default*>(m_prm->defaults),
			const_cast<PRM_Range*>(m_prm->range),
			const_cast<PRM_SpareData*>(m_prm->spareData),
			m_prm->helpText,
			m_prm->conditional,
			m_prm->callbackFunc);
	}
	else {
		return PRM_Template(
			m_prm->type,
			m_prm->typeExtended,
			m_prm->exportLevel,
			m_prm->vectorSize,
			const_cast<PRM_Name*>(m_prm->name),
			const_cast<PRM_Default*>(m_prm->defaults),
			const_cast<PRM_ChoiceList*>(m_prm->choicelist),
			const_cast<PRM_Range*>(m_prm->range),
			m_prm->callbackFunc,
			const_cast<PRM_SpareData*>(m_prm->spareData),
			m_prm->parmGroup,
			m_prm->helpText,
			m_prm->conditional);
	}

	return PRM_Template();
}
