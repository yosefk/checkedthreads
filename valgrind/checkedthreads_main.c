#line 2 "checkedthreads_main.c"
/*--------------------------------------------------------------------*/
/*--- A checkedthreads data race detector.   checkedthreads_main.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is based on Lackey, an example Valgrind tool that does
   some simple program measurement and tracing, and Massif, a tool
   for allocation profiling; both are GPL'd, and
   
   Copyright (C) 2002-2012 Nicholas Nethercote.
*/

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "pub_tool_machine.h"     // VG_(fnptr_to_fnentry)
#include "pub_tool_mallocfree.h"
#include "pub_tool_replacemalloc.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_stacktrace.h"
#include "pub_tool_debuginfo.h"
#include <stdint.h>

/*------------------------------------------------------------*/
/*--- Command line options                                 ---*/
/*------------------------------------------------------------*/

/* Command line options controlling instrumentation kinds, as described at
 * the top of this file. */
static Bool clo_trace_mem       = True;
static Bool clo_print_commands  = False;

static Bool ct_process_cmd_line_option(Char* arg)
{
   if VG_BOOL_CLO(arg, "--print-commands", clo_print_commands) {}
   else
      return False;
   return True;
}

static void ct_print_usage(void)
{  
   VG_(printf)(
"    --print-commands=no|yes   print commands issued by the checkedtheads\n"
"                              runtime [no]\n"
   );
}

static void ct_print_debug_usage(void)
{  
   VG_(printf)(
"    (none)\n"
   );
}

#define MAX_DSIZE    512

typedef
   IRExpr 
   IRAtom;

typedef 
   enum { Event_Ir, Event_Dr, Event_Dw, Event_Dm }
   EventKind;

typedef
   struct {
      EventKind  ekind;
      IRAtom*    addr;
      Int        size;
   }
   Event;

/* Up to this many unnotified events are allowed.  Must be at least two,
   so that reads and writes to the same address can be merged into a modify.
   Beyond that, larger numbers just potentially induce more spilling due to
   extending live ranges of address temporaries. */
#define N_EVENTS 4

/* Maintain an ordered list of memory events which are outstanding, in
   the sense that no IR has yet been generated to do the relevant
   helper calls.  The SB is scanned top to bottom and memory events
   are added to the end of the list, merging with the most recent
   notified event where possible (Dw immediately following Dr and
   having the same size and EA can be merged).

   This merging is done so that for architectures which have
   load-op-store instructions (x86, amd64), the instr is treated as if
   it makes just one memory reference (a modify), rather than two (a
   read followed by a write at the same address).

   At various points the list will need to be flushed, that is, IR
   generated from it.  That must happen before any possible exit from
   the block (the end, or an IRStmt_Exit).  Flushing also takes place
   when there is no space to add a new event.

   If we require the simulation statistics to be up to date with
   respect to possible memory exceptions, then the list would have to
   be flushed before each memory reference.  That's a pain so we don't
   bother.

   Flushing the list consists of walking it start to end and emitting
   instrumentation IR for each event, in the order in which they
   appear. */

static Event events[N_EVENTS];
static Int   events_used = 0;

#define MAGIC 0x12345678
#define CONST_MAGIC "Valgrind command"
#define MAX_CMD 128

typedef struct {
    volatile uint32_t stored_magic;
    const char const_magic[16];
    volatile char payload[MAX_CMD];
} ct_cmd;

static ct_cmd* g_ct_last_cmd = 0; /* ignore writes to cmd */

/* 3-level page table; up to 2^36 pages of 2^12 bytes each,
   organized into levels of up to 2^12 entries each.
   should work fine for virtual addresses of up to 48 bits. */
#define PAGE_BITS 12
#define PAGE_SIZE (1<<PAGE_BITS)
#define L1_BITS 12
#define NUM_PAGES (1<<L1_BITS)
#define L2_BITS 12
#define NUM_L1_PAGETABS (1<<L2_BITS)
#define L3_BITS 12
#define NUM_L2_PAGETABS (1<<L3_BITS)

