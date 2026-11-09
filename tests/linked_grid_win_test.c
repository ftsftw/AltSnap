/* Runs the unmodified production adapter against deterministic Win32 stubs. */
#include "linked_grid_win_stub.h"
#include "../linked_grid_win.h"
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)

static void setup(unsigned quadrant, int trace)
{
    unsigned i;
    memset(&linked,0,sizeof linked); memset(fake,0,sizeof fake);
    linked_queue=(LGQueue){1,0,0}; linked_dispatch=1;
    CHECK(lg_queue_enter(&linked_queue,linked_dispatch));
    linked_trace=trace; linked_trace_offers=0;
    conf.LinkedGridResize=1; conf.FullWin=1;
    state.hwnd=quadrant+1; state.mdiclient=0; state.origin.dpi=96;
    state.resize=(POINT){quadrant&1 ? RZ_LEFT:RZ_RIGHT,quadrant&2 ? RZ_TOP:RZ_BOTTOM};
    position_calls=native_reads=dwm_reads=debug_lines=0;
    batch_fail=batch_calls=cancel_in_batch=0; debug_used=0; debug_log[0]=0;
    read_effect=read_effect_at=restore_effect=0; effect_cell=3;
    memset(post_apply_reads,0,sizeof post_apply_reads);
    memset(restore_calls,0,sizeof restore_calls);
    for (i=0;i<4;++i) {
        LONG x=(i&1)?500:0, y=(i&2)?400:0;
        fake[i].dwm=(RECT){x,y,x+500,y+400};
        fake[i].before=fake[i].native=(RECT){x-8,y,x+508,y+408};
        fake[i].min_width=116; fake[i].min_height=108;
        fake[i].correction=107;
    }
    BeginLinkedGrid((POINT){500,400});
    CHECK(linked.drag.mode==LG_DRAG_ACTIVE);
    CHECK(lg_preview_active(&linked.drag).left==fake[quadrant].dwm.left);
    CHECK(LinkedGridValid());
}
static int grid_intact(void)
{
    RECT v[4];
    unsigned i;
    for (i=0;i<4;++i) {
        v[i]=fake[i].native;
        v[i].left+=8; v[i].right-=8; v[i].bottom-=8;
    }
    return v[0].left==fake[0].before.left+8 && v[2].left==v[0].left
        && v[1].right==fake[3].before.right-8 && v[3].right==v[1].right
        && v[0].top==fake[0].before.top && v[1].top==v[0].top
        && v[2].bottom==fake[3].before.bottom-8 && v[3].bottom==v[2].bottom
        && v[0].right==v[1].left && v[2].right==v[3].left && v[0].right==v[2].right
        && v[0].bottom==v[2].top && v[1].bottom==v[3].top && v[0].bottom==v[1].bottom;
}
static void exact_and_limits(void)
{
    unsigned q, i;
    for (q=0;q<4;++q) {
        setup(q,0);
        CHECK(lg_drag_update(&linked.drag,INT32_MAX,INT32_MIN)==LG_OK);
        CHECK(linked.drag.pending.x==900 && linked.drag.pending.y==100);
        CHECK(CommitLinkedGrid()); CHECK(grid_intact()); CHECK(LinkedGridValid());
        CHECK(lg_drag_update(&linked.drag,INT32_MIN,INT32_MAX)==LG_OK);
        CHECK(CommitLinkedGrid()); CHECK(grid_intact()); CHECK(LinkedGridValid());
        CHECK(debug_lines==0);
        /* Rapid coalesced samples feed the real solver and adapter. */
        for (i=0;i<200;++i) {
            LGMoveSlot slot={0,0,0,0};
            uintptr_t g;
            int32_t x,y;
            CHECK(lg_move_offer(&slot,1,INT32_MAX,INT32_MIN));
            CHECK(!lg_move_offer(&slot,1,(int32_t)(200+i),(int32_t)(150+i)));
            CHECK(lg_move_take(&slot,&g,&x,&y) && g==1);
            CHECK(lg_drag_update(&linked.drag,x,y)==LG_OK);
            CHECK(CommitLinkedGrid()); CHECK(grid_intact()); CHECK(LinkedGridValid());
        }
    }
}
static void failure_models(void)
{
    unsigned q, bad, i;
    for (q=0;q<4;++q) for (bad=0;bad<4;++bad) {
        setup(q,1); fake[bad].behavior=REJECT;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid()); CHECK(linked.drag.mode==LG_DRAG_BLOCKED);
        CHECK(grid_intact());
        for (i=0;i<4;++i) CHECK(EqualRect(&fake[i].native,&fake[i].before));
        CHECK(strstr(debug_log,"verify-mismatch") && strstr(debug_log,"recover-result"));
        CHECK(!CommitLinkedGrid());

        setup(q,1); fake[bad].behavior=ADJUST;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid()); CHECK(linked.drag.mode==LG_DRAG_BLOCKED);
        /* Successful API call, corrected right edge +107, current DWM: all
           four post-apply witnesses still match and all four restore. */
        CHECK(grid_intact());
        CHECK(strstr(debug_log,"recover-mask cell=-1 a=15 b=15"));
        for (i=0;i<4;++i) {
            CHECK(EqualRect(&fake[i].native,&fake[i].before));
            CHECK(restore_calls[i]==1);
        }
        CHECK(batch_calls==1);

        setup(q,1);
        for (i=0;i<4;++i) fake[i].lag=1;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(CommitLinkedGrid()); CHECK(grid_intact());
        CHECK(!LinkedGridValid()); /* stale DWM alone stops, but does not break grid */
        lg_drag_stop(&linked.drag);
        CHECK(linked.drag.mode==LG_DRAG_BLOCKED);
        CHECK(strstr(debug_log,"valid-border-stop"));

        setup(q,1); fake[bad].behavior=REJECT;
        for (i=0;i<4;++i) fake[i].lag=1;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid()); CHECK(!grid_intact());
        CHECK(linked.drag.mode==LG_DRAG_BLOCKED);
        /* Exact-size survivors lose recovery eligibility when DWM lags. */
        CHECK(strstr(debug_log,"recover-border"));
        CHECK(strstr(debug_log,"recover-mask cell=-1 a=0"));
        for (i=0;i<4;++i) if (i!=bad) CHECK(!EqualRect(&fake[i].native,&fake[i].before));

        setup(q,1);
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK && CommitLinkedGrid());
        /* An application changes geometry after the verification read. */
        fake[bad].native.right-=7; fake[bad].dwm.right-=7;
        CHECK(!LinkedGridValid()); lg_drag_stop(&linked.drag);
        CHECK(linked.drag.mode==LG_DRAG_BLOCKED && !grid_intact());
        CHECK(strstr(debug_log,"valid-native"));
    }
}
/* Reproduce the native coordinates from the Windows 10 trace exactly. */
static void reported_correction(void)
{
    unsigned i;
    setup(0,1);
    for (i=0;i<4;++i) {
        LONG l=(i&1)?1430:400, r=(i&1)?2559:1430;
        LONG t=(i&2)?600:100, b=(i&2)?1358:600;
        fake[i].dwm=(RECT){l,t,r,b};
        fake[i].native=fake[i].before=(RECT){l-8,t,r+8,b+8};
    }
    BeginLinkedGrid((POINT){1430,600});
    CHECK(linked.drag.mode==LG_DRAG_ACTIVE && LinkedGridValid());
    fake[3].behavior=ADJUST;
    CHECK(lg_drag_update(&linked.drag,1493,429)==LG_OK);
    CHECK(!CommitLinkedGrid());
    CHECK(strstr(debug_log,"event=end-defer cell=-1 a=1 b=15"));
    CHECK(strstr(debug_log,"event=verify-mismatch cell=3 readable=1 "
                           "want=1485,429,2567,1366 got=1485,429,2674,1366"));
    CHECK(strstr(debug_log,"recover-mask cell=-1 a=15 b=15"));
    CHECK(grid_intact() && linked.drag.mode==LG_DRAG_BLOCKED);
    for (i=0;i<4;++i) CHECK(EqualRect(&fake[i].native,&fake[i].before));
}

