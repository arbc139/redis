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

#ifdef USE_PMDK
#include <stdlib.h>
#include "server.h"
#include "obj.h"
#include "libpmemobj.h"
#include "util.h"
#include "sds.h"

int
pmemReconstruct(void)
{
#ifdef TODIS
    serverLog(LL_TODIS, "   ");
    serverLog(LL_TODIS, "TODIS, pmemReconstruct START");
#endif
    TOID(struct redis_pmem_root) root;
    TOID(struct key_val_pair_PM) kv_PM_oid;
    struct key_val_pair_PM *kv_PM;
    dict *d;
    void *key;
    void *val;
    void *pmem_base_addr;

    root = server.pm_rootoid;
    pmem_base_addr = (void *)server.pm_pool->addr;
    d = server.db[0].dict;
    dictExpand(d, D_RO(root)->num_dict_entries);
    for (kv_PM_oid = D_RO(root)->pe_first; TOID_IS_NULL(kv_PM_oid) == 0; kv_PM_oid = D_RO(kv_PM_oid)->pmem_list_next){
		kv_PM = (key_val_pair_PM *)(kv_PM_oid.oid.off + (uint64_t)pmem_base_addr);
		key = (void *)(kv_PM->key_oid.off + (uint64_t)pmem_base_addr);
		val = (void *)(kv_PM->val_oid.off + (uint64_t)pmem_base_addr);
#ifdef TODIS
        server.used_pmem_memory += sdsAllocSizePM(key);
        server.used_pmem_memory += sdsAllocSizePM(val);
        server.used_pmem_memory += sizeof(struct key_val_pair_PM);
        serverLog(
                LL_TODIS,
                "TODIS, pmemReconstruct reconstructed used_pmem_memory: %zu",
                server.used_pmem_memory);
#endif

        (void)dictAddReconstructedPM(d, key, val);
    }
#ifdef TODIS
    serverLog(LL_TODIS, "TODIS, pmemReconstruct END");
#endif
    return C_OK;
}

void pmemKVpairSet(void *key, void *val)
{
    PMEMoid *kv_PM_oid;
    PMEMoid val_oid;
    struct key_val_pair_PM *kv_PM_p;

    kv_PM_oid = sdsPMEMoidBackReference((sds)key);
    kv_PM_p = (struct key_val_pair_PM *)pmemobj_direct(*kv_PM_oid);

    val_oid.pool_uuid_lo = server.pool_uuid_lo;
    val_oid.off = (uint64_t)val - (uint64_t)server.pm_pool->addr;

    TX_ADD_FIELD_DIRECT(kv_PM_p, val_oid);
    kv_PM_p->val_oid = val_oid;
    return;
}

#ifdef TODIS
void pmemKVpairSetRearrangeList(void *key, void *val)
{
    serverLog(LL_TODIS, "   ");
    serverLog(LL_TODIS, "TODIS, pmemKVpairSetRearrangeList START");
    PMEMoid *kv_PM_oid;

    kv_PM_oid = sdsPMEMoidBackReference((sds)key);
    pmemRemoveFromPmemList(*kv_PM_oid);
    PMEMoid new_kv_PM_oid = pmemAddToPmemList(key, val);
    *kv_PM_oid = new_kv_PM_oid;
    serverLog(LL_TODIS, "TODIS, pmemKVpairSetRearrangeList END");
    return;
}
#endif