#define L2_PAGETAB(addr) ((((UInt)addr >> 24) >> 12) & 0xfff)
#define L1_PAGETAB(addr) (((UInt)addr >> 24) & 0xfff)
#define PAGE(addr) (((UInt)addr >> 12) & 0xfff)
#define BYTE_IN_PAGE(addr) ((UInt)addr & 0xfff)

#define OWNER_INACCESSIBLE 0xff /* inaccessible memory - "owned" by thread 255 which is never the current thread. */

/* owning_thread[] is a summary of all levels up to this one; if
   owning_thread[i]==0, perhaps indeed the location is owned by nobody -
   and perhaps it's owned by a thread who spawned the current for.

   dirty_owners[i], if non-zero, is always equal to owning_thread[i]
   (since they're updated simultaneously), and it means that the ownership
   was obtained in the current for, and must be committed to the spawner's
   page table when this for quits. */
typedef struct ct_page_ {
    /* 0 means "owned by none" (so is OK to access).
       the rest means "owned by i" (so is OK to access for i only.) */
    unsigned char owning_thread[PAGE_SIZE];
    unsigned char* dirty_owners; /* PAGE_SIZE entries, when allocated. */
    Addr base_address;
    /* we keep a linked a list of allocated pages so as to not have
       to traverse all indexes to find allocated pages. */
    struct ct_page_* prev_alloc_page;
} ct_page;

typedef struct ct_pagetab_L1_ {
    ct_page* pages[NUM_PAGES];
    ct_page* last_alloc_page; /* head of allocated pages list */
    struct ct_pagetab_L1_* prev_alloc_pagetab_L1;
} ct_pagetab_L1;

typedef struct ct_pagetab_L2_ {
    ct_pagetab_L1* pagetabs_L1[NUM_L1_PAGETABS];
    ct_pagetab_L1* last_alloc_pagetab_L1;
    struct ct_pagetab_L2_* prev_alloc_pagetab_L2;
} ct_pagetab_L2;

typedef struct ct_pagetab_L3_ {
    ct_pagetab_L2* pagetabs_L2[NUM_L2_PAGETABS];
    ct_pagetab_L2* last_alloc_pagetab_L2;
} ct_pagetab_L3;

typedef struct ct_pagetab_stack_entry_ {
    ct_pagetab_L3* pagetab_L3;
    int thread;
    int active;
    char* stackbot;
    struct ct_pagetab_stack_entry_* next_stack_entry;
} ct_pagetab_stack_entry;

static Bool g_ct_active = False;
static ct_pagetab_stack_entry* g_ct_pagetab_stack = 0;
static ct_pagetab_L3* g_ct_pagetab_L3 = 0; /* top/curr pagetab */
static Int g_ct_curr_thread = 0; /* top/curr thread */
static char* g_ct_stackbot = 0;
static char* g_ct_stackend = 0;

static ct_page* ct_get_page(Addr a, ct_pagetab_L3* pagetab_L3, int readonly_pagetab);

static void ct_init_ownership(ct_page* page)
{
    if(g_ct_pagetab_stack == 0 || g_ct_pagetab_stack->pagetab_L3 == 0) {
        return;
    }
    ct_page* spawner_page = ct_get_page(page->base_address, g_ct_pagetab_stack->pagetab_L3, 1);
    if(spawner_page == 0) {
        return;
    }
    int spawner_thread = g_ct_pagetab_stack->thread;
    int i;
    for(i=0; i<PAGE_SIZE; ++i) {
        int spawner_owner = spawner_page->owning_thread[i];
        if(spawner_owner != spawner_thread) {
            page->owning_thread[i] = spawner_owner;
        }
        /* otherwise, don't copy ownership - it's OK to access that location */
    }
}

