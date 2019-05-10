#include "networkflowprocessor.h"
#include "utils.h"
using namespace networkflow;

LoggerPtr NetworkFlowProcessor::m_logger(Logger::getLogger("sysflow.networkflow"));

NetworkFlowProcessor::NetworkFlowProcessor(SysFlowContext* cxt, SysFlowWriter* writer, process::ProcessContext* processCxt, DataFlowSet* dfSet)  {
    m_cxt = cxt;
    m_writer = writer;
    m_processCxt = processCxt;
    m_dfSet = dfSet;
}

NetworkFlowProcessor::~NetworkFlowProcessor() {
}

inline int32_t NetworkFlowProcessor::getProtocol(scap_l4_proto proto) {
    int32_t prt = -1;
    switch(proto)
    {
        case SCAP_L4_TCP:
            prt = 6;
            break;
        case SCAP_L4_UDP:
            prt = 17;
            break;
        case SCAP_L4_ICMP:
            prt = 1;
            break;
        case SCAP_L4_RAW:
            prt = 254;
            break;
         default:
             break;
    }
    return prt;
}


inline void NetworkFlowProcessor::canonicalizeKey(sinsp_fdinfo_t* fdinfo, NFKey* key, uint64_t tid) {
    uint32_t sip = fdinfo->m_sockinfo.m_ipv4info.m_fields.m_sip;
    uint32_t dip = fdinfo->m_sockinfo.m_ipv4info.m_fields.m_dip;
    uint32_t sport = fdinfo->m_sockinfo.m_ipv4info.m_fields.m_sport;
    uint32_t dport = fdinfo->m_sockinfo.m_ipv4info.m_fields.m_dport;

    //key->oid.hpid = oid->hpid;
    //key->oid.createTS = oid->createTS;

    key->tid = tid;

//    if(sip < dip) {
    key->ip1 = sip;
    key->port1 = sport;
    key->ip2 = dip;
    key->port2 = dport;
 /*   } else if(dip < sip) {
       key->ip1 = dip;
       key->port1 = dport;
       key->ip2 = sip;
       key->port2 = sport;
    } else if(sport < dport) {
       key->ip1 = sip;
       key->port1 = sport;
       key->ip2 = dip;
       key->port2 = dport;
    } else {
       key->ip1 = dip;
       key->port1 = dport;
       key->ip2 = sip;
       key->port2 = sport;
    }*/
}

inline void NetworkFlowProcessor::canonicalizeKey(NetFlowObj* nf, NFKey* key) {
    uint32_t sip = nf->netflow.sip;
    uint32_t dip = nf->netflow.dip;
    uint32_t sport = nf->netflow.sport;
    uint32_t dport = nf->netflow.dport;
    key->tid = nf->netflow.tid;
    //key->oid.hpid = oid->hpid;
    //key->oid.createTS = oid->createTS;

   // if(sip < dip) {
    key->ip1 = sip;
    key->port1 = sport;
    key->ip2 = dip;
    key->port2 = dport;
   /* } else if(dip < sip) {
       key->ip1 = dip;
       key->port1 = dport;
       key->ip2 = sip;
       key->port2 = sport;
    } else if(sport < dport) {
       key->ip1 = sip;
       key->port1 = sport;
       key->ip2 = dip;
       key->port2 = dport;
    } else {
       key->ip1 = dip;
       key->port1 = dport;
       key->ip2 = sip;
       key->port2 = sport;
    }*/
}


