#include "gap.h"
#include <check.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

START_TEST(init_empty_gapbuf)
{
    struct GapBuffer* gap = Gap_new("");
    char str[sizeof("")];
    Gap_str(gap, str);
    ck_assert_str_eq("", str);
}
END_TEST
START_TEST(init_nonempty_gapbuf)
{
    struct GapBuffer* gap = Gap_new("not empty");
    char str[sizeof("not empty")];
    Gap_str(gap, str);
    ck_assert_str_eq("not empty", str);
}
END_TEST

START_TEST(insert_single_char)
{
    struct GapBuffer* gap = Gap_new("nsert");
    Gap_insert_str(gap, "i");
    char str[sizeof("insert")];
    Gap_str(gap, str);
    ck_assert_str_eq("insert", str);
}
END_TEST

START_TEST(insert_some_chars)
{
    struct GapBuffer* gap = Gap_new("ert");
    Gap_insert_str(gap, "ins");
    char str[sizeof("insert")];
    Gap_str(gap, str);
    ck_assert_str_eq("insert", str);
}
END_TEST

START_TEST(insert_many_chars)
{

    char original[] = "string of text";
    char insert[] = "prepend a reasonably long, more than 16 chars ";
    struct GapBuffer* gap = Gap_new(original);
    Gap_insert_str(gap, insert);
    char str[sizeof("prepend a reasonably long, more than 16 chars string of "
                    "text")];
    Gap_str(gap, str);
    ck_assert_str_eq(
      "prepend a reasonably long, more than 16 chars string of text", str);
}
END_TEST

START_TEST(many_smaller_inserts)
{
    struct GapBuffer* gap = Gap_new("string");
    Gap_insert_str(gap, "insertions ");
    Gap_insert_str(gap, "in front of ");
    Gap_insert_str(gap, "this ");
    char str[sizeof("insertions in front of this string")];
    Gap_str(gap, str);
    ck_assert_str_eq("insertions in front of this string", str);
}
END_TEST

START_TEST(mov_cursor_then_insert)
{
    struct GapBuffer* gap = Gap_new("inert");
    Gap_mov(gap, 2);
    Gap_insert_str(gap, "s");
    char str[sizeof("insert")];
    Gap_str(gap, str);
    ck_assert_str_eq("insert", str);
}
END_TEST

START_TEST(mov_fwd_then_back_then_insert)
{
    struct GapBuffer* gap = Gap_new("insert the middle");
    Gap_mov(gap, 10);
    Gap_mov(gap, -3);
    Gap_insert_str(gap, "in ");
    char str[sizeof("insert in the middle")];
    Gap_str(gap, str);
    ck_assert_str_eq("insert in the middle", str);
}
END_TEST

START_TEST(clamp_at_the_end)
{
    struct GapBuffer* gap = Gap_new("begin");
    Gap_mov(gap, 20);
    Gap_insert_str(gap, " end");
    char str[sizeof("begin end")] = { 0 };
    Gap_str(gap, str);
    ck_assert_str_eq("begin end", str);
}
END_TEST

START_TEST(clamp_at_beginning)
{
    struct GapBuffer* gap = Gap_new("end");
    Gap_mov(gap, -20);
    Gap_insert_str(gap, "begin ");
    char str[sizeof("bein end")];
    Gap_str(gap, str);
    ck_assert_str_eq("begin end", str);
}
END_TEST

START_TEST(del_char)
{
    struct GapBuffer* gap = Gap_new("delete.");
    Gap_mov(gap, gap->size - 1);
    Gap_del(gap, 1);
    char str[sizeof("delete")] = { 0 };
    Gap_str(gap, str);
    ck_assert_str_eq("delete", str);
}
END_TEST

START_TEST(del_empty_is_noop)
{
    struct GapBuffer* gap = Gap_new("");
    Gap_del(gap, 1);
    char str[sizeof("")] = { 0 };
    Gap_str(gap, str);
    ck_assert_str_eq("", str);
}
END_TEST

START_TEST(del_from_start_deletes_first)
{
    struct GapBuffer* gap = Gap_new("something");
    Gap_del(gap, 1);
    char str[sizeof("omething")] = { 0 };
    Gap_str(gap, str);
    ck_assert_str_eq("omething", str);
}
END_TEST

START_TEST(del_past_end_clamps_to_end)
{
    struct GapBuffer* gap = Gap_new("something");
    Gap_del(gap, 20);
    char str[sizeof("")];
    Gap_str(gap, str);
    ck_assert_str_eq("", str);
}
END_TEST

START_TEST(del_then_insert)
{
    struct GapBuffer* gap = Gap_new("delete something in here");
    Gap_mov(gap, 7);
    Gap_del(gap, 9);
    Gap_insert_str(gap, "nothing");
    char str[sizeof("delete nothing in here")] = { 0 };
    Gap_str(gap, str);
    ck_assert_str_eq("delete nothing in here", str);
}
END_TEST

START_TEST(del_then_insert_large)
{
    struct GapBuffer* gap = Gap_new("delete something in here");
    Gap_mov(gap, 7);
    Gap_del(gap, 9);
    Gap_insert_str(gap, "something quite larger than that");
    char str[sizeof("delete something quite larger than that in here")] = { 0 };
    Gap_str(gap, str);
    ck_assert_str_eq("delete something quite larger than that in here", str);
}
END_TEST

START_TEST(insert_many_single_chars)
{
    char end[] = "end";
    struct GapBuffer* gap = Gap_new(end);
    char c = 'a';
    for (size_t i = 0; i < 32; i++) {
        Gap_insert_chr(gap, c);
    }
    char str[sizeof("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaend")];
    Gap_str(gap, str);
    ck_assert_str_eq("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaend", str);
}
END_TEST

