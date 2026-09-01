#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "mado.hpp"

// test framework

enum class TestCategory {
    INIT,
};

struct TestCase {
    std::vector<std::string> args;
    TestCategory category;
    const char *description;
    void (*func)(TestCase *test);
};

size_t failed_tests = 0;

const char *category_name(TestCategory cat) {
    switch (cat) {
    case TestCategory::INIT:
        return "Init";
    default:
        return "Unknown";
    }
}

void test(bool check, TestCase *t) {
    if (!check) {
        failed_tests++;
        fprintf(stderr, "%-20s %-50s FAIL\n",
                category_name(t->category), t->description);
    } else {
        fprintf(stderr, "%-20s %-50s PASS\n",
                category_name(t->category), t->description);
    }
}

// helpers

std::filesystem::path create_temp_dir() {
    auto temp_dir = std::filesystem::temp_directory_path() /
                    ("mado_test_" + std::to_string(rand()));
    std::filesystem::create_directory(temp_dir);
    return temp_dir;
}

bool is_default_mado_structure(const std::filesystem::path &main_dir) {

    // chekc main dir
    if (!std::filesystem::exists(main_dir) ||
        !std::filesystem::is_directory(main_dir))
        return false;

    // check templates dir
    auto templates_dir = main_dir / ".templates";
    if (!std::filesystem::exists(templates_dir) ||
        !std::filesystem::is_directory(templates_dir))
        return false;

    // check default templates
    auto task_template = templates_dir / "task.md";
    auto note_template = templates_dir / "note.md";

    if (!std::filesystem::exists(task_template) ||
        !std::filesystem::is_regular_file(task_template))
        return false;

    if (!std::filesystem::exists(note_template) ||
        !std::filesystem::is_regular_file(note_template))
        return false;

    return true;
}

// test init

void test_init_base(TestCase *t) {
    auto temp_dir = create_temp_dir();
    std::filesystem::current_path(temp_dir);

    Mado_Config cfg;
    mado_init_config(&cfg);

    auto [main_dir, err] = mado_main_dir_init(&cfg, 0);

    Mado_Error templates_err = mado_templates_dir_init(&cfg);

    test(err == Mado_Error::OK &&
             templates_err == Mado_Error::OK &&
             is_default_mado_structure(main_dir),
         t);

    std::filesystem::remove_all(temp_dir);
}

// test init already exists

void test_init_already_exists_above(TestCase *t) {
    auto temp_dir = create_temp_dir();
    std::filesystem::current_path(temp_dir);

    Mado_Config cfg;
    mado_init_config(&cfg);

    auto [main_dir1, err1] = mado_main_dir_init(&cfg, 0);
    Mado_Error templates_err = mado_templates_dir_init(&cfg);

    std::filesystem::current_path(main_dir1);
    auto [main_dir2, err2] = mado_main_dir_init(&cfg, 0);

    test(err1 == Mado_Error::OK &&
             templates_err == Mado_Error::OK &&
             err2 == Mado_Error::FOUND_ABOVE &&
             is_default_mado_structure(main_dir1),
         t);

    std::filesystem::remove_all(temp_dir);
}

void test_init_already_exists_above_force(TestCase *t) {
    auto temp_dir = create_temp_dir();
    Mado_Config cfg;

    std::filesystem::current_path(temp_dir);
    mado_init_config(&cfg);
    auto [main_dir1, err1] = mado_main_dir_init(&cfg, 0);
    Mado_Error terr = mado_templates_dir_init(&cfg);

    std::filesystem::current_path(main_dir1);
    mado_init_config(&cfg);
    auto [main_dir2, err2] = mado_main_dir_init(&cfg, 1);
    Mado_Error terr2 = mado_templates_dir_init(&cfg);

    test(err1 == Mado_Error::OK &&
             terr == Mado_Error::OK &&
             is_default_mado_structure(main_dir1) &&

             err2 == Mado_Error::OK &&
             terr2 == Mado_Error::OK &&
             is_default_mado_structure(main_dir2),
         t);

    std::filesystem::remove_all(temp_dir);
}

void test_init_already_exists_current_force(TestCase *t) {
    auto temp_dir = create_temp_dir();
    Mado_Config cfg;

    std::filesystem::current_path(temp_dir);
    mado_init_config(&cfg);
    auto [main_dir1, err1] = mado_main_dir_init(&cfg, 0);
    Mado_Error terr = mado_templates_dir_init(&cfg);

    std::filesystem::current_path(temp_dir);
    mado_init_config(&cfg);
    auto [main_dir2, err2] = mado_main_dir_init(&cfg, 1);
    Mado_Error terr2 = mado_templates_dir_init(&cfg);

    test(err1 == Mado_Error::OK &&
             terr == Mado_Error::OK &&
             is_default_mado_structure(main_dir1) &&

             err2 == Mado_Error::ALREADY_EXISTS &&
             terr2 == Mado_Error::OK,
         t);

    std::filesystem::remove_all(temp_dir);
}

// test suite

TestCase tests[] = {
    {
        .args = {},
        .category = TestCategory::INIT,
        .description = "base init",
        .func = test_init_base,
    },
    {
        .args = {},
        .category = TestCategory::INIT,
        .description = "fail init if already exists above",
        .func = test_init_already_exists_above,
    },
    {
        .args = {},
        .category = TestCategory::INIT,
        .description = "init if already exists above (force)",
        .func = test_init_already_exists_above_force,
    },
    {
        .args = {},
        .category = TestCategory::INIT,
        .description = "fail init if already exists current (force)",
        .func = test_init_already_exists_current_force,
    },
};

// entry point

int main() {
    size_t tests_count = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < tests_count; i++) {
        tests[i].func(&tests[i]);
    }

    printf("\n(%zu / %zu) tests passed\n",
           tests_count - failed_tests, tests_count);

    return failed_tests >= 1;
}