static void witnessed_recovery(void)
{
    unsigned q, bad, i, stage;
    /* Restore the last verified grid, including after earlier successful moves.
       An app that refuses that baseline gets just one attempt; never a loop. */
    for (q=0;q<4;++q) for (bad=0;bad<4;++bad) {
        setup(q,0);
        CHECK(lg_drag_update(&linked.drag,600,450)==LG_OK && CommitLinkedGrid());
        for (i=0;i<4;++i) fake[i].before=fake[i].native;
        fake[bad].behavior=ADJUST;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid() && grid_intact());
        for (i=0;i<4;++i) CHECK(EqualRect(&fake[i].native,&linked.actual[i]));

        setup(q,1); fake[bad].behavior=ADJUST; fake[bad].refuse_restore=1;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid() && !grid_intact());
        CHECK(linked.drag.mode==LG_DRAG_BLOCKED && restore_calls[bad]==1);
        CHECK(strstr(debug_log,"recover-mismatch"));
        CHECK(!CommitLinkedGrid() && restore_calls[bad]==1);
        for (i=0;i<4;++i) if (i!=bad) CHECK(EqualRect(&fake[i].native,&fake[i].before));

        /* An origin change is not a size-correction witness, even when first
           observed after a successful API call. Conservatively leave it alone. */
        setup(q,0); fake[bad].behavior=REPOSITION;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid() && restore_calls[bad]==0);
        CHECK(fake[bad].native.left==batch_rect[bad].left+31);
        for (i=0;i<4;++i) if (i!=bad) CHECK(EqualRect(&fake[i].native,&fake[i].before));

        /* A failed EndDeferWindowPos grants no third-rectangle witness. */
        setup(q,0); fake[bad].behavior=ADJUST; batch_fail=1;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid() && !grid_intact());
        CHECK(restore_calls[bad]==0);
        for (i=0;i<4;++i) if (i!=bad) CHECK(EqualRect(&fake[i].native,&fake[i].before));
    }
    for (bad=0;bad<4;++bad) {
        setup(0,0); fake[bad].behavior=ADJUST; fake[bad].correction=-7;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid() && grid_intact());
        for (i=0;i<4;++i) CHECK(EqualRect(&fake[i].native,&fake[i].before));
    }
    /* Native reads 2/5 are before recovery selection / at the final per-HWND
       check (trace off). A later move is never adopted as a new witness. */
    for (stage=2;stage<=5;stage+=3) for (bad=0;bad<4;++bad) {
        setup(0,0); fake[bad].behavior=ADJUST;
        effect_cell=bad; read_effect_at=(int)stage; read_effect=MOVE_EXTERNAL;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid());
        CHECK(restore_calls[bad]==0 && linked.drag.mode==LG_DRAG_BLOCKED);
        CHECK(fake[bad].native.left==batch_rect[bad].left+31);
        CHECK(fake[bad].native.right==batch_rect[bad].right+138);
        for (i=0;i<4;++i) if (i!=bad) CHECK(EqualRect(&fake[i].native,&fake[i].before));
    }
    /* A preceding restore's callback can move a later participant, even back
       to its requested rectangle: neither rectangle may override the witness. */
    for (i=MOVE_EXTERNAL;i<=STALE_OPERATION;++i) {
        unsigned calls;
        setup(0,1); fake[3].behavior=ADJUST;
        restore_effect=(int)i;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid());
        CHECK(restore_calls[0]==1 && restore_calls[3]==0);
        if (i==MOVE_EXTERNAL) CHECK(fake[3].native.left==batch_rect[3].left+31);
        if (i==RESIZE_EXTERNAL) {
            CHECK(fake[3].native.left==batch_rect[3].left);
            CHECK(fake[3].native.right==batch_rect[3].right+138);
        }
        if (i==MOVE_TO_REQUESTED) CHECK(EqualRect(&fake[3].native,&batch_rect[3]));
        if (i==CANCEL_OPERATION || i==STALE_OPERATION) {
            CHECK(!lg_queue_current(&linked_queue,linked_dispatch));
            CHECK(restore_calls[1]==0 && restore_calls[2]==0);
            /* Token invalidation alone in this stub must not let an old
               dispatch mutate drag state belonging to a newer generation. */
            CHECK(linked.drag.mode==LG_DRAG_ACTIVE);
        } else CHECK(linked.drag.mode==LG_DRAG_BLOCKED);
        calls=position_calls; CHECK(!CommitLinkedGrid() && calls==position_calls);
    }
    /* Disappearance or identity replacement at the first verification read
       prevents that HWND from ever receiving recovery positioning. */
    for (bad=0;bad<4;++bad) for (i=DISAPPEAR;i<=REPLACE_IDENTITY;++i) {
        unsigned j;
        setup(0,0); fake[bad].behavior=ADJUST;
        effect_cell=bad; read_effect_at=1; read_effect=(int)i;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid() && restore_calls[bad]==0);
        CHECK(linked.drag.mode==LG_DRAG_BLOCKED);
        for (j=0;j<4;++j) if (j!=bad) CHECK(EqualRect(&fake[j].native,&fake[j].before));
    }
    for (i=CANCEL_OPERATION;i<=STALE_OPERATION;++i) {
        setup(0,0); fake[3].behavior=ADJUST;
        effect_cell=0; read_effect_at=1; read_effect=(int)i;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid() && position_calls==4);
        CHECK(!lg_queue_current(&linked_queue,linked_dispatch));
    }
    setup(0,0);
    CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
    lg_queue_invalidate(&linked_queue);
    CHECK(!CommitLinkedGrid() && position_calls==0);

    /* Cancellation/replacement during the last eligibility read must also
       prevent positioning, even though the rectangle itself still matches. */
    for (i=CANCEL_OPERATION;i<=STALE_OPERATION;++i) {
        setup(0,0); fake[3].behavior=ADJUST;
        effect_cell=0; read_effect_at=5; read_effect=(int)i;
        CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
        CHECK(!CommitLinkedGrid() && !lg_queue_current(&linked_queue,linked_dispatch));
        CHECK(position_calls==4);
    }
    /* A change after the one restoration attempt is diagnosed, never retried. */
    setup(0,1); fake[3].behavior=ADJUST;
    effect_cell=3; read_effect=MOVE_EXTERNAL;
    /* Trace geometry adds one read before recovery, so the result read is 7. */
    read_effect_at=7;
    CHECK(lg_drag_update(&linked.drag,850,650)==LG_OK);
    CHECK(!CommitLinkedGrid() && restore_calls[3]==1);
    CHECK(strstr(debug_log,"recover-mismatch"));
    CHECK(!grid_intact());
}

