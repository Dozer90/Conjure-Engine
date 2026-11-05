#include "conjure/testing/doctest.h"

#include "conjure/core/vfs/path_utils.h"

#include <string>
#include <string_view>

using namespace conjure;
using namespace conjure::core::vfs;

TEST_SUITE("core/path_utils") {

    TEST_CASE("path_utils - normalize converts backslashes and collapses repeated separators") {
        CHECK(path_utils::normalize("a\\b\\c") == "a/b/c");
        CHECK(path_utils::normalize("a//b///c") == "a/b/c");
        CHECK(path_utils::normalize("a\\\\b//c") == "a/b/c");
    }

    TEST_CASE("path_utils - normalize removes dot segments and trailing separators") {
        CHECK(path_utils::normalize("./a/./b/") == "a/b");
        CHECK(path_utils::normalize("a/b/.") == "a/b");
        CHECK(path_utils::normalize(".") == "");
        CHECK(path_utils::normalize("./") == "");
        CHECK(path_utils::normalize("") == "");
        CHECK(path_utils::normalize("a/") == "a");
    }

    TEST_CASE("path_utils - normalize resolves parent references") {
        CHECK(path_utils::normalize("a/b/../c") == "a/c");
        CHECK(path_utils::normalize("a/b/../../c") == "c");
        CHECK(path_utils::normalize("a/..") == "");
        CHECK(path_utils::normalize("..") == "..");
        CHECK(path_utils::normalize("../a") == "../a");
        CHECK(path_utils::normalize("../../a/../b") == "../../b");
        CHECK(path_utils::normalize("a/../..") == "..");
    }

    TEST_CASE("path_utils - normalize keeps absolute roots and drops parent references above them") {
        CHECK(path_utils::normalize("/a/b") == "/a/b");
        CHECK(path_utils::normalize("/") == "/");
        CHECK(path_utils::normalize("/../a") == "/a");
        CHECK(path_utils::normalize("/a/../..") == "/");
        CHECK(path_utils::normalize("\\a\\b\\") == "/a/b");
    }

    TEST_CASE("path_utils - normalize preserves drive letters and UNC prefixes") {
        CHECK(path_utils::normalize("C:\\Users\\x") == "C:/Users/x");
        CHECK(path_utils::normalize("C:/a/../b") == "C:/b");
        CHECK(path_utils::normalize("C:") == "C:/");
        CHECK(path_utils::normalize("C:/..") == "C:/");
        CHECK(path_utils::normalize("\\\\server\\share\\x") == "//server/share/x");
        CHECK(path_utils::normalize("//server/share/../x") == "//server/x");
    }

    TEST_CASE("path_utils - normalize is idempotent") {
        const std::string once = path_utils::normalize("./a//b\\..\\c/./d/");
        CHECK(once == "a/c/d");
        CHECK(path_utils::normalize(once) == once);
    }

    TEST_CASE("path_utils - join inserts exactly one separator") {
        CHECK(path_utils::join("a", "b") == "a/b");
        CHECK(path_utils::join("a/", "b") == "a/b");
        CHECK(path_utils::join("a\\", "b") == "a\\b");
        CHECK(path_utils::join("a/b", "c/d") == "a/b/c/d");
    }

    TEST_CASE("path_utils - join with an empty side returns the other side") {
        CHECK(path_utils::join("", "b") == "b");
        CHECK(path_utils::join("a", "") == "a");
        CHECK(path_utils::join("", "") == "");
    }

    TEST_CASE("path_utils - join with an absolute right side replaces the left side") {
        CHECK(path_utils::join("a", "/b") == "/b");
        CHECK(path_utils::join("a", "C:/b") == "C:/b");
    }

    TEST_CASE("path_utils - parent strips the last segment") {
        CHECK(path_utils::parent("a/b/c.txt") == "a/b");
        CHECK(path_utils::parent("a\\b\\c.txt") == "a\\b");
        CHECK(path_utils::parent("a") == "");
        CHECK(path_utils::parent("") == "");
        CHECK(path_utils::parent("/a") == "/");
        CHECK(path_utils::parent("/") == "/");
        CHECK(path_utils::parent("a/b/") == "a");
        CHECK(path_utils::parent("C:/a") == "C:");
    }

    TEST_CASE("path_utils - filename returns the last segment") {
        CHECK(path_utils::filename("a/b/c.txt") == "c.txt");
        CHECK(path_utils::filename("a\\b\\c.txt") == "c.txt");
        CHECK(path_utils::filename("c.txt") == "c.txt");
        CHECK(path_utils::filename("a/b/") == "");
        CHECK(path_utils::filename("") == "");
        CHECK(path_utils::filename("/") == "");
    }

    TEST_CASE("path_utils - stem removes the last extension only") {
        CHECK(path_utils::stem("a/b/c.txt") == "c");
        CHECK(path_utils::stem("archive.tar.gz") == "archive.tar");
        CHECK(path_utils::stem("README") == "README");
        CHECK(path_utils::stem(".gitignore") == ".gitignore");
        CHECK(path_utils::stem("a/.config.json") == ".config");
        CHECK(path_utils::stem("..") == "..");
        CHECK(path_utils::stem("") == "");
        CHECK(path_utils::stem("dir/") == "");
    }

    TEST_CASE("path_utils - extension is lower-cased and includes the dot") {
        CHECK(path_utils::extension("a/b/c.txt") == ".txt");
        CHECK(path_utils::extension("texture.PNG") == ".png");
        CHECK(path_utils::extension("Mixed.JpEg") == ".jpeg");
        CHECK(path_utils::extension("archive.tar.gz") == ".gz");
        CHECK(path_utils::extension("README") == "");
        CHECK(path_utils::extension(".gitignore") == "");
        CHECK(path_utils::extension("dir.v2/file") == "");
        CHECK(path_utils::extension("trailing.") == ".");
        CHECK(path_utils::extension("..") == "");
        CHECK(path_utils::extension("") == "");
    }

    TEST_CASE("path_utils - has_extension compares case-insensitively with or without the dot") {
        CHECK(path_utils::has_extension("a.PNG", "png"));
        CHECK(path_utils::has_extension("a.PNG", ".png"));
        CHECK(path_utils::has_extension("a.png", ".PNG"));
        CHECK_FALSE(path_utils::has_extension("a.png", "jpg"));
        CHECK_FALSE(path_utils::has_extension("a.png", "pn"));
        CHECK_FALSE(path_utils::has_extension("README", ""));
        CHECK_FALSE(path_utils::has_extension("README", "txt"));
        CHECK_FALSE(path_utils::has_extension(".gitignore", "gitignore"));
    }

    TEST_CASE("path_utils - is_absolute recognises slashes, drive letters and UNC prefixes") {
        CHECK(path_utils::is_absolute("/a"));
        CHECK(path_utils::is_absolute("\\a"));
        CHECK(path_utils::is_absolute("C:/a"));
        CHECK(path_utils::is_absolute("c:\\a"));
        CHECK(path_utils::is_absolute("C:"));
        CHECK(path_utils::is_absolute("//server/share"));
        CHECK_FALSE(path_utils::is_absolute("a/b"));
        CHECK_FALSE(path_utils::is_absolute("1:/a"));
        CHECK_FALSE(path_utils::is_absolute(""));
        CHECK_FALSE(path_utils::is_absolute("."));
    }

    TEST_CASE("path_utils - relative_to strips a common base") {
        CHECK(path_utils::relative_to("a/b/c", "a") == "b/c");
        CHECK(path_utils::relative_to("a/b/c", "a/b") == "c");
        CHECK(path_utils::relative_to("a/b/c", "a/b/c") == "");
        CHECK(path_utils::relative_to("/x/y/z", "/x") == "y/z");
        CHECK(path_utils::relative_to("a/b/c", "") == "a/b/c");
    }

    TEST_CASE("path_utils - relative_to climbs with parent references") {
        CHECK(path_utils::relative_to("a/b/c", "a/x") == "../b/c");
        CHECK(path_utils::relative_to("a", "a/b/c") == "../..");
        CHECK(path_utils::relative_to("x", "a/b") == "../../x");
        CHECK(path_utils::relative_to("/a", "/b/c") == "../../a");
        CHECK(path_utils::relative_to("", "a") == "..");
        CHECK(path_utils::relative_to("../a", "b") == "../../a");
        CHECK(path_utils::relative_to("../a", "../b") == "../a");
    }

    TEST_CASE("path_utils - relative_to ignores the case of the root prefix only") {
        CHECK(path_utils::relative_to("C:/Users/x", "c:/Users") == "x");
        CHECK(path_utils::relative_to("C:/Users/x", "c:/users") == "../Users/x");
        CHECK(path_utils::relative_to("a/B/c", "a/b") == "../B/c");
    }

    TEST_CASE("path_utils - relative_to normalises both inputs first") {
        CHECK(path_utils::relative_to("a\\b\\..\\c//d", "./a") == "c/d");
        CHECK(path_utils::relative_to("C:\\x\\y", "c:/x") == "y");
    }

    TEST_CASE("path_utils - relative_to returns the path unchanged when the roots differ") {
        CHECK(path_utils::relative_to("/a/b", "a") == "/a/b");
        CHECK(path_utils::relative_to("a/b", "/a") == "a/b");
        CHECK(path_utils::relative_to("C:/a", "D:/a") == "C:/a");
        CHECK(path_utils::relative_to("a/b", "../x") == "a/b");
    }
}
