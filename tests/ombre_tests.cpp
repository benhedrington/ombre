#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"

class OmbreTests : public QObject {
    Q_OBJECT

private slots:
    void paletteUsesTheThemeColors()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "darker_background = \"#0e0e14\"\n"
              "background = \"#1a1b26\"\n"
              "lighter_background = \"#24283b\"\n"
              "accent = \"#7aa2f7\"\n"
              "blue = \"#7aa2f7\"\n"
              "bright_cyan = \"#0db9d7\"\n");

        Backend backend(dir.path());
        backend.setSeed(0);
        const QStringList palettes = backend.paletteString().split(';');
        QCOMPARE(palettes.size(), 6);
        QCOMPARE(palettes.first(), QString("#0e0e14,#1a1b26,#24283b,#5c77b5"));
        QVERIFY(palettes.first() != palettes.last());

        // Three base tones plus the two ends of the accent sweep.
        const QStringList swatches = backend.paletteColors();
        QCOMPARE(swatches.size(), 5);
        QCOMPARE(swatches.last(), QString("#1586a0"));
    }

    void paletteFallsBackWithoutATheme()
    {
        QTemporaryDir dir;
        Backend backend(dir.path());
        backend.setSeed(0);
        const QStringList palettes = backend.paletteString().split(';');
        QCOMPARE(palettes.size(), backend.count());
        QCOMPARE(palettes.first(), QString("#13141c,#1a1b26,#24283b,#5c77b5"));
    }

    void paletteModesUseTheirOwnAccents()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "darker_background = \"#101010\"\n"
              "background = \"#111111\"\n"
              "lighter_background = \"#222222\"\n"
              "red = \"#ff0000\"\n"
              "bright_yellow = \"#ffff00\"\n");

        Backend backend(dir.path());
        backend.setPaletteMode("warm");
        const QStringList palettes = backend.paletteString().split(';');
        QCOMPARE(palettes.first(), QString("#101010,#111111,#222222,#b20c0c"));
        QCOMPARE(palettes.last(), QString("#101010,#111111,#222222,#b2b20c"));
        QCOMPARE(backend.paletteMode(), QString("warm"));
    }

    void themeAccentsRotateWithTheSeed()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "darker_background = \"#101010\"\n"
              "background = \"#111111\"\n"
              "lighter_background = \"#222222\"\n"
              "accent = \"#7aa2f7\"\n"
              "blue = \"#7aa2f7\"\n"
              "bright_cyan = \"#0db9d7\"\n"
              "bright_magenta = \"#bb9af7\"\n");

        Backend backend(dir.path());
        backend.setSeed(0);
        const QString first = backend.paletteString().split(';').first();
        backend.setSeed(1);
        const QString second = backend.paletteString().split(';').first();
        QVERIFY(first != second);
    }

    void eachImageGetsItsOwnAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "background = \"#111111\"\n"
              "lighter_background = \"#222222\"\n"
              "red = \"#ff0000\"\n"
              "bright_yellow = \"#ffff00\"\n");

        Backend backend(dir.path());
        backend.setPaletteMode("warm");
        backend.setCount(6);
        const QStringList palettes = backend.paletteString().split(';');
        QCOMPARE(palettes.size(), 6);
        QVERIFY(palettes.first() != palettes.last());
    }

    void accentsAreMutedTowardTheBackground()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "background = \"#111111\"\n"
              "lighter_background = \"#222222\"\n"
              "red = \"#ff0000\"\n"
              "bright_yellow = \"#ffff00\"\n");

        Backend backend(dir.path());
        backend.setPaletteMode("warm");
        QCOMPARE(backend.paletteColors().last(), QString("#b2b20c"));
    }

    void environmentChoosesThePaletteMode()
    {
        qputenv("OMBRE_PALETTE_MODE", "custom");
        QTemporaryDir dir;
        Backend backend(dir.path());
        QVERIFY(backend.paletteModes().contains("custom"));
        QCOMPARE(backend.paletteMode(), QString("custom"));
        qunsetenv("OMBRE_PALETTE_MODE");
    }

    void sizesCoverTheCommonDeskAspectRatios()
    {
        QTemporaryDir dir;
        Backend backend(dir.path());
        for (const auto &id : {"wuxga", "laptop-1610", "qhd", "uw-1440", "4k", "5k", "framework-13", "suw"})
            QVERIFY2(backend.sizes().contains(id), id);
    }

    void grainIsClamped()
    {
        QTemporaryDir dir;
        Backend backend(dir.path());
        backend.setGrain(0.5);
        QCOMPARE(backend.grain(), 0.1);
        backend.setGrain(0.04);
        QCOMPARE(backend.grain(), 0.04);
    }

    void surpriseLeavesThePaletteToTheRenderer()
    {
        QTemporaryDir dir;
        Backend backend(dir.path());
        backend.setPaletteMode("surprise");
        QVERIFY(backend.paletteString().isEmpty());
        QVERIFY(backend.paletteColors().isEmpty());
    }

    void themeNameComesFromThemeDotName()
    {
        QTemporaryDir dir;
        write(dir.path() + "/theme.name", "tokyo-night\n");

        Backend backend(dir.path());
        QCOMPARE(backend.themeName(), QString("tokyo-night"));
    }

    void rerollKeepsTheSeedInRange()
    {
        QTemporaryDir dir;
        Backend backend(dir.path());
        backend.reroll();
        QVERIFY(backend.seed() >= 1);
        QVERIFY(backend.seed() < 1000000);
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(OmbreTests)
#include "ombre_tests.moc"
