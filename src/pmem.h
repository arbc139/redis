/*
 * Copyright (c) 2017, Andreas Bluemle <andreas dot bluemle at itxperts dot de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PMEM_H
#define __PMEM_H

#include "server.h"
#include "sds.h"

#ifdef USE_PMDK
typedef struct key_val_pair_PM {
    PMEMoid key_oid;
    PMEMoid val_oid;
    TOID(struct key_val_pair_PM) pmem_list_next;
    TOID(struct key_val_pair_PM) pmem_list_prev;
} key_val_pair_PM;

int pmemReconstruct(void);
void pmemKVpairSet(void *key, void *val);
PMEMoid pmemAddToPmemList(void *key, void *val);
void pmemRemoveFromPmemList(PMEMoid kv_PM_oid);
#ifdef USE_ND
int pmemReconstructNDHEDIS(void);
void pmemKVpairSetRearrangeList(void *key, void *val);
PMEMoid pmemUnlinkFromPmemList(PMEMoid oid);
int getBestEvictionKeysPMEMoid(PMEMoid *victim_oids);
PMEMoid getBestEvictionKeyPMEMoid(void);
sds getBestEvictionKeyPM(void);
struct key_val_pair_PM *getPMObjectFromOid(PMEMoid oid);
sds getKeyFromPMObject(struct key_val_pair_PM *obj);
sds getValFromPMObject(struct key_val_pair_PM *obj);
sds getKeyFromOid(PMEMoid oid);
sds getValFromOid(PMEMoid oid);
int evictPmemNodesToVictimList(PMEMoid *victim_oids);
int evictPmemNodeToVictimList(PMEMoid victim_oid);
void freeVictimList(PMEMoid start_oid);
size_t pmem_used_memory(void);
size_t sizeOfPmemNode(PMEMoid oid);
struct redis_pmem_root *getPmemRootObject(void);
#endif
#endif

#endif
