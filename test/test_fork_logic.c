/**
 * Lightweight regression tests for fork pure logic.
 * Does not link the full game — duplicates small pure helpers under test.
 *
 * Build (MSVC example from repo root):
 *   cl /nologo /W3 /Fe:test_fork_logic.exe test\test_fork_logic.c
 * Run:
 *   test_fork_logic.exe
 */

#include <stdio.h>
#include <stdlib.h>

static int failures;

static void expect_int(const char *name, int got, int want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
        failures++;
    } else {
        printf("ok   %s\n", name);
    }
}

/* --- Blueprint footprint rotate (mirrors blueprint.c math) --- */

static void rotate_cw_footprint(int *dx, int *dy, int s, int old_h)
{
    int x = *dx;
    int y = *dy;
    *dx = old_h - y - s;
    *dy = x;
}

static void mirror_h_footprint(int *dx, int s, int width)
{
    *dx = width - s - *dx;
}

static void test_blueprint_math(void)
{
    int dx = 0, dy = 0;
    rotate_cw_footprint(&dx, &dy, 1, 3);
    expect_int("rotate 1x1 (0,0) in 3x3 -> dx", dx, 2);
    expect_int("rotate 1x1 (0,0) in 3x3 -> dy", dy, 0);

    dx = 1;
    dy = 0;
    rotate_cw_footprint(&dx, &dy, 1, 3);
    expect_int("rotate (1,0) -> dx", dx, 2);
    expect_int("rotate (1,0) -> dy", dy, 1);

    /* 3x3 warehouse at TL (0,0) in 5x5 stamp */
    dx = 0;
    dy = 0;
    rotate_cw_footprint(&dx, &dy, 3, 5);
    expect_int("warehouse TL rotate dx", dx, 2);
    expect_int("warehouse TL rotate dy", dy, 0);

    dx = 1;
    mirror_h_footprint(&dx, 1, 5);
    expect_int("mirror (1) in W=5 S=1", dx, 3);

    dx = 0;
    mirror_h_footprint(&dx, 3, 5);
    expect_int("mirror warehouse TL", dx, 2);
}

/* --- Storage staffing threshold (mirrors accept logic) --- */

static int storage_accepts_staff(int pct_workers, int optional_50_pct_mode)
{
    int min_staff = optional_50_pct_mode ? 50 : 100;
    return pct_workers >= min_staff;
}

static void test_storage_staff(void)
{
    expect_int("vanilla 99% reject", storage_accepts_staff(99, 0), 0);
    expect_int("vanilla 100% accept", storage_accepts_staff(100, 0), 1);
    expect_int("optional 50% accept 50", storage_accepts_staff(50, 1), 1);
    expect_int("optional 50% reject 49", storage_accepts_staff(49, 1), 0);
}

/* --- Depot threshold cycle never hits 0 --- */

static int depot_next_threshold(int t, int step)
{
    int max = 32;
    t = t + step;
    if (t > max) {
        t = step;
    }
    return t;
}

static void test_depot_threshold(void)
{
    int t = 32;
    t = depot_next_threshold(t, 8);
    expect_int("32+8 wraps to 8 not 0", t, 8);
    t = 28;
    t = depot_next_threshold(t, 4);
    expect_int("28+4 = 32", t, 32);
    t = depot_next_threshold(t, 4);
    expect_int("32+4 wraps to 4", t, 4);
}

/* --- Tax mood uses max(live, settled) --- */

static int tax_for_mood(int live, int settled)
{
    return settled > live ? settled : live;
}

static void test_tax_mood(void)
{
    expect_int("cheese: live 0 settled 25", tax_for_mood(0, 25), 25);
    expect_int("normal: live 7 settled 7", tax_for_mood(7, 7), 7);
    expect_int("raise: live 15 settled 7", tax_for_mood(15, 7), 15);
}

/* --- Sentiment message only on threshold cross (mirrors sentiment.c) --- */

static int sentiment_message_type(int value, int prev, int delay)
{
    if (value >= 48 || delay > 0) {
        return 0;
    }
    if (value < 35 && prev >= 35) {
        return 3; /* ANGRY */
    }
    if (value < 40 && prev >= 40) {
        return 2; /* UNHAPPY */
    }
    if (prev >= 48) {
        return 1; /* DISGRUNTLED */
    }
    return 0;
}

static void test_sentiment_threshold(void)
{
    expect_int("cross 48->47", sentiment_message_type(47, 48, 0), 1);
    expect_int("stay 47 no spam", sentiment_message_type(46, 47, 0), 0);
    expect_int("cross 40", sentiment_message_type(39, 40, 0), 2);
    expect_int("cross 35", sentiment_message_type(34, 35, 0), 3);
    expect_int("delay blocks", sentiment_message_type(34, 50, 5), 0);
    expect_int("happy no msg", sentiment_message_type(60, 70, 0), 0);
}

int main(void)
{
    test_blueprint_math();
    test_storage_staff();
    test_depot_threshold();
    test_tax_mood();
    test_sentiment_threshold();
    if (failures) {
        fprintf(stderr, "\n%d test(s) failed\n", failures);
        return 1;
    }
    printf("\nAll tests passed\n");
    return 0;
}
