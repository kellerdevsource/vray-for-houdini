//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_prm_templates.h"
#include "vfh_log.h"

#include <OP/OP_Node.h>
#include <OP/OP_SpareParms.h>
#include <PRM/PRM_Template.h>
#include <PRM/PRM_ScriptPage.h>
#include <PRM/PRM_ScriptParm.h>
#include <PRM/DS_Stream.h>
#include <UT/UT_IStream.h>
#include <FS/FS_Info.h>

#include <unordered_map>


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


std::string Parm::expandUiPath(const std::string &relPath)
{
	static const char *uiroot = getenv("VRAY_UI_DS_PATH");
	if (!UTisstring(uiroot)) {
		Log::getLog().error("Invalid UI path. Please check if VRAY_UI_DS_PATH env var is pointing to the correct path.");
	}

	static FS_Info fsuiroot(uiroot);
	if (   !fsuiroot.exists()
		|| !fsuiroot.getIsDirectory() ) {
		Log::getLog().error("Invalid UI path: %s. Please check if VRAY_UI_DS_PATH env var is pointing to the correct path.",
							uiroot);
		return "";
	}

	UT_String uipath = fsuiroot.path().c_str();
	uipath += "/";
	uipath += relPath;
	FS_Info fsuipath(uipath);
	if (fsuipath.fileExists()) {
		return uipath.toStdString();
	}

	UT_StringArray files;
	UT_StringArray dirs;
	dirs.append(uiroot);

	do {
		uipath = dirs.last();
		dirs.removeLast();

		int idx = dirs.size();
		FS_Info::getContentsFromDiskPath(uipath, files, &dirs);
		for (int i = 0; i < files.size(); ++i) {
			UT_String path = uipath;
			path += "/";
			path += files(i);
			if (path.endsWith(relPath.c_str())) {
				return path.toStdString();
			}
		}

		for (int i = idx; i < dirs.size(); ++i) {
			UT_String path = uipath;
			path += "/";
			path += dirs(i);
			dirs(i) = path;
		}

	} while (dirs.size() > 0);

	Log::getLog().error("Invalid UI path requested: %s.",
						relPath.c_str());
	return "";
}


bool Parm::addPrmTemplateForPlugin(const std::string &pluginID, Parm::PRMList &prmList)
{
	if (pluginID.empty()) {
		return false;
	}

	static boost::format dspath("plugins/%s.ds");
	const std::string dsfullpath = Parm::expandUiPath( boost::str(dspath % pluginID) );
	if (dsfullpath.empty()) {
		return false;
	}

	prmList.addFromFile( dsfullpath.c_str() );
	return true;
}


Parm::PRMList* Parm::generatePrmTemplate(const std::string &pluginID)
{
	typedef std::unordered_map<std::string, PRMList> PRMListMap;
	static PRMListMap prmListMap;

	if (prmListMap.count(pluginID) == 0) {
		PRMList &prmList = prmListMap[pluginID];
		addPrmTemplateForPlugin(pluginID, prmList);
	}

	PRMList &prmList = prmListMap.at(pluginID);
	return &prmList;
}


PRM_Template* Parm::getPrmTemplate(const std::string &pluginID)
{
	Parm::PRMList *prmList = generatePrmTemplate(pluginID);
	if (!prmList) {
		Log::getLog().warning("No parameter template generated for plugin %s.", pluginID.c_str());
	}

	return (prmList)? prmList->getPRMTemplate() : nullptr;
}


PRM_Template* Parm::PRMList::loadFromFile(const char *filepath, bool cookDependent)
{
	if (!UTisstring(filepath)) {
		return nullptr;
	}

	OP_Operator op( "dummy", "dummy",
				nullptr, static_cast<PRM_Template *>(nullptr), 0 );
	UT_IFStream is(filepath);
	OP_SpareParms *opprms = op.loadSpareParms(is);
	if (!opprms) {
		return nullptr;
	}

	// NOTE: opprms is internally cached and reference-counted,
	// so bumping its ref count here will keep it alive after retrieving the parm templates
	opprms->bumpRef(1);
	PRM_Template *tmpl = opprms->getSpareTemplates();

	if (cookDependent) {
		setCookDependent(tmpl, cookDependent);
	}

	return tmpl;
}


void Parm::PRMList::setCookDependent(PRM_Template tmpl[], bool recook)
{
	if (!tmpl) {
		return;
	}

	for (int i = 0; tmpl[i].getType() != PRM_LIST_TERMINATOR; ++i) {
		if (tmpl[i].getType() != PRM_SWITCHER) {
			tmpl[i].setNoCook(!recook);
		}
	}
}