inline void NetworkFlowProcessor::populateNetFlow(NetFlowObj* nf, OpFlags flag, sinsp_evt* ev, ProcessObj* proc) {
   sinsp_fdinfo_t * fdinfo = ev->get_fd_info();
   sinsp_threadinfo* ti = ev->get_thread_info();
   nf->netflow.opFlags = flag;
   nf->netflow.ts = ev->get_ts();
   nf->netflow.endTs = 0;
   nf->netflow.procOID.hpid = proc->proc.oid.hpid;
   nf->netflow.procOID.createTS = proc->proc.oid.createTS;
   nf->netflow.tid = ti->m_tid;
   nf->netflow.sip = fdinfo->m_sockinfo.m_ipv4info.m_fields.m_sip; 
   nf->netflow.dip = fdinfo->m_sockinfo.m_ipv4info.m_fields.m_dip; 
   nf->netflow.sport = fdinfo->m_sockinfo.m_ipv4info.m_fields.m_sport; 
   nf->netflow.dport = fdinfo->m_sockinfo.m_ipv4info.m_fields.m_dport; 
   nf->netflow.proto = getProtocol(fdinfo->get_l4proto());
   nf->netflow.numRRecvOps = 0;
   nf->netflow.numWSendOps = 0;
   nf->netflow.numRRecvBytes = 0;
   nf->netflow.numWSendBytes = 0;
}

inline void NetworkFlowProcessor::updateNetFlow(NetFlowObj* nf, OpFlags flag, sinsp_evt* ev) {
       nf->netflow.opFlags |= flag;
       nf->lastUpdate = utils::getCurrentTime(m_cxt);
       if(flag == OP_WRITE_SEND) {
           nf->netflow.numWSendOps++;
           int res = utils::getSyscallResult(ev);
           if(res > 0 ) {
               nf->netflow.numWSendBytes+= res;
           }
       }else if(flag == OP_READ_RECV) {
           nf->netflow.numRRecvOps++;
           int res = utils::getSyscallResult(ev);
           if(res > 0 ) {
               nf->netflow.numRRecvBytes+= res;
           }
       }
}

inline void NetworkFlowProcessor::processNewFlow(sinsp_evt* ev, ProcessObj* proc, OpFlags flag, NFKey key) {
    NetFlowObj* nf = new NetFlowObj();
    nf->exportTime = utils::getExportTime(m_cxt); 
    nf->lastUpdate = utils::getCurrentTime(m_cxt);
    populateNetFlow(nf, flag, ev, proc);
    updateNetFlow(nf, flag, ev);
    if(flag != OP_CLOSE) {
         proc->netflows[key] = nf;
        //m_netflows[key] = nf;
        m_dfSet->insert(nf);
     }else {
        nf->netflow.endTs = ev->get_ts();
        m_writer->writeNetFlow(&(nf->netflow));
        delete nf;
     }
}

inline void NetworkFlowProcessor::removeAndWriteNetworkFlow(ProcessObj* proc, NetFlowObj** nf, NFKey* key) {
    m_writer->writeNetFlow(&((*nf)->netflow));
    //m_netflows.erase(*key);
    //m_nfSet.erase((*nf));
    removeNetworkFlowFromSet(nf, false);
    removeNetworkFlow(proc, nf, key);
}