START_TEST(substring_from_beginning)
{
    struct GapBuffer* gap = Gap_new("substring this");
    char substr[10];
    Gap_substr(gap, 0, 9, substr);
    ck_assert_str_eq("substring", substr);
}
END_TEST

START_TEST(substring_clamps_at_the_end)
{
    struct GapBuffer* gap = Gap_new("substring this");
    char substr[11];
    Gap_substr(gap, 10, 20, substr);
    ck_assert_str_eq("this", substr);
}
END_TEST

START_TEST(nextline_goes_to_the_next_line)
{
    struct GapBuffer* gap = Gap_new("next\nline");
    Gap_nextline(gap);
    ck_assert_str_eq("line", &gap->buf[gap->cur_end]);
}
END_TEST

START_TEST(nextline_with_no_next_line_is_noop)
{

    struct GapBuffer* gap = Gap_new("no next line");
    Gap_nextline(gap);
    ck_assert_str_eq("no next line", &gap->buf[gap->cur_end]);
}
END_TEST

START_TEST(prevline_goes_to_prev_line)
{

    struct GapBuffer* gap = Gap_new("prev\nline");
    Gap_mov(gap, 5);
    Gap_prevline(gap);
    ck_assert_str_eq("prev\nline", &gap->buf[gap->cur_end]);
}
END_TEST

START_TEST(next_line_preserves_pos_in_line)
{

    struct GapBuffer* gap = Gap_new("prev\nline");
    Gap_mov(gap, 2);
    Gap_nextline(gap);
    ck_assert_str_eq("ne", &gap->buf[gap->cur_end]);
}
END_TEST

START_TEST(next_line_clamps_to_end_of_next)
{

    struct GapBuffer* gap = Gap_new("longer than\nnext\nline");
    Gap_mov(gap, 8);
    Gap_nextline(gap);
    ck_assert_str_eq("\nline", &gap->buf[gap->cur_end]);
}
END_TEST
START_TEST(prev_line_preserves_pos)
{

    struct GapBuffer* gap = Gap_new("prev\nline");
    Gap_mov(gap, 7);
    Gap_prevline(gap);
    char str[sizeof("ev\nline")];
    Gap_substr(gap, gap->cur_beg, gap->size, str);
    ck_assert_str_eq("ev\nline", str);
}
END_TEST

START_TEST(next_line_at_end_stays_at_end)
{
    struct GapBuffer* gap = Gap_new("gap buffer\nwith several\nlines");
    Gap_mov(gap, gap->size);
    Gap_nextline(gap);
    ck_assert(gap->cur_beg == gap->size);
}
END_TEST

START_TEST(prev_line_at_beginning_stays_at_beginning)
{
    struct GapBuffer* gap = Gap_new("gap buffer\nwith several\nlines");
    Gap_prevline(gap);
    ck_assert(gap->cur_beg == 0);
}
END_TEST

START_TEST(nextline_with_new_line_after_next)
{
    struct GapBuffer* gap = Gap_new("1111111\n22\n222");
    Gap_nextline(gap);
    ck_assert_str_eq("22\n222", &gap->buf[gap->cur_end]);
}

START_TEST(delete_newline)
{
    struct GapBuffer* gap = Gap_new("1111111\n22\n222");
    Gap_nextline(gap);
    Gap_mov(gap, -1);
    Gap_del(gap, 1);
    char str[sizeof("111111122\n222")];
    Gap_str(gap, str);
    ck_assert_str_eq("111111122\n222", str);
}
START_TEST(delete_then_mov)
{
    struct GapBuffer* gap = Gap_new("1111111\n22\n222");
    Gap_nextline(gap);
    Gap_mov(gap, -1);
    Gap_del(gap, 1);
    Gap_mov(gap, -3);
    ck_assert_str_eq("11122\n222", &gap->buf[gap->cur_end]);
}

START_TEST(failing_case)
{
    struct GapBuffer* gap = Gap_new("111\n\n1\n11");
    Gap_nextline(gap);
    Gap_nextline(gap);
    Gap_nextline(gap);
    ck_assert_str_eq("11", &gap->buf[gap->cur_end]);
}

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
    tcase_add_test(tc_core, del_from_start_deletes_first);
    tcase_add_test(tc_core, del_past_end_clamps_to_end);
    tcase_add_test(tc_core, del_then_insert);
    tcase_add_test(tc_core, del_then_insert_large);
    tcase_add_test(tc_core, insert_many_single_chars);
    tcase_add_test(tc_core, substring_from_beginning);
    tcase_add_test(tc_core, substring_clamps_at_the_end);
    tcase_add_test(tc_core, nextline_goes_to_the_next_line);
    tcase_add_test(tc_core, nextline_with_no_next_line_is_noop);
    tcase_add_test(tc_core, prevline_goes_to_prev_line);
    tcase_add_test(tc_core, next_line_preserves_pos_in_line);
    tcase_add_test(tc_core, next_line_clamps_to_end_of_next);
    tcase_add_test(tc_core, prev_line_preserves_pos);
    tcase_add_test(tc_core, next_line_at_end_stays_at_end);
    tcase_add_test(tc_core, prev_line_at_beginning_stays_at_beginning);
    tcase_add_test(tc_core, nextline_with_new_line_after_next);
    tcase_add_test(tc_core, delete_newline);
    tcase_add_test(tc_core, delete_then_mov);
    tcase_add_test(tc_core, failing_case);

    suite_add_tcase(s, tc_core);
    return s;
}

int
main(void)
{
    int failed;
    Suite* s = test_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (failed) ? EXIT_FAILURE : EXIT_SUCCESS;
}