void Parm::PRMList::renamePRMTemplate(PRM_Template tmpl[], const char *prefix)
{
	if (   !tmpl
		|| !UTisstring(prefix))
	{
		return;
	}

	static boost::format prmname("%s_%s");

	for (int i = 0; tmpl[i].getType() != PRM_LIST_TERMINATOR; ++i) {
		if (tmpl[i].getType() != PRM_SWITCHER) {
			PRM_Name *name = tmpl[i].getNamePtr();
			if (name) {
				const std::string prmtoken = boost::str(prmname % prefix % name->getToken()) ;
				name->setToken(prmtoken.c_str());
				name->harden();
			}
		}
	}
}


/////////                         VfhPRMList definition
///
///
Parm::PRMList::PRMList():
	m_prmVec(1)
{
	// NOTE: extra item is list terminator
}


Parm::PRMList::~PRMList()
{
	clear();
}


void Parm::PRMList::clear()
{
	m_switcherList.clear();
	m_switcherStack.clear();
	m_prmVec.clear();
	// NOTE: extra item is list terminator
	m_prmVec.emplace_back();
	m_scriptGroups.clear();
}


Parm::PRMList& Parm::PRMList::setCookDependent(bool recook)
{
	setCookDependent(m_prmVec.data(), recook);
	return *this;
}

int Parm::PRMList::findPRMTemplate(const char *token) const
{
	const int idx = PRM_Template::getTemplateIndexByToken(m_prmVec.data(), token);
	return (idx < 0 || idx > this->size())? -1 : idx;
}


std::shared_ptr<PRM_Template> Parm::PRMList::getPRMTemplateCopy() const
{
	const int count = m_prmVec.size();

	std::shared_ptr<PRM_Template> tpl( new PRM_Template[count], std::default_delete< PRM_Template[] >() );

	for (int c = 0; c < count; ++c) {
		tpl.get()[c] = m_prmVec[c];
	}

	return tpl;
}


Parm::PRMList& Parm::PRMList::addPrm(const PRM_Template& p)
{
	m_prmVec.back() = p;
	m_prmVec.emplace_back();
	incCurrentFolderPrmCnt(1);
	return *this;
}


Parm::PRMList& Parm::PRMList::addPrm(const PRMFactory& p)
{
	m_prmVec.back() = p.getPRMTemplate();
	m_prmVec.emplace_back();
	incCurrentFolderPrmCnt(1);
	return *this;
}


Parm::PRMList& Parm::PRMList::switcherBegin(const char *token, const char *label)
{
	// add new switcher info to our list with NO default folders
	m_switcherList.emplace_back(this->size());
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
			// set correct folder count and folder info on
			// the current switcher parameter (i.e the one created with last beginSwitcher())
			PRM_Template& switcherParm = m_prmVec[info->m_parmIdx];
			switcherParm.assign(switcherParm, info->m_folders.size(), &info->m_folders.front());
		}
		m_switcherStack.pop_back();
	}

	return *this;
}


Parm::PRMList& Parm::PRMList::addFolder(const char *label)
{
	SwitcherInfo *info = getCurrentSwitcher();
	if (NOT(info)) {
		throw std::runtime_error("folder added to nonexistent switcher");
	}
	else {
		info->m_folders.emplace_back(/*numParms=*/0, ::strdup(label));
	}

	return *this;
}


Parm::PRMList& Parm::PRMList::addFromFile(const char *filepath, const char *includePath)
{
	if (!UTisstring(filepath)) {
		return *this;
	}

	DS_Stream stream(filepath);
	if (includePath && *includePath) {
		stream.addIncludePath(includePath);
	}

	// need to keep all the pages as myTemplate will have references to it
	std::shared_ptr< PRM_ScriptGroup > group = std::make_shared< PRM_ScriptGroup >(nullptr);

	int res = -1;
	while ((res = stream.getOpenBrace()) > 0) {
		PRM_ScriptPage *currentPage = new PRM_ScriptPage();

		res = currentPage->parse(stream, false, nullptr, true, true);
		if (res > 0) {
			group->addPage(currentPage);
		}
		else {
			Log::getLog().error("Parse error in file %s.", filepath);
			delete currentPage;
		}
	}

	int size = group->computeTemplateSize();
	if (!size) {
		return *this;
	}

	// save group of pages
	m_scriptGroups.push_back(group);

	// start from the last valid
	int idx = this->size();
	const int startIdx = idx;

	// resize to accomodate space for new params
	m_prmVec.resize(m_prmVec.size() + size);

	PRM_ScriptImports *imports = 0;
	group->fillTemplate(m_prmVec.data(), idx, imports, 0);

	UT_ASSERT_MSG(idx == this->size(), "Read unexpected number of params from file.");

	// add params to currently active folder, if any
	size = PRM_Template::countTemplates(m_prmVec.data() + startIdx, true);
	incCurrentFolderPrmCnt(size);

	return *this;
}