static ct_page* ct_get_page(Addr a, ct_pagetab_L3* pagetab_L3, int readonly_pagetab)
{
    ct_pagetab_L2* pagetab_L2;
    ct_pagetab_L1* pagetab_L1;
    ct_page* page;

    UInt pt2_index = L2_PAGETAB(a);
    pagetab_L2 = pagetab_L3->pagetabs_L2[pt2_index];
    if(pagetab_L2 == 0) {
        if(readonly_pagetab) return 0;
        pagetab_L2 = (ct_pagetab_L2*)VG_(calloc)("pagetab_L2", 1, sizeof(ct_pagetab_L2));
        pagetab_L2->prev_alloc_pagetab_L2 = pagetab_L3->last_alloc_pagetab_L2; 
        pagetab_L3->last_alloc_pagetab_L2 = pagetab_L2;
        pagetab_L3->pagetabs_L2[pt2_index] = pagetab_L2;
    }

    UInt pt1_index = L1_PAGETAB(a);
    pagetab_L1 = pagetab_L2->pagetabs_L1[pt1_index];
    if(pagetab_L1 == 0) {
        if(readonly_pagetab) return 0;
        pagetab_L1 = (ct_pagetab_L1*)VG_(calloc)("pagetab_L1", 1, sizeof(ct_pagetab_L1));
        pagetab_L1->prev_alloc_pagetab_L1 = pagetab_L2->last_alloc_pagetab_L1;
        pagetab_L2->last_alloc_pagetab_L1 = pagetab_L1;
        pagetab_L2->pagetabs_L1[pt1_index] = pagetab_L1;
    }
    
    Int page_index = PAGE(a);
    page = pagetab_L1->pages[page_index];
    if(page == 0) {
        if(readonly_pagetab) return 0;
        page = (ct_page*)VG_(calloc)("page", 1, sizeof(ct_page));
        page->base_address = a - BYTE_IN_PAGE(a);
        page->prev_alloc_page = pagetab_L1->last_alloc_page;
        pagetab_L1->last_alloc_page = page;
        pagetab_L1->pages[page_index] = page;

        ct_init_ownership(page);
    }
    return page;
}

static void ct_push_pagetab(void)
{
    ct_pagetab_stack_entry* entry = (ct_pagetab_stack_entry*)VG_(calloc)("pagetab_stack_entry", 1, sizeof(ct_pagetab_stack_entry));

    entry->pagetab_L3 = g_ct_pagetab_L3;
    entry->thread = g_ct_curr_thread;
    entry->active = g_ct_active;
    entry->stackbot = g_ct_stackbot;
    entry->next_stack_entry = g_ct_pagetab_stack;

    g_ct_pagetab_stack = entry;

    g_ct_pagetab_L3 = (ct_pagetab_L3*)VG_(calloc)("pagetab_L3", 1, sizeof(ct_pagetab_L3));
}

static void ct_commit_ownership(ct_page* page)
{
    if(!page->dirty_owners || g_ct_pagetab_stack == 0 || g_ct_pagetab_stack->pagetab_L3 == 0) {
        return;
    }
    /* FIXME: we need parent pointers in page tables or something - instead of these globals...
       because here, for instance, ct_get_page may fiddle with g_ct_pagetab_stack itself in init_ownership. */
    ct_page* spawner_page = ct_get_page(page->base_address, g_ct_pagetab_stack->pagetab_L3, 0);
    if(!spawner_page->dirty_owners) {
        spawner_page->dirty_owners = VG_(calloc)("dirty_owners", 1, PAGE_SIZE);
    }
    int curr_thread = g_ct_curr_thread;
    int i;
    for(i=0; i<PAGE_SIZE; ++i) {
        /* no matter who owned the location in this loop, the location now
           is owned by the loop spawner - "joining" means "as if we never forked" */
        int owner = page->dirty_owners[i];
        if(owner) {
            /* inaccessible memory is a special case - it stays inaccessible after the join. */
            int joined_owner = owner == OWNER_INACCESSIBLE ? owner : curr_thread;
            spawner_page->dirty_owners[i] = joined_owner;
            spawner_page->owning_thread[i] = joined_owner;
        }
    }
}

