#ifndef TESTSPELLCHECK_H
#define TESTSPELLCHECK_H

#include <QObject>
#include <qtest_kde.h>

class TestSpellCheck : public QObject
{
    Q_OBJECT
public:
    TestSpellCheck() {}

private Q_SLOTS:
    void testFetchMoreText();
    void testFetchMoreText2();
};

#endif
