/* SPDX-License-Identifier: MIT */

#ifndef S2R_RESET_H
#define S2R_RESET_H

/* The host tests bring their own u32/u64/bool/ALIGNED (tests/python/test_s2r_reset.py). */
#ifndef S2R_RESET_HOST
#include "types.h"
#endif

/*
 * What a core entering at RVBAR does, and the record of each such entry once the OS has the
 * S2R mailbox. Kept free of hardware access so the host tests can run it.
 *
 * Before the mailbox is published, the boot core runs aura's boot and a secondary takes the
 * spin table. Afterwards a secondary still takes the spin table, as before, and the boot core
 * resumes the OS where the OS armed resume_pa. A boot-core
 * entry the OS did not arm has no OS context to go back to. It used to reboot, which looked
 * like an orderly restart from a running OS and left nothing on record (deep-sleep-4). It now
 * parks the core, so the OS sees CPU 0 stop (RCU stall reports) and the record says when.
 */
enum s2r_reset_action {
    S2R_RESET_AURA_MAIN,
    S2R_RESET_SECONDARY,
    S2R_RESET_RESUME_OS,
    S2R_RESET_PARK,
};

static inline enum s2r_reset_action s2r_reset_decide(bool published, bool boot_core,
                                                     u64 resume_pa, u64 mpidr, u64 boot_mpidr)
{
    if (!boot_core)
        return S2R_RESET_SECONDARY;
    if (!published)
        return S2R_RESET_AURA_MAIN;
    if (resume_pa && (mpidr & 0xFFFFFF) == (boot_mpidr & 0xFFFFFF))
        return S2R_RESET_RESUME_OS;
    return S2R_RESET_PARK;
}

/*
 * One 16 KiB page the OS can read (phram "aura_s2r_resets"), one slot per core so that cores
 * entering together never share a line: no atomics, which a core with its MMU off can't use.
 */
#define S2R_RESET_LOG_MAGIC 0x31474c5253525541ULL /* "AURSRLG1" */
#define S2R_RESET_LOG_SLOTS 64
#define S2R_RESET_LOG_SIZE  0x4000

struct s2r_reset_slot {
    u64 mpidr;
    u64 entries;     /* RVBAR entries since the mailbox was published */
    u64 first_cntpct;
    u64 last_cntpct;
    u32 last_action; /* enum s2r_reset_action */
    u32 parked;      /* boot-core entries that found no resume armed */
    u64 pad[3];
} ALIGNED(64);

struct s2r_reset_log {
    u64 magic;
    u64 published_cntpct; /* when the mailbox was published, for the slots' times */
    u64 cntfrq;
    u64 pad[5];
    struct s2r_reset_slot slot[S2R_RESET_LOG_SLOTS];
} ALIGNED(64);

_Static_assert(sizeof(struct s2r_reset_log) <= S2R_RESET_LOG_SIZE, "reset log fits its page");

/* T6050: aff0 is the core, aff1 the cluster, aff2 the die. Out of range shares the last slot. */
static inline unsigned int s2r_reset_slot_index(u64 mpidr)
{
    unsigned int core = mpidr & 0xff, cluster = (mpidr >> 8) & 0xff, die = (mpidr >> 16) & 0xff;
    unsigned int idx = (die * 4 + cluster) * 8 + core;

    if (core >= 8 || cluster >= 4 || die >= 2)
        return S2R_RESET_LOG_SLOTS - 1;
    return idx;
}

static inline void s2r_reset_log_init(struct s2r_reset_log *log, u64 now, u64 cntfrq)
{
    u8 *p = (u8 *)log;

    for (size_t i = 0; i < sizeof(*log); i++)
        p[i] = 0;
    log->published_cntpct = now;
    log->cntfrq = cntfrq;
    log->magic = S2R_RESET_LOG_MAGIC;
}

/* Returns the slot written, for the caller to clean to the point of coherency. */
static inline struct s2r_reset_slot *s2r_reset_log_note(struct s2r_reset_log *log, u64 mpidr,
                                                        enum s2r_reset_action action, u64 now)
{
    if (!log || log->magic != S2R_RESET_LOG_MAGIC)
        return NULL;

    struct s2r_reset_slot *s = &log->slot[s2r_reset_slot_index(mpidr)];

    if (!s->entries)
        s->first_cntpct = now;
    s->mpidr = mpidr & 0xFFFFFF;
    s->entries++;
    s->last_cntpct = now;
    s->last_action = action;
    if (action == S2R_RESET_PARK)
        s->parked++;
    return s;
}

#endif
