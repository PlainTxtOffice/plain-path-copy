// Unit tests for the feedback email draft.

#include "feedback.h"
#include "test-support.h"

using feedback::Kind;

TEST(SubjectNamesAppKindAndPlatform) {
  EXPECT_EQ(feedback::Subject(L"Plain Path Copy", Kind::kBugReport),
            L"Plain Path Copy bug report (Windows)");
  EXPECT_EQ(feedback::Subject(L"Plain Path Copy", Kind::kFeatureRequest),
            L"Plain Path Copy feature request (Windows)");
}

TEST(SummaryHasAppWindowsAndArchitectureLines) {
  EXPECT_EQ(feedback::FormatSystemSummary(L"Plain Path Copy", L"1.0.0", L"11",
                                          L"10.0.26100", L"x64"),
            L"Plain Path Copy 1.0.0\nWindows 11 (10.0.26100)\nx64");
}

TEST(SystemSummaryStartsWithAppNameAndVersion) {
  const std::wstring summary = feedback::SystemSummary();
  EXPECT_TRUE(summary.starts_with(L"Plain Path Copy 1."));
  EXPECT_TRUE(summary.find(L"\nWindows 1") != std::wstring::npos);
}

TEST(PercentEncodesReservedAndNonAsciiCharacters) {
  EXPECT_EQ(feedback::PercentEncode(L"a b&c=d/é~._-"),
            L"a%20b%26c%3Dd%2F%C3%A9~._-");
}

TEST(MailtoUsesCrLfAndBlankLinesBeforeSummary) {
  EXPECT_EQ(
      feedback::MailtoUrl(L"Plain Path Copy", Kind::kBugReport,
                          L"Plain Path Copy 1.0.0\nWindows 11 (10.0.1)\nx64"),
      L"mailto:support@plaintxtoffice.com"
      L"?subject=Plain%20Path%20Copy%20bug%20report%20%28Windows%29"
      L"&body=%0D%0A%0D%0APlain%20Path%20Copy%201.0.0%0D%0AWindows%2011%20"
      L"%2810.0.1%29%0D%0Ax64");
}

int main() { return test_support::RunAll(); }