inline void NetworkFlowProcessor::processExistingFlow(sinsp_evt* ev, ProcessObj* proc, OpFlags flag, NFKey key, NetFlowObj* nf) {
     //sinsp_fdinfo_t * fdinfo = ev->get_fd_info();
     /*
     if(!(nf->netflow.procOID.hpid == proc->oid.hpid  &&
          nf->netflow.procOID.createTS == proc->oid.createTS)){
          //DO WE WANT TO ALLOW processes that aren't ancestors of each other to delegate? If not,
          //what should we do?
          if(!m_processCxt->isAncestor(&(nf->netflow.procOID), proc)) {
               cout << "Netflow: " << nf->netflow.sip << " " << nf->netflow.dip << " " << nf->netflow.sport << " " << nf->netflow.dport << " already exists but isn't an ancestor of: " << proc->exe <<  " Writing! " << endl;
               Process* nfProc = m_processCxt->getProcess(&(nf->netflow.procOID));
               if(nfProc != NULL) {
                   cout << "Netflow proc oid: " << nf->netflow.procOID.hpid << " " << nf->netflow.procOID.createTS << " " << nfProc->exe << " " << nfProc->exeArgs << "Netflow proc ancestors: Old proc" <<  endl;
                   m_processCxt->printAncestors(nfProc);
                   cout << "Current proc oid: " << proc->oid.hpid << " " << proc->oid.createTS << " " << proc->exe << " " << proc->exeArgs << " Current Process ancestors: Is ROLE SERVER? " << fdinfo->is_role_server() << endl;
                   m_processCxt->printAncestors(proc);
              }else {
                   cout << "Netflow proc oid: " << nf->netflow.procOID.hpid << " " << nf->netflow.procOID.createTS << " " << "Process is NULL" << endl;
              }
          } 
          nf->netflow.opFlags |= OP_NF_DELEGATE;
          m_writer->writeNetFlow(&(nf->netflow));
          nf->netflow.opFlags &= ~OP_NF_DELEGATE;
          nf->netflow.opFlags |= OP_NF_INHERIT;
          nf->netflow.procOID.createTS = proc->oid.createTS;
          nf->netflow.procOID.hpid = proc->oid.hpid;
      }*/
      updateNetFlow(nf, flag, ev);
      if(flag == OP_CLOSE) {
          nf->netflow.endTs = ev->get_ts();
          removeAndWriteNetworkFlow(proc, &nf, &key);
      }
}
int NetworkFlowProcessor::handleNetFlowEvent(sinsp_evt* ev, OpFlags flag) {
    sinsp_fdinfo_t * fdinfo = ev->get_fd_info();
    if(fdinfo == NULL) {
       LOG4CXX_DEBUG(m_logger, "Event: " << ev->get_name() << " doesn't have an fdinfo associated with it! ");
       return 1;
    }

    if(!(fdinfo->is_ipv4_socket() || fdinfo->is_ipv6_socket())) {
        LOG4CXX_ERROR(m_logger, "handleNetFlowEvent can only handle ip sockets, not file descriptor of type: " << fdinfo->get_typechar() << ". Ignoring..");
        return 1;
    }

    if(fdinfo->is_ipv6_socket()) {
        LOG4CXX_WARN(m_logger, "IPv6 is not supported in the current version of SysFlow.  Ignoring..");
        return 1;
    }
    bool created = false;
    //calling get process is important because it ensures that the process object has been written to the
    //sysflow file.   This is important for long running NetworkFlows that may span across files.
    ProcessObj* proc = m_processCxt->getProcess(ev, SFObjectState::REUP, created);
    NetFlowObj* nf = NULL;
    sinsp_threadinfo* ti = ev->get_thread_info();
    NFKey key;
    canonicalizeKey(fdinfo, &key, ti->m_tid);
    NetworkFlowTable::iterator nfi = proc->netflows.find(key);
    if(nfi != proc->netflows.end()) {
         nf = nfi->second;
    }

    string ip4tuple = ipv4tuple_to_string(&(fdinfo->m_sockinfo.m_ipv4info), false);

    LOG4CXX_DEBUG(m_logger, proc->proc.exe << " " << ip4tuple << " Proto: " << getProtocol(fdinfo->get_l4proto()) << " Server: " << fdinfo->is_role_server() << " Client: " << fdinfo->is_role_client() << " " << ev->get_name());

    if(nf == NULL) {
       processNewFlow(ev, proc, flag, key);
    }else {
       processExistingFlow(ev, proc, flag, key, nf);
    }
    return 0;
}

void NetworkFlowProcessor::removeNetworkFlow(ProcessObj* proc, NetFlowObj** nf, NFKey* key) {
    proc->netflows.erase(*key);
    delete *nf;
    nf = NULL;
}

