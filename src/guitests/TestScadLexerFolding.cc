#include "TestScadLexerFolding.h"

#include <QList>
#include <QTest>

#include "gui/ScintillaEditor.h"

namespace {

int rawFoldLevel(QsciScintilla *qsci, int line)
{
  return qsci->SendScintilla(QsciScintilla::SCI_GETFOLDLEVEL, line);
}

// Fold depth of 'line', relative to 'baseLine' (line 0 by default).
// Comparing relative to a baseline -- rather than asserting an absolute
// depth -- avoids depending on Scintilla's internal SC_FOLDLEVELBASE
// offset, which ScadLexer2::fold() itself never hardcodes either; it
// only ever reasons about level *deltas*.
int foldDepth(QsciScintilla *qsci, int line, int baseLine = 0)
{
  const int base = rawFoldLevel(qsci, baseLine) & QsciScintilla::SC_FOLDLEVELNUMBERMASK;
  const int level = rawFoldLevel(qsci, line) & QsciScintilla::SC_FOLDLEVELNUMBERMASK;
  return level - base;
}

bool isFoldHeader(QsciScintilla *qsci, int line)
{
  return rawFoldLevel(qsci, line) & QsciScintilla::SC_FOLDLEVELHEADERFLAG;
}

}  // namespace

void TestScadLexerFolding::testFolding_data()
{
  QTest::addColumn<QString>("source");
  QTest::addColumn<QList<int>>("expectedDepths");   // one entry per line
  QTest::addColumn<QList<int>>("expectedHeaders");  // line indices expected to be fold headers

  // clang-format off

  QTest::newRow("braces")
    << R"(
        module box() {
          cube(1);
        }
        x = 1;
      )"
    << QList<int>{0, 1, 1, 0}
    << QList<int>{0};

  QTest::newRow("brackets")
    << R"(
        x = [
          1,
          2
        ];
        y = 1;
      )"
    << QList<int>{0, 1, 1, 1, 0}
    << QList<int>{0};

  QTest::newRow("parens")
    << R"(
        translate(
          [1, 0, 0]
        ) cube(1);
        y = 1;
      )"
    << QList<int>{0, 1, 1, 0}
    << QList<int>{0};

  QTest::newRow("function_with_let")
    << R"(
        function foo(x) =
        let(
                a = 1
            )
            x + a;
        y = 1;
      )"
    << QList<int>{0, 1, 2, 2, 1, 0}
    << QList<int>{0, 1};

  QTest::newRow("function_single_line")
    << R"(
        function inc(x) = x + 1;
        y = 1;
      )"
    << QList<int>{0, 0}
    << QList<int>{};

  QTest::newRow("function_with_object")
    << R"(
        function foo(x) =
          let(
              a = 1
          )
          object(
              key = a
          );
        y = 1;
      )"
    << QList<int>{0, 1, 2, 2, 1, 2, 2, 0}
    << QList<int>{0, 1, 4};

  // A more complex nesting case: brackets and parens that open *and* close
  // again within a single line should net to zero and not produce a spurious
  // fold header there, even though the surrounding module still folds on its
  // braces as usual.
  QTest::newRow("nested_module_call")
    << R"(
        module box(size=[1,1,1]) {
          translate([0,0,0])
            cube(size);
        }
        x = 1;
      )"
    << QList<int>{0, 1, 1, 1, 0}
    << QList<int>{0};

  // clang-format on
}

void TestScadLexerFolding::testFolding()
{
  restoreWindowInitialState();
  window->designActionAutoReload->setChecked(false);  // only folding matters here

  QFETCH(const QString, source);
  QFETCH(const QList<int>, expectedDepths);
  QFETCH(const QList<int>, expectedHeaders);

  auto *const editor = dynamic_cast<ScintillaEditor *>(window->activeEditor);
  QVERIFY(editor != nullptr);

  editor->setPlainText(source.trimmed());
  editor->resetHighlighting();

  for (auto line = 0; line < expectedDepths.size(); ++line) {
    const auto actualFolding = std::tuple{line, isFoldHeader(editor->qsci, line)};
    const auto expectedFolding = std::tuple{line, expectedHeaders.contains(line)};
    QCOMPARE(actualFolding, expectedFolding);

    const auto actualDepth = std::tuple{line, foldDepth(editor->qsci, line)};
    const auto expectedDepth = std::tuple{line, expectedDepths[line]};
    QCOMPARE(actualDepth, expectedDepth);
  }
}
