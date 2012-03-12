/*
	Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010  Belledonne Communications SARL.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <cstring>
#include <algorithm>
#include <iostream>

#include "lpconfig.h"
#include "configmanager.hh"
#include "common.hh"

using namespace::std;

ConfigValue::ConfigValue(const string &name, ConfigValueType  vt, const string &help, const string &default_value) 
	:  ConfigEntry (name,vt,help), mDefaultValue(default_value){
	
}

void ConfigValue::set(const string  &value){
	if (getType()==Boolean){
		if (value!="true" && value!="false" && value!="1" && value!="0"){
			LOGF("Not a boolean: \"%s\" for key \"%s\" ", value.c_str(), getName().c_str());
		}
	}
	mValue=value;
}

void ConfigValue::setDefault(const string & value){
	if (getType()==Boolean){
		if (value!="true" && value!="false" && value!="1" && value!="0"){
			LOGF("Not a boolean: \"%s\" for key \"%s\" ", value.c_str(), getName().c_str());
		}
	}
	mDefaultValue=value;
}

const string & ConfigValue::get()const{
	return mValue;
}

const string & ConfigValue::getDefault()const{
	return mDefaultValue;
}

ConfigEntry::ConfigEntry(const string &name, ConfigValueType type, const string &help) : 
mName(name),mHelp(help),mType(type),mParent(0){
	if (strchr(name.c_str(),'_'))
		LOGA("Underscores not allowed in config items, please use minus sign.");
}

void ConfigEntry::setParent(ConfigEntry *parent){
	mParent=parent;
}

ConfigStruct::ConfigStruct(const string &name, const string &help) : ConfigEntry(name,Struct,help){
	
}

ConfigEntry * ConfigStruct::addChild(ConfigEntry *c){
	mEntries.push_back(c);
	c->setParent(this);
	return c;
}

void ConfigStruct::addChildrenValues(ConfigItemDescriptor *items){
	for (;items->name!=NULL;items++){
		ConfigValue *val=NULL;
		switch(items->type){
			case Boolean:
				val=new ConfigBoolean(items->name,items->help,items->default_value);
			break;
			case Integer:
				val=new ConfigInt(items->name,items->help,items->default_value);
			break;
			case String:
				val=new ConfigString(items->name,items->help,items->default_value);
			break;
			case StringList:
				val=new ConfigStringList(items->name,items->help,items->default_value);
			break;
			default:
				LOGA("Bad ConfigValue type !");
			break;
		}
		addChild(val);
	}
}

struct matchEntryName{
	matchEntryName(const char *name) : mName(name){};
	bool operator()(ConfigEntry* e){
		return strcmp(mName,e->getName().c_str())==0;
	}
	const char *mName;
};

ConfigEntry *ConfigStruct::find(const char *name)const{
	list<ConfigEntry*>::const_iterator it=find_if(mEntries.begin(),mEntries.end(),matchEntryName(name));
	if (it!=mEntries.end()) return *it;
	return NULL;
}

struct matchEntryNameApprox{
	matchEntryNameApprox(const char *name) : mName(name){};
	bool operator()(ConfigEntry* e){
		unsigned int i;
		int count=0;
		int min_required=mName.size()-2;
		if (min_required<1) return false;
		
		for(i=0;i<mName.size();++i){
			if (e->getName().find(mName[i])!=string::npos){
				count++;
			}
		}
		if (count>=min_required){
			return true;
		}
		return false;
	}
	const string mName;
};

ConfigEntry * ConfigStruct::findApproximate(const char *name)const{
	list<ConfigEntry*>::const_iterator it=find_if(mEntries.begin(),mEntries.end(),matchEntryNameApprox(name));
	if (it!=mEntries.end()) return *it;
	return NULL;
}

list<ConfigEntry*> &ConfigStruct::getChildren(){
	return mEntries;
}

struct destroy{
	void operator()(ConfigEntry *e){
		delete e;
	}
};

ConfigStruct::~ConfigStruct(){
	for_each(mEntries.begin(),mEntries.end(),destroy());
}


ConfigBoolean::ConfigBoolean(const string &name, const string &help, const string &default_value)
	: ConfigValue(name, Boolean, help, default_value){
}

bool ConfigBoolean::read()const{
	if (get()=="true" || get()=="1") return true;
	else if (get()=="false" || get()=="0") return false;
	LOGA("Bad boolean value %s",get().c_str());
	return false;
}


ConfigInt::ConfigInt(const string &name, const string &help, const string &default_value)
	:	ConfigValue(name,Integer,help,default_value){
}

int ConfigInt::read()const{
	return atoi(get().c_str());
}

ConfigString::ConfigString(const string &name, const string &help, const string &default_value)
	:	ConfigValue(name,String,help,default_value){
}

const string & ConfigString::read()const{
	return get();
}


ConfigStringList::ConfigStringList(const string &name, const string &help, const string &default_value)
	:	ConfigValue(name,StringList,help,default_value){
}

#define DELIMITERS " \n,"

list<string>  ConfigStringList::read()const{
	list<string> retlist;
	char *res=strdup(get().c_str());
	char *saveptr=NULL;
	char *ret=strtok_r(res,DELIMITERS,&saveptr);
	while(ret!=NULL){
		retlist.push_back(string(ret));
		ret=strtok_r(NULL,DELIMITERS,&saveptr);
	}
	free(res);
	return retlist;
}


ConfigManager *ConfigManager::sInstance=0;

void ConfigManager::atexit() {
	if (sInstance!=NULL) {
		delete sInstance;
		sInstance = NULL;
	}
}

ConfigManager *ConfigManager::get(){
	if (sInstance==NULL) {
		sInstance=new ConfigManager();
		::atexit(ConfigManager::atexit);
	}
	return sInstance;
}

static ConfigItemDescriptor global_conf[]={
	{	Boolean	,	"debug"	,	"Outputs very detailed logs",	"false"	},
	{	Boolean	,	"auto-respawn",	"Automatically respawn flexisip in case of abnormal termination (crashes)",	"true"	},
	{	StringList	,	"aliases"	,	"List of white space separated host names pointing to this machine. This is to prevent loops while routing SIP messages.", "localhost" },
	{	String	,	"ip-address",	"The public ip address of the proxy.",	"guess"},
	{	String	,	"bind-address",	"The local interface's ip address where to listen. The wildcard (*) means all interfaces.",	"*"},
	{	Integer	,	"port"		,	"UDP/TCP port number to listen for sip messages.",	"5060"},
	config_item_end
};

static ConfigItemDescriptor tls_conf[]={
	{	Boolean	,	"enabled"	,	"Enable SIP/TLS (sips)",	"true"	},
	{	Integer	,	"port",	"The port used for SIP/TLS",	"5061"},
	{	String	,	"certificates-dir", "An absolute path of a directory where TLS certificate can be found. "
		"The private key for TLS server must be in a agent.pem file within this directory" , "/etc/flexisip/tls"	},
	config_item_end
};

ConfigManager::ConfigManager() : mConfigRoot("root","This is the default Flexisip configuration file"), mReader(&mConfigRoot){
	ConfigStruct *global=new ConfigStruct("global","Some global settings of the flexisip proxy.");
	global->addChildrenValues(global_conf);
	mConfigRoot.addChild(global);
	ConfigStruct *tls=new ConfigStruct("tls","TLS specific parameters.");
	tls->addChildrenValues(tls_conf);
	mConfigRoot.addChild(tls);
}

int ConfigManager::load(const char* configfile){
	return mReader.read(configfile);
}

void ConfigManager::loadStrict(){
	mReader.reload();
	mReader.checkUnread();
}

ConfigStruct *ConfigManager::getRoot(){
	return &mConfigRoot;
}

const ConfigStruct *ConfigManager::getGlobal(){
	return mConfigRoot.get<ConfigStruct>("global");
}

ostream &FileConfigDumper::dump(ostream & ostr)const {
	return dump2(ostr,mRoot,0);
}

ostream & FileConfigDumper::printHelp(ostream &os, const string &help, const string &comment_prefix)const{
	const char *p=help.c_str();
	const char *begin=p;
	const char *origin=help.c_str();
	for(;*p!=0;++p){
		if (p-begin>60 && *p==' '){
			os<<comment_prefix<<" "<<help.substr(begin-origin,p-begin)<<endl;
			p++;
			begin=p;
		}
	}
	os<<comment_prefix<<" "<<help.substr(begin-origin,p-origin)<<endl;
	return os;
}

ostream &FileConfigDumper::dump2(ostream & ostr, ConfigEntry *entry, int level)const{
	ConfigStruct *cs=dynamic_cast<ConfigStruct*>(entry);
	ConfigValue *val;
	
	if (cs){
		ostr<<"##"<<endl;
		printHelp(ostr,cs->getHelp(),"##");
		ostr<<"##"<<endl;
		if (level>0){
			ostr<<"["<<cs->getName()<<"]"<<endl;
		}else ostr<<endl;
		list<ConfigEntry*>::iterator it;
		for(it=cs->getChildren().begin();it!=cs->getChildren().end();++it){
			dump2(ostr,*it,level+1);
			ostr<<endl;
		}
	}else if ((val=dynamic_cast<ConfigValue*>(entry))!=NULL){
		printHelp(ostr,entry->getHelp(),"#");
		ostr<<"#  Default value: "<<val->getDefault()<<endl;
		ostr<<entry->getName()<<"="<<val->getDefault()<<endl;
	}
	return ostr;
}


int FileConfigReader::read(const char *filename){
	int err;
	mCfg=lp_config_new(NULL);
	err=lp_config_read_file(mCfg,filename);
	read2(mRoot,0);
	return err;
}

int FileConfigReader::reload(){
	read2(mRoot,0);
	return 0;
}

void FileConfigReader::onUnreadItem(void *p, const char *secname, const char *key, int lineno){
	FileConfigReader *zis=(FileConfigReader*)p;
	zis->onUnreadItem(secname,key,lineno);
}

void FileConfigReader::onUnreadItem(const char *secname, const char *key, int lineno){
	LOGE("Unsupported parameter '%s' in section [%s] at line %i", key, secname, lineno);
	mHaveUnreads=true;
	ConfigEntry *sec=mRoot->find(secname);
	if (sec==NULL){
		sec=mRoot->findApproximate(secname);
		if (sec!=NULL){
			LOGE("Did you meant '[%s]' ?",sec->getName().c_str());
		}
		return;
	}
	ConfigStruct *st=dynamic_cast<ConfigStruct*>(sec);
	if (st){
		ConfigEntry *val=st->find(key);
		if (val==NULL){
			val=st->findApproximate(key);
			if (val!=NULL){
				LOGE("Did you meant '%s' ?",val->getName().c_str());
			}
		}
	}
}

void FileConfigReader::checkUnread(){
	lp_config_for_each_unread (mCfg,onUnreadItem,this);
	if (mHaveUnreads)
		LOGF("Please fix your configuration file.");
}

int FileConfigReader::read2(ConfigEntry *entry, int level){
	ConfigStruct *cs=dynamic_cast<ConfigStruct*>(entry);
	ConfigValue *cv;
	if (cs){
		list<ConfigEntry*> & entries=cs->getChildren();
		list<ConfigEntry*>::iterator it;
		for(it=entries.begin();it!=entries.end();++it){
			read2(*it,level+1);
		}
	}else if ((cv=dynamic_cast<ConfigValue*>(entry))){
		if (level<2){
			LOGF("ConfigValues at root is disallowed.");
		}else if (level==2){
			const char *val=lp_config_get_string(mCfg,cv->getParent()->getName().c_str(),cv->getName().c_str(),cv->getDefault().c_str());
			cv->set(val);
		}else{
			LOGF("The current file format doesn't support recursive subsections.");
		}
	}
	return 0;
}

FileConfigReader::~FileConfigReader(){
	if (mCfg) lp_config_destroy(mCfg);
}