int NetworkFlowProcessor::removeAndWriteNFFromProc(ProcessObj* proc, int64_t tid) {
    LOG4CXX_DEBUG(m_logger, "CALLING removeAndWriteNFFromProc");
    int deleted = 0;
    for(NetworkFlowTable::iterator nfi = proc->netflows.begin(); nfi != proc->netflows.end(); nfi++) {
        if(tid == -1 ||  tid == nfi->second->netflow.tid) {
            nfi->second->netflow.endTs = utils::getSysdigTime(m_cxt);
            nfi->second->netflow.opFlags |= OP_TRUNCATE;
            LOG4CXX_DEBUG(m_logger,"Writing NETFLOW!!");
            m_writer->writeNetFlow(&(nfi->second->netflow));
            NetFlowObj* nfo = nfi->second;
            proc->netflows.erase(nfi);
            LOG4CXX_DEBUG(m_logger, "Set size: " << m_dfSet->size());
            deleted += removeNetworkFlowFromSet(&nfo, true);
            LOG4CXX_DEBUG(m_logger, "After Set size: " << m_dfSet->size());
        }
    }
    if(tid == -1) {
        proc->netflows.clear();
    }
    return deleted;
}

int NetworkFlowProcessor::removeNetworkFlowFromSet(NetFlowObj** nfo, bool deleteNetFlow) {
        bool found = false;
        int removed = 0;
        for(DataFlowSet::iterator iter = m_dfSet->find(*nfo); iter != m_dfSet->end(); iter++) {
            if((*iter)->isNetworkFlow) {
                NetFlowObj* foundObj = static_cast<NetFlowObj*>(*iter);
                //cout << "Found: " << foundObj->netflow.procOID.createTS << " " << foundObj->netflow.procOID.hpid << endl;
                if(*foundObj == **nfo) {
                     LOG4CXX_DEBUG(m_logger, "Removing element from multiset.");
                     m_dfSet->erase(iter);
                     if(deleteNetFlow) {
                        delete *nfo;
                        nfo = NULL;
                     }
                     removed++;
                     found = true;
                     break;   
               }
           }
        }
        if(!found) {
           LOG4CXX_ERROR(m_logger, "Cannot find Netflow Object in data flow set. Deleting. This should not happen");
           if(deleteNetFlow) {
               delete *nfo;
               nfo = NULL;
           }
        }
        return removed;
}

void NetworkFlowProcessor::removeNetworkFlow(DataFlowObj* dfo) {
     NFKey key;
     NetFlowObj* nfo = static_cast<NetFlowObj*>(dfo);
     //do we want to write out a netflow that hasn't had any action in an interval?
     //nfo->netflow.endTs = utils::getSysdigTime(m_cxt);
     //m_writer->writeNetFlow(&(nfo->netflow));
     canonicalizeKey(nfo, &key);
     LOG4CXX_DEBUG(m_logger, "Erasing network flow!!!");
     ProcessObj* proc = m_processCxt->getProcess(&(nfo->netflow.procOID));
     if(proc == NULL) {
         LOG4CXX_ERROR(m_logger, "Could not find proc " << nfo->netflow.procOID.hpid << " " << nfo->netflow.procOID.createTS << " This shouldn't happen!");
     } else {
          removeNetworkFlow(proc, &nfo, &key);
     }
}

void NetworkFlowProcessor::exportNetworkFlow(DataFlowObj* dfo, time_t now) {
     NetFlowObj* nfo = static_cast<NetFlowObj*>(dfo);
     nfo->netflow.endTs = utils::getSysdigTime(m_cxt);
     m_processCxt->exportProcess(&(nfo->netflow.procOID));
     m_writer->writeNetFlow(&(nfo->netflow));
     LOG4CXX_DEBUG(m_logger, "Reupping network flow!!! ");
     nfo->netflow.ts = utils::getSysdigTime(m_cxt);
     nfo->netflow.endTs = 0;
     nfo->netflow.opFlags = 0;
     nfo->netflow.numRRecvOps = 0;
     nfo->netflow.numWSendOps = 0;
     nfo->netflow.numRRecvBytes = 0;
     nfo->netflow.numWSendBytes = 0;
}