static void ct_pop_pagetab(void)
{
    ct_pagetab_L3* pagetab_L3 = g_ct_pagetab_L3;
    ct_pagetab_L2* pagetab_L2 = pagetab_L3->last_alloc_pagetab_L2;

    g_ct_curr_thread = g_ct_pagetab_stack->thread;

    /* free all L2 pages */
    while(pagetab_L2) {
        ct_pagetab_L2* prev_pagetab_L2 = pagetab_L2->prev_alloc_pagetab_L2;
        /* free all L1 pages */
        ct_pagetab_L1* pagetab_L1 = pagetab_L2->last_alloc_pagetab_L1;
        while(pagetab_L1) {
            ct_pagetab_L1* prev_pagetab_L1 = pagetab_L1->prev_alloc_pagetab_L1;
            /* free all pages */
            ct_page* page = pagetab_L1->last_alloc_page;
            while(page) {
                ct_page* prev_page = page->prev_alloc_page;
                if(page->dirty_owners) {
                    ct_commit_ownership(page);
                    VG_(free)(page->dirty_owners);
                }
                VG_(free)(page);
                page = prev_page;
            }
            /* free the L1 pagetab */
            VG_(free)(pagetab_L1);
            pagetab_L1 = prev_pagetab_L1;
        }
        /* free the L2 pagetab */
        VG_(free)(pagetab_L2);
        pagetab_L2 = prev_pagetab_L2;
    }
    VG_(free)(pagetab_L3);

    g_ct_pagetab_L3 = g_ct_pagetab_stack->pagetab_L3;
    g_ct_active = g_ct_pagetab_stack->active;
    g_ct_stackbot = g_ct_pagetab_stack->stackbot;

    ct_pagetab_stack_entry* entry = g_ct_pagetab_stack->next_stack_entry;
    VG_(free)(g_ct_pagetab_stack);
    g_ct_pagetab_stack = entry;
}

static char* ct_stack_end(void)
{
    ThreadId tid = VG_(get_running_tid)();
    return (char*)(VG_(thread_get_stack_max)(tid) - VG_(thread_get_stack_size)(tid));
}

/* reuse (abuse?) of the page table structure to keep permanently suppressed addresses. */
static ct_pagetab_L3 g_ct_supp_L3;

static void ct_suppress_forever(Addr addr)
{
    ct_pagetab_stack_entry* real_stack = g_ct_pagetab_stack;
    g_ct_pagetab_stack = 0; //TODO: this is too ugly... prevents "ownership initialization".
    ct_page* page = ct_get_page(addr, &g_ct_supp_L3, 0); 
    g_ct_pagetab_stack = real_stack;

    int index_in_page = BYTE_IN_PAGE(addr);
    page->owning_thread[index_in_page] = 1; /* in this page table, a non-zero value means "suppressed" */
}

static Bool ct_is_supressed_forever(Addr addr)
{
    ct_page* page = ct_get_page(addr, &g_ct_supp_L3, 1);
    return page && page->owning_thread[BYTE_IN_PAGE(addr)];
}

static Bool ct_suppress(Addr addr)
{
    char* p = (char*)addr;
    /* ignore writes to the stack below stackbot (the point where the ct framework was entered.) */
    if(p >= g_ct_stackend && p < g_ct_stackbot) {
        return True;
    }
    if(p < g_ct_stackend) { /* perhaps the stack grew in the meanwhile? */
        g_ct_stackend = ct_stack_end();
        /* repeat the test */
        if(p >= g_ct_stackend && p < g_ct_stackbot) {
            return True;
        }
    }
    /* ignore permanent suppressions. */
    if(ct_is_supressed_forever(addr)) {
        return True;
    }
    /* ignore writes to the command object. */
    if(p >= (char*)g_ct_last_cmd && p < (char*)(g_ct_last_cmd+1)) {
        ct_suppress_forever(addr);
        return True;
    }
    /* ignore changes to .got/.plt/.got.plt */
    VgSectKind kind = VG_(DebugInfo_sect_kind)(NULL, 0, addr);
    if(kind == Vg_SectGOT || kind == Vg_SectPLT || kind == Vg_SectGOTPLT) {
        ct_suppress_forever(addr);
        return True;
    }

    return False;
}

