"""RVBAR entries once the OS has the S2R mailbox (src/s2r_reset.h): what each core does, and the record."""
from pathlib import Path
import os
import subprocess


HARNESS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
#define ALIGNED(x) __attribute__((aligned(x)))
#define S2R_RESET_HOST
#include "s2r_reset.h"

static struct s2r_reset_log log_page;

int main(void)
{
    const u64 boot = 0x10100, other = 0x10203;

    /* Before the mailbox: the boot core runs aura's boot, a secondary the spin table. */
    assert(s2r_reset_decide(false, true, 0, boot, boot) == S2R_RESET_AURA_MAIN);
    assert(s2r_reset_decide(false, true, 0x1234, boot, boot) == S2R_RESET_AURA_MAIN);
    assert(s2r_reset_decide(false, false, 0, other, boot) == S2R_RESET_SECONDARY);

    /* After it: a secondary still takes the spin table, armed or not. */
    assert(s2r_reset_decide(true, false, 0, other, boot) == S2R_RESET_SECONDARY);
    assert(s2r_reset_decide(true, false, 0x1234, other, boot) == S2R_RESET_SECONDARY);

    /* The boot core resumes only where the OS armed resume_pa, and only as the boot core. */
    assert(s2r_reset_decide(true, true, 0x80001000, boot, boot) == S2R_RESET_RESUME_OS);
    assert(s2r_reset_decide(true, true, 0x80001000, 0x80000000 | boot, boot) ==
           S2R_RESET_RESUME_OS);

    /* Never a reboot: an unarmed boot-core entry parks, and so does a mismatched core. */
    assert(s2r_reset_decide(true, true, 0, boot, boot) == S2R_RESET_PARK);
    assert(s2r_reset_decide(true, true, 0x80001000, other, boot) == S2R_RESET_PARK);

    /* Slots: one per core, distinct across T6050's 3 x 6 cores and both dies. */
    int seen[S2R_RESET_LOG_SLOTS] = {0};
    for (unsigned die = 0; die < 2; die++)
        for (unsigned cl = 0; cl < 3; cl++)
            for (unsigned core = 0; core < 6; core++) {
                unsigned i = s2r_reset_slot_index((u64)die << 16 | cl << 8 | core);
                assert(i < S2R_RESET_LOG_SLOTS - 1);
                assert(!seen[i]++);
            }
    assert(s2r_reset_slot_index(0xff) == S2R_RESET_LOG_SLOTS - 1);
    assert(s2r_reset_slot_index(0x0900) == S2R_RESET_LOG_SLOTS - 1);
    assert(s2r_reset_slot_index(0x020000) == S2R_RESET_LOG_SLOTS - 1);

    /* No record without the page, or before it is set up. */
    assert(!s2r_reset_log_note(NULL, boot, S2R_RESET_PARK, 5));
    assert(!s2r_reset_log_note(&log_page, boot, S2R_RESET_PARK, 5));

    s2r_reset_log_init(&log_page, 1000, 24000000);
    assert(log_page.magic == S2R_RESET_LOG_MAGIC && log_page.published_cntpct == 1000);
    assert(log_page.cntfrq == 24000000);

    struct s2r_reset_slot *s = s2r_reset_log_note(&log_page, 0x80000000 | other,
                                                  S2R_RESET_SECONDARY, 2000);
    assert(s == &log_page.slot[s2r_reset_slot_index(other)]);
    assert(s->mpidr == other && s->entries == 1 && s->parked == 0);
    assert(s->first_cntpct == 2000 && s->last_cntpct == 2000);
    s = s2r_reset_log_note(&log_page, other, S2R_RESET_SECONDARY, 3000);
    assert(s->entries == 2 && s->first_cntpct == 2000 && s->last_cntpct == 3000);

    s = s2r_reset_log_note(&log_page, boot, S2R_RESET_PARK, 4000);
    assert(s->entries == 1 && s->parked == 1 && s->last_action == S2R_RESET_PARK);
    s = s2r_reset_log_note(&log_page, boot, S2R_RESET_RESUME_OS, 5000);
    assert(s->entries == 2 && s->parked == 1 && s->last_action == S2R_RESET_RESUME_OS);

    /* Each slot fills whole cache lines of its own. */
    assert(sizeof(struct s2r_reset_slot) % 64 == 0);
    assert(((u64)&log_page.slot[1] - (u64)&log_page.slot[0]) % 64 == 0);
    assert(sizeof(log_page) <= S2R_RESET_LOG_SIZE);
    puts("s2r reset decisions and record: PASS");
    return 0;
}
'''


def test_s2r_reset(tmp_path):
    repo = Path(__file__).resolve().parents[2]
    src = tmp_path / "s2r_reset_host.c"
    src.write_text(HARNESS)
    exe = tmp_path / "s2r_reset_host"
    cc = os.environ.get("CC", "cc")
    subprocess.run([cc, "-std=gnu11", "-O1", "-Wall", "-Wextra", "-Werror", "-I", str(repo / "src"),
                    str(src), "-o", str(exe)], check=True)
    out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
    assert "PASS" in out
