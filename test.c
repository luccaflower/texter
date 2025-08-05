#include "gap.h"
#include <check.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

START_TEST(init_empty_gapbuf)
{
    struct GapBuffer* gap = Gap_new("");
    char* str = Gap_str(gap);
    ck_assert_str_eq("", str);
}
END_TEST
START_TEST(init_nonempty_gapbuf)
{
    struct GapBuffer* gap = Gap_new("not empty");
    char* str = Gap_str(gap);
    ck_assert_str_eq("not empty", str);
}
END_TEST

START_TEST(insert_single_char)
{
    struct GapBuffer* gap = Gap_new("nsert");
    Gap_insert(gap, "i");
    ck_assert_str_eq("insert", Gap_str(gap));
}
END_TEST

START_TEST(insert_some_chars)
{
    struct GapBuffer* gap = Gap_new("ert");
    Gap_insert(gap, "ins");
    ck_assert_str_eq("insert", Gap_str(gap));
}
END_TEST

START_TEST(insert_many_chars)
{

    char original[] = "string of text";
    char insert[] = "prepend a reasonably long, more than 16 chars ";
    struct GapBuffer* gap = Gap_new(original);
    Gap_insert(gap, insert);
    ck_assert_str_eq(
      "prepend a reasonably long, more than 16 chars string of text",
      Gap_str(gap));
}
END_TEST

START_TEST(many_smaller_inserts)
{
    struct GapBuffer* gap = Gap_new("string");
    Gap_insert(gap, "insertions ");
    Gap_insert(gap, "in front of ");
    Gap_insert(gap, "this ");
    ck_assert_str_eq("insertions in front of this string", Gap_str(gap));
}
END_TEST

START_TEST(mov_cursor_then_insert)
{
    struct GapBuffer* gap = Gap_new("inert");
    Gap_mov(gap, 2);
    Gap_insert(gap, "s");
    ck_assert_str_eq("insert", Gap_str(gap));
}
END_TEST

START_TEST(mov_fwd_then_back_then_insert)
{
    struct GapBuffer* gap = Gap_new("insert the middle");
    Gap_mov(gap, 10);
    Gap_mov(gap, -3);
    Gap_insert(gap, "in ");
    ck_assert_str_eq("insert in the middle", Gap_str(gap));
}
END_TEST

START_TEST(clamp_at_the_end)
{
    struct GapBuffer* gap = Gap_new("begin");
    Gap_mov(gap, 20);
    Gap_insert(gap, " end");
    ck_assert_str_eq("begin end", Gap_str(gap));
}
END_TEST

START_TEST(clamp_at_beginning)
{
    struct GapBuffer* gap = Gap_new("end");
    Gap_mov(gap, -20);
    Gap_insert(gap, "begin ");
    ck_assert_str_eq("begin end", Gap_str(gap));
}
END_TEST

START_TEST(del_char)
{
    struct GapBuffer* gap = Gap_new("delete.");
    Gap_mov(gap, gap->size);
    Gap_del(gap, 1);
    ck_assert_str_eq("delete", Gap_str(gap));
}
END_TEST

START_TEST(del_empty_is_noop)
{
    struct GapBuffer* gap = Gap_new("");
    Gap_del(gap, 1);
    ck_assert_str_eq("", Gap_str(gap));
}
END_TEST

START_TEST(del_from_start_is_noop)
{
    struct GapBuffer* gap = Gap_new("something");
    Gap_del(gap, 1);
    ck_assert_str_eq("something", Gap_str(gap));
}
END_TEST

START_TEST(del_past_end_clamps_to_end)
{
    struct GapBuffer* gap = Gap_new("something");
    Gap_mov(gap, gap->size);
    Gap_del(gap, 20);
    ck_assert_str_eq("", Gap_str(gap));
}
END_TEST

START_TEST(del_then_insert)
{
    struct GapBuffer* gap = Gap_new("delete something in here");
    Gap_mov(gap, 16);
    Gap_del(gap, 9);
    Gap_insert(gap, "nothing");
    ck_assert_str_eq("delete nothing in here", Gap_str(gap));
}
END_TEST

START_TEST(del_then_insert_large)
{
    struct GapBuffer* gap = Gap_new("delete something in here");
    Gap_mov(gap, 16);
    Gap_del(gap, 9);
    Gap_insert(gap, "something quite larger than that");
    ck_assert_str_eq("delete something quite larger than that in here",
                     Gap_str(gap));
}
END_TEST
Suite*
test_suite(void)
{
    Suite* s = suite_create("Test");
    TCase* tc_core = tcase_create("Core");
    tcase_add_test(tc_core, init_empty_gapbuf);
    tcase_add_test(tc_core, init_nonempty_gapbuf);
    tcase_add_test(tc_core, insert_single_char);
    tcase_add_test(tc_core, insert_some_chars);
    tcase_add_test(tc_core, insert_many_chars);
    tcase_add_test(tc_core, many_smaller_inserts);
    tcase_add_test(tc_core, mov_cursor_then_insert);
    tcase_add_test(tc_core, mov_fwd_then_back_then_insert);
    tcase_add_test(tc_core, clamp_at_the_end);
    tcase_add_test(tc_core, clamp_at_beginning);
    tcase_add_test(tc_core, del_char);
    tcase_add_test(tc_core, del_empty_is_noop);
    tcase_add_test(tc_core, del_from_start_is_noop);
    tcase_add_test(tc_core, del_past_end_clamps_to_end);
    tcase_add_test(tc_core, del_then_insert);
    tcase_add_test(tc_core, del_then_insert_large);
    suite_add_tcase(s, tc_core);
    return s;
}

int
main(int argc, char* argv[])
{
    int failed;
    Suite* s = test_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed) ? EXIT_FAILURE : EXIT_SUCCESS;
}