PMEMoid
pmemAddToPmemList(void *key, void *val)
{
    serverLog(LL_TODIS, "   ");
    serverLog(LL_TODIS, "TODIS, pmemAddToPmemList START");
    PMEMoid key_oid;
    PMEMoid val_oid;
    PMEMoid kv_PM;
    struct key_val_pair_PM *kv_PM_p;
    TOID(struct key_val_pair_PM) typed_kv_PM;
    struct redis_pmem_root *root;

    key_oid.pool_uuid_lo = server.pool_uuid_lo;
    key_oid.off = (uint64_t)key - (uint64_t)server.pm_pool->addr;

    val_oid.pool_uuid_lo = server.pool_uuid_lo;
    val_oid.off = (uint64_t)val - (uint64_t)server.pm_pool->addr;

    kv_PM = pmemobj_tx_zalloc(sizeof(struct key_val_pair_PM), pm_type_key_val_pair_PM);
    kv_PM_p = (struct key_val_pair_PM *)pmemobj_direct(kv_PM);
    kv_PM_p->key_oid = key_oid;
    kv_PM_p->val_oid = val_oid;
    typed_kv_PM.oid = kv_PM;

#ifdef TODIS
        serverLog(
                LL_TODIS,
                "TODIS, pmemAddToPmemList key_val_pair_PM size: %zu",
                sizeof(struct key_val_pair_PM));
    server.used_pmem_memory += sizeof(struct key_val_pair_PM);
#endif

    root = pmemobj_direct(server.pm_rootoid.oid);

    kv_PM_p->pmem_list_next = root->pe_first;
    if(!TOID_IS_NULL(root->pe_first)) {
        struct key_val_pair_PM *head = D_RW(root->pe_first);
        TX_ADD_FIELD_DIRECT(head,pmem_list_prev);
    	head->pmem_list_prev = typed_kv_PM;
    }

    if(TOID_IS_NULL(root->pe_last)) {
        root->pe_last = typed_kv_PM;
    }

    TX_ADD_DIRECT(root);
    root->pe_first = typed_kv_PM;
    root->num_dict_entries++;

#ifdef TODIS
    serverLog(LL_TODIS, "TODIS, pmemAddFromPmemList END");
#endif
    return kv_PM;
}

void
pmemRemoveFromPmemList(PMEMoid kv_PM_oid)
{
#ifdef TODIS
    serverLog(LL_TODIS, "   ");
    serverLog(LL_TODIS, "TODIS, pmemRemoveFromPmemList START");
#endif
    TOID(struct key_val_pair_PM) typed_kv_PM;
    struct redis_pmem_root *root;

    root = pmemobj_direct(server.pm_rootoid.oid);

    typed_kv_PM.oid = kv_PM_oid;

#ifdef TODIS
        serverLog(
                LL_TODIS,
                "TODIS, pmemRemoveFromPmemList key_val_pair_PM size: %zu",
                sizeof(struct key_val_pair_PM));
        server.used_pmem_memory -= sizeof(struct key_val_pair_PM);
#endif

    if (TOID_EQUALS(root->pe_first, typed_kv_PM) &&
        TOID_EQUALS(root->pe_last, typed_kv_PM)) {
        TX_FREE(root->pe_first);
        TX_ADD_DIRECT(root);
        root->pe_first = TOID_NULL(struct key_val_pair_PM);
        root->pe_last = TOID_NULL(struct key_val_pair_PM);
        root->num_dict_entries--;
        serverLog(LL_TODIS, "TODIS, pmemRemovFromPmemList END");
        return;
    }
    else if(TOID_EQUALS(root->pe_first, typed_kv_PM)) {
    	TOID(struct key_val_pair_PM) typed_kv_PM_next = D_RO(typed_kv_PM)->pmem_list_next;
    	if(!TOID_IS_NULL(typed_kv_PM_next)){
    		struct key_val_pair_PM *next = D_RW(typed_kv_PM_next);
    		TX_ADD_FIELD_DIRECT(next,pmem_list_prev);
    		next->pmem_list_prev.oid = OID_NULL;
    	}
    	TX_FREE(root->pe_first);
    	TX_ADD_DIRECT(root);
    	root->pe_first = typed_kv_PM_next;
        root->num_dict_entries--;
        serverLog(LL_TODIS, "TODIS, pmemRemoveFromPmemList END");
        return;
    }
    else if (TOID_EQUALS(root->pe_last, typed_kv_PM)) {
        TOID(struct key_val_pair_PM) typed_kv_PM_prev = D_RO(typed_kv_PM)->pmem_list_prev;
        if (!TOID_IS_NULL(typed_kv_PM_prev)) {
            struct key_val_pair_PM *prev = D_RW(typed_kv_PM_prev);
            TX_ADD_FIELD_DIRECT(prev, pmem_list_next);
            prev->pmem_list_next.oid = OID_NULL;
        }
        TX_FREE(root->pe_last);
        TX_ADD_DIRECT(root);
        root->pe_last = typed_kv_PM_prev;
        root->num_dict_entries--;
        serverLog(LL_TODIS, "TODIS, pmemRemovFromPmemList END");
        return;
    }
    else {
    	TOID(struct key_val_pair_PM) typed_kv_PM_prev = D_RO(typed_kv_PM)->pmem_list_prev;
    	TOID(struct key_val_pair_PM) typed_kv_PM_next = D_RO(typed_kv_PM)->pmem_list_next;
    	if(!TOID_IS_NULL(typed_kv_PM_prev)){
    		struct key_val_pair_PM *prev = D_RW(typed_kv_PM_prev);
    		TX_ADD_FIELD_DIRECT(prev,pmem_list_next);
    		prev->pmem_list_next = typed_kv_PM_next;
    	}
    	if(!TOID_IS_NULL(typed_kv_PM_next)){
    		struct key_val_pair_PM *next = D_RW(typed_kv_PM_next);
    		TX_ADD_FIELD_DIRECT(next,pmem_list_prev);
    		next->pmem_list_prev = typed_kv_PM_prev;
    	}
    	TX_FREE(typed_kv_PM);
    	TX_ADD_FIELD_DIRECT(root,num_dict_entries);
        root->num_dict_entries--;
        serverLog(LL_TODIS, "TODIS, pmemRemoveFromPmemList END");
        return;
    }
}
#endif