static inline void ct_on_access(Addr base, SizeT size, Bool store, Bool report_errors)
{
    SizeT i;
    ct_pagetab_L3* pagetab_L3 = g_ct_pagetab_L3;
    int curr_thread = g_ct_curr_thread;
    for(i=0; i<size; ++i) {
        Addr addr = base+i;
        ct_page* page = ct_get_page(addr, pagetab_L3, 0);
        int index_in_page = BYTE_IN_PAGE(addr);
        int owner = page->owning_thread[index_in_page];
        if(owner && owner != curr_thread) {
            if(report_errors && !ct_suppress(addr)) {
                VG_(printf)("checkedthreads: error - thread %d accessed %p [%p,%d], owned by %d\n",
                        g_ct_curr_thread-1,
                        (void*)addr, (void*)base, (int)size,
                        owner-1);
                VG_(get_and_pp_StackTrace)(VG_(get_running_tid)(), 20);
                break;
            }
        }
        if(store) {
            /* update the owner */
            page->owning_thread[index_in_page] = curr_thread;
            if(!page->dirty_owners) {
                page->dirty_owners = (unsigned char*)VG_(calloc)("dirty_owners", 1, PAGE_SIZE);
            }
            page->dirty_owners[index_in_page] = curr_thread;
        }
    }
}

static Bool ct_str_is(volatile const char* variable, const char* constant)
{
    int i=0;
    while(constant[i]) {
        if(variable[i] != constant[i]) {
            return False;
        }
        ++i;
    }
    return True;
}

static Addr ct_cmd_ptr(ct_cmd* cmd, int oft)
{
    return *(Addr*)&cmd->payload[oft];
}

static Int ct_cmd_int(ct_cmd* cmd, int oft)
{
    return *(volatile int32_t*)&cmd->payload[oft];
}

static void ct_process_command(ct_cmd* cmd)
{
    if(!ct_str_is(cmd->const_magic, CONST_MAGIC)) {
        return;
    }
    if(ct_str_is(cmd->payload, "begin_for")) {
        if(clo_print_commands) VG_(printf)("begin_for\n");
        ct_push_pagetab();
    }
    else if(ct_str_is(cmd->payload, "end_for")) {
        if(clo_print_commands) VG_(printf)("end_for\n");
        ct_pop_pagetab();
        if(clo_print_commands && g_ct_active) VG_(printf)("stackbot restored to %p\n",
                (void*)g_ct_stackbot);
    }
    else if(ct_str_is(cmd->payload, "iter")) {
        if(clo_print_commands) VG_(printf)("iter %d\n", ct_cmd_int(cmd, 4));
        g_ct_active = True;
    }
    else if(ct_str_is(cmd->payload, "done")) {
        if(clo_print_commands) VG_(printf)("done %d\n", ct_cmd_int(cmd, 4));
        g_ct_active = False;
    }
    else if(ct_str_is(cmd->payload, "setactiv")) {
        g_ct_active = (Bool)ct_cmd_int(cmd, 8);
    }
    else if(ct_str_is(cmd->payload, "thrd")) {
        g_ct_curr_thread = ct_cmd_int(cmd, 4)+1;
    }
    else if(ct_str_is(cmd->payload, "stackbot")) {
        g_ct_stackbot = (char*)ct_cmd_ptr(cmd, 8);
        g_ct_stackend = ct_stack_end();
        if(clo_print_commands) VG_(printf)("stackbot %p [stackend %p]\n",
                (void*)g_ct_stackbot, (void*)g_ct_stackend);
    }
    else if(ct_str_is(cmd->payload, "getowner")) {
        Addr addr = ct_cmd_ptr(cmd, 8);
        int owner = 0;
        ct_page* page = ct_get_page(addr, g_ct_pagetab_L3, 1);
        if(page) {
            int index_in_page = BYTE_IN_PAGE(addr);
            owner = page->owning_thread[index_in_page];
        }
        cmd->stored_magic = owner-1;
        if(clo_print_commands) VG_(printf)("getowner %p -> %d\n", (void*)addr, owner-1);
    }
    else {
        VG_(printf)("checkedthreads: WARNING - unknown command!\n");
        VG_(get_and_pp_StackTrace)(VG_(get_running_tid)(), 20);
    }
    g_ct_last_cmd = cmd;
}

static VG_REGPARM(2) void trace_load(Addr addr, SizeT size)
{
    if(g_ct_active) {
        ct_on_access(addr, size, False, True);
    }
}

static inline void ct_on_store(Addr addr, SizeT size)
{
   ct_cmd* p = (ct_cmd*)addr;
   if(p->stored_magic == MAGIC) {
       ct_process_command(p);
   }
   if(g_ct_active) {
       ct_on_access(addr, size, True, True);
   }
}