static void lifecycle_and_trace(void)
{
    unsigned calls, n, d;
    setup(0,0);
    n=native_reads; d=dwm_reads;
    LinkedTraceGeometry("disabled",linked.actual);
    CHECK(native_reads==n && dwm_reads==d && !debug_lines);
    CHECK(lg_drag_update(&linked.drag,750,600)==LG_OK);
    cancel_in_batch=1;
    CHECK(!CommitLinkedGrid()); calls=position_calls;
    CHECK(!lg_queue_current(&linked_queue,linked_dispatch));
    CHECK(!CommitLinkedGrid() && position_calls==calls);

    setup(0,1); conf.LinkedGridResize=0;
    n=native_reads; d=dwm_reads; calls=debug_lines;
    BeginLinkedGrid((POINT){500,400});
    CHECK(linked.drag.mode==LG_DRAG_LEGACY);
    CHECK(native_reads==n && dwm_reads==d && debug_lines==calls);

    setup(0,0); conf.FullWin=0; BeginLinkedGrid((POINT){500,400});
    CHECK(lg_drag_update(&linked.drag,750,600)==LG_OK && !position_calls);
    CHECK(!linked.drag.fullwin); CHECK(CommitLinkedGrid() && grid_intact());

    setup(0,1); fake[3].missing=1;
    CHECK(!LinkedGridValid()); lg_drag_stop(&linked.drag);
    CHECK(linked.drag.mode==LG_DRAG_BLOCKED && !linked.drag.dirty);
    CHECK(lg_drag_update(&linked.drag,700,700)==LG_NOT_FOUND && !CommitLinkedGrid());

    setup(0,1); fake[3].behavior=REJECT; fake[1].refuse_restore=1;
    CHECK(lg_drag_update(&linked.drag,750,600)==LG_OK);
    CHECK(!CommitLinkedGrid()); CHECK(!grid_intact());
    CHECK(strstr(debug_log,"recover-individual"));
    CHECK(linked.drag.mode==LG_DRAG_BLOCKED);
    CHECK(!strstr(debug_log,"hwnd") && !strstr(debug_log,"pid=") && !strstr(debug_log,"title"));

    setup(0,1); batch_fail=1;
    CHECK(lg_drag_update(&linked.drag,750,600)==LG_OK);
    CHECK(!CommitLinkedGrid() && grid_intact());
    CHECK(linked.drag.mode==LG_DRAG_BLOCKED);
    CHECK(strstr(debug_log,"end-defer-error") && strstr(debug_log,"recover-result"));

    setup(0,1); fake[3].min_width=800;
    BeginLinkedGrid((POINT){500,400});
    CHECK(linked.drag.mode==LG_DRAG_BLOCKED && !position_calls);
    CHECK(!CommitLinkedGrid() && strstr(debug_log,"event=begin cell=-1 a=4"));
}
static void negative_coordinates(void)
{
    unsigned q,i;
    for (q=0;q<4;++q) {
        setup(q,0);
        for (i=0;i<4;++i) {
            RECT *r[3]={&fake[i].native,&fake[i].dwm,&fake[i].before};
            unsigned j;
            for (j=0;j<3;++j) {
                r[j]->left-=1500; r[j]->right-=1500;
                r[j]->top-=900; r[j]->bottom-=900;
            }
        }
        BeginLinkedGrid((POINT){-1000,-500});
        CHECK(linked.drag.mode==LG_DRAG_ACTIVE && LinkedGridValid());
        CHECK(lg_drag_update(&linked.drag,INT32_MAX,INT32_MIN)==LG_OK);
        CHECK(linked.drag.pending.x==-600 && linked.drag.pending.y==-800);
        CHECK(CommitLinkedGrid() && grid_intact() && LinkedGridValid());
    }
}
int main(void)
{
    exact_and_limits(); failure_models(); reported_correction(); witnessed_recovery();
    lifecycle_and_trace(); negative_coordinates();
    printf("linked_grid_win_stubs: %u checks passed (no Windows GUI)\n",checks);
    return 0;
}