#ifdef TODIS
key_val_pair_PM *getPmemKvObject(uint64_t i) {
    TOID(struct redis_pmem_root) root;
    TOID(struct key_val_pair_PM) kv_PM_oid;
    void *pmem_base_addr;
    struct redis_pmem_root *root_obj;
    uint64_t count = 0;

    root = server.pm_rootoid;
    root_obj = pmemobj_direct(root.oid);
    pmem_base_addr = (void *)server.pm_pool->addr;

    if (i >= root_obj->num_dict_entries) return NULL;

    for (kv_PM_oid = D_RO(root)->pe_first;
        TOID_IS_NULL(kv_PM_oid) == 0;
        kv_PM_oid = D_RO(kv_PM_oid)->pmem_list_next
    ) {
        if (count < i) {
            ++count;
            continue;
        }
        return (key_val_pair_PM *)(kv_PM_oid.oid.off + (uint64_t) pmem_base_addr);
    }
    return NULL;
}
#endif

#ifdef TODIS
sds getBestEvictionKeyPM(void) {
    TOID(struct redis_pmem_root) root;
    void *pmem_base_addr;
    struct redis_pmem_root *root_obj;
    uint64_t num_pmem_entries;

    root = server.pm_rootoid;
    root_obj = pmemobj_direct(root.oid);
    num_pmem_entries = root_obj->num_dict_entries;
    pmem_base_addr = (void *)server.pm_pool->addr;

    /* allkeys-random policy */
    if (server.max_pmem_memory_policy == MAXMEMORY_ALLKEYS_RANDOM) {
        uint64_t index = random() % num_pmem_entries;
        struct key_val_pair_PM *victim = getPmemKvObject(index);
        if (victim == NULL) return NULL;
        return (sds)(victim->key_oid.off + (uint64_t) pmem_base_addr);
    }

    /* allkeys-lru policy */
    else if (server.max_pmem_memory_policy == MAXMEMORY_ALLKEYS_LRU) {
        // TODO(totoro): Needs to implement LRU algorithm...
        return NULL;
    }
    return NULL;
}
#endif

#ifdef TODIS
size_t pmem_used_memory(void) {
    return server.used_pmem_memory;
}
#endif
