// Unit tests for path conversion and quoting.

#include "path-format.h"
#include "test-support.h"

using path_format::ApplyQuoteMode;
using path_format::ContainsWhitespace;
using path_format::FormatPaths;
using path_format::QuoteMode;
using path_format::ToForwardSlashes;

TEST(ConvertsDrivePath) {
  EXPECT_EQ(ToForwardSlashes(L"H:\\documents\\programming\\work"),
            L"H:/documents/programming/work");
}

TEST(ConvertsDriveRoot) { EXPECT_EQ(ToForwardSlashes(L"C:\\"), L"C:/"); }

TEST(ConvertsUncPath) {
  EXPECT_EQ(ToForwardSlashes(L"\\\\server\\share\\folder\\file.txt"),
            L"//server/share/folder/file.txt");
}

TEST(DropsFileNamespacePrefixFromDrivePath) {
  EXPECT_EQ(ToForwardSlashes(L"\\\\?\\C:\\very\\long\\path"),
            L"C:/very/long/path");
}

TEST(DropsFileNamespacePrefixFromUncPath) {
  EXPECT_EQ(ToForwardSlashes(L"\\\\?\\UNC\\server\\share\\a"),
            L"//server/share/a");
}

TEST(KeepsVolumeGuidPathPrefix) {
  EXPECT_EQ(ToForwardSlashes(L"\\\\?\\Volume{1234}\\folder"),
            L"//?/Volume{1234}/folder");
}

TEST(LeavesForwardSlashPathUnchanged) {
  EXPECT_EQ(ToForwardSlashes(L"C:/already/forward"), L"C:/already/forward");
}

TEST(KeepsNonAsciiCharacters) {
  EXPECT_EQ(ToForwardSlashes(L"D:\\Résumé\\日本語.txt"), L"D:/Résumé/日本語.txt");
}

TEST(DetectsWhitespace) {
  EXPECT_TRUE(ContainsWhitespace(L"C:/My Files"));
  EXPECT_TRUE(ContainsWhitespace(L"C:/tab\there"));
  EXPECT_TRUE(!ContainsWhitespace(L"C:/work/file.txt"));
}

TEST(WhenNeededQuotesOnlyPathsWithSpaces) {
  EXPECT_EQ(ApplyQuoteMode(L"C:/My Files/a.txt", QuoteMode::kWhenNeeded),
            L"\"C:/My Files/a.txt\"");
  EXPECT_EQ(ApplyQuoteMode(L"C:/work/a.txt", QuoteMode::kWhenNeeded),
            L"C:/work/a.txt");
}

TEST(AlwaysQuotesEveryPath) {
  EXPECT_EQ(ApplyQuoteMode(L"C:/work/a.txt", QuoteMode::kAlways),
            L"\"C:/work/a.txt\"");
}

TEST(NeverQuotes) {
  EXPECT_EQ(ApplyQuoteMode(L"C:/My Files/a.txt", QuoteMode::kNever),
            L"C:/My Files/a.txt");
}

TEST(FormatsOnePath) {
  EXPECT_EQ(FormatPaths({L"H:\\work\\notes.txt"}, QuoteMode::kWhenNeeded),
            L"H:/work/notes.txt");
}

TEST(JoinsSeveralPathsWithCrLfAndNoTrailingSeparator) {
  EXPECT_EQ(FormatPaths({L"C:\\a b\\one.txt", L"C:\\two.txt"},
                        QuoteMode::kWhenNeeded),
            L"\"C:/a b/one.txt\"\r\nC:/two.txt");
}

TEST(FormatsEmptyListAsEmptyText) {
  EXPECT_EQ(FormatPaths({}, QuoteMode::kAlways), L"");
}

TEST(ValidatesStoredQuoteModes) {
  EXPECT_TRUE(path_format::IsValidQuoteMode(0));
  EXPECT_TRUE(path_format::IsValidQuoteMode(2));
  EXPECT_TRUE(!path_format::IsValidQuoteMode(3));
}

TEST(DefaultsToQuotingWhenNeeded) {
  EXPECT_TRUE(path_format::kDefaultQuoteMode == QuoteMode::kWhenNeeded);
}

int main() { return test_support::RunAll(); }