static VG_REGPARM(2) void trace_store(Addr addr, SizeT size)
{
    ct_on_store(addr, size);
}

static VG_REGPARM(2) void trace_modify(Addr addr, SizeT size)
{
    ct_on_store(addr, size);
}


static void flushEvents(IRSB* sb)
{
   Int        i;
   Char*      helperName;
   void*      helperAddr;
   IRExpr**   argv;
   IRDirty*   di;
   Event*     ev;

   for (i = 0; i < events_used; i++) {

      ev = &events[i];

      helperAddr = NULL;
      
      // Decide on helper fn to call and args to pass it.
      switch (ev->ekind) {
         case Event_Ir: helperAddr = NULL; break;

         case Event_Dr: helperName = "trace_load";
                        helperAddr =  trace_load;   break;

         case Event_Dw: helperName = "trace_store";
                        helperAddr =  trace_store;  break;

         case Event_Dm: helperName = "trace_modify";
                        helperAddr =  trace_modify; break;
         default:
            tl_assert(0);
      }

      // Add the helper.
      if (helperAddr) {
          argv = mkIRExprVec_2( ev->addr, mkIRExpr_HWord( ev->size ) );
          di   = unsafeIRDirty_0_N( /*regparms*/2, 
                  helperName, VG_(fnptr_to_fnentry)( helperAddr ),
                  argv );
          addStmtToIRSB( sb, IRStmt_Dirty(di) );
      }
   }

   events_used = 0;
}

// original comment from Lackey follows; checkedthreads follows the advice
// and indeed doesn't add calls to trace_instr in flushEvents.