Parm::PRMList& Parm::PRMList::addFromPRMTemplate(const PRM_Template tmpl[])
{
	if (!tmpl) {
		return *this;
	}

	const int size = PRM_Template::countTemplates(tmpl);
	// reserve space for new params
	m_prmVec.reserve(m_prmVec.size() + size);

	for (int i = 0; i < size; ++i) {
		// handle top most switcher
		if (tmpl[i].getType() == PRM_SWITCHER) {
			if (getCurrentSwitcher() != nullptr) {
				// close current switcher
				switcherEnd();
			}

			// add entry for top most switcher in our switcher list
			switcherBegin(tmpl[i].getToken(), tmpl[i].getLabel());
			// init folders for top most switcher
			SwitcherInfo *swinfo = getCurrentSwitcher();
			UT_ASSERT( swinfo );

			swinfo->m_folders.reserve(tmpl[i].getVectorSize());
			PRM_Default *prmdeflist = tmpl[i].getFactoryDefaults();
			for (int j = 0; j < tmpl[i].getVectorSize(); ++j) {
				PRM_Default &prmdef = prmdeflist[j];
				swinfo->m_folders.emplace_back(prmdef.getOrdinal(), prmdef.getString());
			}

			PRM_Template& swprm = m_prmVec[swinfo->m_parmIdx];
			swprm.assign(swprm, swinfo->m_folders.size(), &swinfo->m_folders.front());

			// find end of switcher
			const PRM_Template	*endtmpl = PRM_Template::getEndOfSwitcher(tmpl + i);
			// move to next template in list i.e. first template in folder
			for (; (tmpl+i+1) != endtmpl; ++i) {
				const int idx = i+1;
				if (tmpl[idx].getType() == PRM_SWITCHER) {
					// add entry for the switcher in our switcher list
					m_switcherList.emplace_back(this->size());
					SwitcherInfo &swinfo = m_switcherList.back();
					swinfo.m_folders.reserve(tmpl[idx].getVectorSize());

					PRM_Default *prmdeflist = tmpl[idx].getFactoryDefaults();
					for (int j = 0; j < tmpl[idx].getVectorSize(); ++j) {
						PRM_Default &prmdef = prmdeflist[j];
						swinfo.m_folders.emplace_back(prmdef.getOrdinal(), prmdef.getString());
					}
				}
				// add the parameter
				m_prmVec.back() = tmpl[idx];
				m_prmVec.emplace_back();
			}
		}
		else {
			addPrm(tmpl[i]);
		}
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


void Parm::PRMList::incCurrentFolderPrmCnt(int cnt)
{
	SwitcherInfo *info = getCurrentSwitcher();
	if (!info) {
		return;
	}

	if (info->m_folders.empty()) {
		throw std::runtime_error("parameter added to switcher with no folders");
	} else {
		// If a parameter is added to this ParmList while a switcher with at least
		// one folder is active, increment the folder's parameter count.
		PRM_Default& def = info->m_folders.back();
		cnt = std::max(-def.getOrdinal(), cnt);
		def.setOrdinal(def.getOrdinal() + cnt);
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
		helpText(nullptr),
		invisible(false)
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
		helpText(nullptr),
		invisible(false)
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
	bool                       invisible;
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


Parm::PRMFactory& Parm::PRMFactory::addSpareData(const char *token, const char *value)
{
	PRM_SpareData *spareData = nullptr;
	if (NOT(m_prm->spareData)) {
		spareData = createPRMSpareData();
	}
	else {
		spareData = const_cast< PRM_SpareData * >(m_prm->spareData);
	}

	spareData->addTokenValue(token, value);
	m_prm->spareData = spareData;

	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setSpareData(const PRM_SpareData* d)
{
	m_prm->spareData = d;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setMultiparms(const PRM_Template tmpl[])
{
	m_prm->multiparms = tmpl;
	return *this;
}


Parm::PRMFactory& Parm::PRMFactory::setParmGroup(int n)
{
	m_prm->parmGroup = n;
	return *this;
}

Parm::PRMFactory& Parm::PRMFactory::setInvisible(bool v)
{
	m_prm->invisible = v;
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
	PRM_Template tmpl;

	if (m_prm->multiType != PRM_MULTITYPE_NONE) {
		tmpl.initMulti(
			m_prm->multiType,
			const_cast<PRM_Template*>(m_prm->multiparms),
			m_prm->exportLevel,
			fpreal(m_prm->vectorSize),
			const_cast<PRM_Name*>(m_prm->name),
			const_cast<PRM_Default*>(m_prm->defaults),
			const_cast<PRM_Range*>(m_prm->range),
			m_prm->callbackFunc,
			const_cast<PRM_SpareData*>(m_prm->spareData),
			m_prm->helpText,
			m_prm->conditional
			);
	}
	else {
		tmpl.initialize(
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

	tmpl.setInvisible(m_prm->invisible);

	return tmpl;
}
