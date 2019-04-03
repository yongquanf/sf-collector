#ifndef _SF_CONTEXT_
#define _SF_CONTEXT_

#include <string> 

#include <time.h>

#include <sinsp.h>

using namespace std;
class SysFlowContext {
    private:
        bool m_filterCont;
        time_t m_start;
        int m_fileDuration;
        bool m_hasPrefix;
        string m_outputFile;
        string m_scapFile;
        string m_schemaFile;
        string m_exporterID;
        sinsp* m_inspector;
        int m_nfExportInterval;
        int m_nfExpireInterval;
        bool m_offline;
    public:
        SysFlowContext(bool fCont, int fDur, bool prefix, string oFile, string sFile, string schFile, string exporterID); 
        virtual ~SysFlowContext();
        uint64_t timeStamp;

        inline bool isOffline() {
            return m_offline;
         }

        inline sinsp* getInspector() {
            return m_inspector;
        }

        inline int getNFExportInterval() {
            return m_nfExportInterval;
        }

        inline int getNFExpireInterval() {
            return m_nfExpireInterval;
        }
        inline string getOutputFile() {
            return m_outputFile;
        }
        inline string getScapFile() {
            return m_scapFile;
        }
        inline const char* getSchemaFile() {
            return m_schemaFile.c_str();
        }
        inline string getExporterID() {
            return m_exporterID;
        }
        inline bool hasPrefix() {
            return m_hasPrefix;
        }
        inline int getFileDuration() {
            return m_fileDuration;
        } 
        inline bool isFilterContainers() {
            return m_filterCont;
        }    
};

#endif