// WARNING:  If you aren't interested in instruction reads, you can omit the
// code that adds calls to trace_instr() in flushEvents().  However, you
// must still call this function, addEvent_Ir() -- it is necessary to add
// the Ir events to the events list so that merging of paired load/store
// events into modify events works correctly.
static void addEvent_Ir ( IRSB* sb, IRAtom* iaddr, UInt isize )
{
   Event* evt;
   tl_assert(clo_trace_mem);
   tl_assert( (VG_MIN_INSTR_SZB <= isize && isize <= VG_MAX_INSTR_SZB)
            || VG_CLREQ_SZB == isize );
   if (events_used == N_EVENTS)
      flushEvents(sb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Ir;
   evt->addr  = iaddr;
   evt->size  = isize;
   events_used++;
}

static
void addEvent_Dr ( IRSB* sb, IRAtom* daddr, Int dsize )
{
   Event* evt;
   tl_assert(clo_trace_mem);
   tl_assert(isIRAtom(daddr));
   tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);
   if (events_used == N_EVENTS)
      flushEvents(sb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Dr;
   evt->addr  = daddr;
   evt->size  = dsize;
   events_used++;
}

static
void addEvent_Dw ( IRSB* sb, IRAtom* daddr, Int dsize )
{
   Event* lastEvt;
   Event* evt;
   tl_assert(clo_trace_mem);
   tl_assert(isIRAtom(daddr));
   tl_assert(dsize >= 1 && dsize <= MAX_DSIZE);

   // Is it possible to merge this write with the preceding read?
   lastEvt = &events[events_used-1];
   if (events_used > 0
    && lastEvt->ekind == Event_Dr
    && lastEvt->size  == dsize
    && eqIRAtom(lastEvt->addr, daddr))
   {
      lastEvt->ekind = Event_Dm;
      return;
   }

   // No.  Add as normal.
   if (events_used == N_EVENTS)
      flushEvents(sb);
   tl_assert(events_used >= 0 && events_used < N_EVENTS);
   evt = &events[events_used];
   evt->ekind = Event_Dw;
   evt->size  = dsize;
   evt->addr  = daddr;
   events_used++;
}


/*------------------------------------------------------------*/
/*--- Basic tool functions                                 ---*/
/*------------------------------------------------------------*/

static void ct_post_clo_init(void)
{
}

static
IRSB* ct_instrument ( VgCallbackClosure* closure,
                      IRSB* sbIn, 
                      VexGuestLayout* layout, 
                      VexGuestExtents* vge,
                      IRType gWordTy, IRType hWordTy )
{
   Int        i;
   IRSB*      sbOut;
   IRTypeEnv* tyenv = sbIn->tyenv;

   if (gWordTy != hWordTy) {
      /* We don't currently support this case. */
      VG_(tool_panic)("host/guest word size mismatch");
   }

   /* Set up SB */
   sbOut = deepCopyIRSBExceptStmts(sbIn);

   // Copy verbatim any IR preamble preceding the first IMark
   i = 0;
   while (i < sbIn->stmts_used && sbIn->stmts[i]->tag != Ist_IMark) {
      addStmtToIRSB( sbOut, sbIn->stmts[i] );
      i++;
   }

   if (clo_trace_mem) {
      events_used = 0;
   }

   for (/*use current i*/; i < sbIn->stmts_used; i++) {
      IRStmt* st = sbIn->stmts[i];
      if (!st || st->tag == Ist_NoOp) continue;

      switch (st->tag) {
         case Ist_NoOp:
         case Ist_AbiHint:
         case Ist_Put:
         case Ist_PutI:
         case Ist_MBE:
            addStmtToIRSB( sbOut, st );
            break;

         case Ist_IMark:
            if (clo_trace_mem) {
               // WARNING: do not remove this function call, even if you
               // aren't interested in instruction reads.  See the comment
               // above the function itself for more detail.
               addEvent_Ir( sbOut, mkIRExpr_HWord( (HWord)st->Ist.IMark.addr ),
                            st->Ist.IMark.len );
            }
            addStmtToIRSB( sbOut, st );
            break;

         case Ist_WrTmp:
            // Add a call to trace_load() if --trace-mem=yes.
            if (clo_trace_mem) {
               IRExpr* data = st->Ist.WrTmp.data;
               if (data->tag == Iex_Load) {
                  addEvent_Dr( sbOut, data->Iex.Load.addr,
                               sizeofIRType(data->Iex.Load.ty) );
               }
            }
            addStmtToIRSB( sbOut, st );
            break;

         case Ist_Store:
            if (clo_trace_mem) {
               IRExpr* data  = st->Ist.Store.data;
               addEvent_Dw( sbOut, st->Ist.Store.addr,
                            sizeofIRType(typeOfIRExpr(tyenv, data)) );
            }
            addStmtToIRSB( sbOut, st );
            break;

         case Ist_Dirty: {
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_CAS: {
            /* We treat it as a read and a write of the location.  I
               think that is the same behaviour as it was before IRCAS
               was introduced, since prior to that point, the Vex
               front ends would translate a lock-prefixed instruction
               into a (normal) read followed by a (normal) write. */
            Int    dataSize;
            IRType dataTy;
            IRCAS* cas = st->Ist.CAS.details;
            tl_assert(cas->addr != NULL);
            tl_assert(cas->dataLo != NULL);
            dataTy   = typeOfIRExpr(tyenv, cas->dataLo);
            dataSize = sizeofIRType(dataTy);
            if (cas->dataHi != NULL)
               dataSize *= 2; /* since it's a doubleword-CAS */
            if (clo_trace_mem) {
               addEvent_Dr( sbOut, cas->addr, dataSize );
               addEvent_Dw( sbOut, cas->addr, dataSize );
            }
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_LLSC: {
            IRType dataTy;
            if (st->Ist.LLSC.storedata == NULL) {
               /* LL */
               dataTy = typeOfIRTemp(tyenv, st->Ist.LLSC.result);
               if (clo_trace_mem)
                  addEvent_Dr( sbOut, st->Ist.LLSC.addr,
                                      sizeofIRType(dataTy) );
            } else {
               /* SC */
               dataTy = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
               if (clo_trace_mem)
                  addEvent_Dw( sbOut, st->Ist.LLSC.addr,
                                      sizeofIRType(dataTy) );
            }
            addStmtToIRSB( sbOut, st );
            break;
         }

         case Ist_Exit:
            if (clo_trace_mem) {
               flushEvents(sbOut);
            }

            addStmtToIRSB( sbOut, st );      // Original statement

            break;

         default:
            tl_assert(0);
      }
   }

   if (clo_trace_mem) {
      /* At the end of the sbIn.  Flush outstandings. */
      flushEvents(sbOut);
   }

   return sbOut;
}

static void ct_fini(Int exitcode)
{
}

//dynamic memory: when allocated, set the allocating thread as the owner.
//when freed, memory becomes inaccessible (we have a special thread for that).

static void* alloc_and_record_block(SizeT req_szB, SizeT req_alignB, Bool is_zeroed)
{
    if ((SSizeT)req_szB < 0) return NULL;

    // Allocate and zero if necessary.
    void* p = VG_(cli_malloc)( req_alignB, req_szB );
    if (!p) {
        return NULL;
    }
    if (is_zeroed) VG_(memset)(p, 0, req_szB);

    if(g_ct_active) {
        ct_on_access((Addr)p, req_szB, True, False);
    }

    return p;
}

static void unrecord_block(void* p)
{
    if(g_ct_active) {
        int real_curr_thread = g_ct_curr_thread;
        g_ct_curr_thread = OWNER_INACCESSIBLE;
        ct_on_access((Addr)p, VG_(malloc_usable_size)(p), True, False);
        g_ct_curr_thread = real_curr_thread;
    }
}

//------------------------------------------------------------//
//--- malloc() et al replacement wrappers                  ---//
//------------------------------------------------------------//

static void* ct_malloc ( ThreadId tid, SizeT szB )
{
    return alloc_and_record_block( szB, VG_(clo_alignment), /*is_zeroed*/False );
}

static void* ct___builtin_new ( ThreadId tid, SizeT szB )
{
    return alloc_and_record_block( szB, VG_(clo_alignment), /*is_zeroed*/False );
}

static void* ct___builtin_vec_new ( ThreadId tid, SizeT szB )
{
    return alloc_and_record_block( szB, VG_(clo_alignment), /*is_zeroed*/False );
}

static void* ct_calloc ( ThreadId tid, SizeT m, SizeT szB )
{
    return alloc_and_record_block( m*szB, VG_(clo_alignment), /*is_zeroed*/True );
}

static void *ct_memalign ( ThreadId tid, SizeT alignB, SizeT szB )
{
    return alloc_and_record_block( szB, alignB, False );
}

static void ct_free ( ThreadId tid, void* p )
{
    unrecord_block(p);
    VG_(cli_free)(p);
}

static void ct___builtin_delete ( ThreadId tid, void* p )
{
    ct_free(tid, p);
}

static void ct___builtin_vec_delete ( ThreadId tid, void* p )
{
    ct_free(tid, p);
}

static void* ct_realloc ( ThreadId tid, void* p_old, SizeT new_szB )
{
    void* p_new = alloc_and_record_block(new_szB, VG_(clo_alignment), False);
    SizeT to_copy = VG_(malloc_usable_size)(p_old);
    if(to_copy > new_szB) {
        to_copy = new_szB;
    }
    VG_(memcpy)(p_new, p_old, to_copy);
    ct_free(tid, p_old);
    return p_new;
}

static SizeT ct_malloc_usable_size ( ThreadId tid, void* p )
{                                                            
    return VG_(malloc_usable_size)(p);
}                                                            

static void ct_pre_clo_init(void)
{
   VG_(details_name)            ("checkedthreads");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a data race detector for the checkedthreads framework");
   VG_(details_copyright_author)(
      "Copyright (C) 2012-2013 by Yossi Kreinin (http://yosefk.com)");
   VG_(details_bug_reports_to)  ("Yossi.Kreinin@gmail.com");
   VG_(details_avg_translation_sizeB) ( 200 );

   VG_(basic_tool_funcs)          (ct_post_clo_init,
                                   ct_instrument,
                                   ct_fini);
   VG_(needs_command_line_options)(ct_process_cmd_line_option,
                                   ct_print_usage,
                                   ct_print_debug_usage);
   VG_(needs_malloc_replacement)  (ct_malloc,
                                   ct___builtin_new,
                                   ct___builtin_vec_new,
                                   ct_memalign,
                                   ct_calloc,
                                   ct_free,
                                   ct___builtin_delete,
                                   ct___builtin_vec_delete,
                                   ct_realloc,
                                   ct_malloc_usable_size,
                                   0 );
}

VG_DETERMINE_INTERFACE_VERSION(ct_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                    checkedthreads_main.c ---*/
/*--------------------------------------------------------------------*/
