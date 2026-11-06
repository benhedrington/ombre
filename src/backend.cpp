#include "backend.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString lastErrorLine(const QByteArray &text) {
    const auto lines = QString::fromUtf8(text).trimmed().split('\n', Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.last().trimmed();
}

QString mixedColor(const QString &from, const QString &to, double t) {
    QColor a(from), b(to);
    if (!a.isValid())
        a = b;
    if (!b.isValid())
        b = a;
    if (!a.isValid())
        return QStringLiteral("#7aa2f7");
    const auto mix = [t](int x, int y) { return qRound(x + (y - x) * t); };
    return QColor(mix(a.red(), b.red()), mix(a.green(), b.green()), mix(a.blue(), b.blue())).name();
}
} // namespace

Backend::Backend(const QString &currentDirectory, QObject *parent)
    : QObject(parent),
      m_currentDirectory(currentDirectory.isEmpty()
                             ? QDir::homePath() + "/.local/state/omarchy/current"
                             : currentDirectory),
      m_previewDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/preview"),
      m_seed(QRandomGenerator::global()->bounded(1, 1000000)) {
    m_status = tr("Press Space to generate");
    const QString mode = qEnvironmentVariable("OMBRE_PALETTE_MODE");
    if (!mode.isEmpty() && paletteModes().contains(mode))
        m_paletteMode = mode;
    refreshTheme();

    const auto colors = readColors();
    m_customColors = {
        colors.value("background", "#1a1b26"),
        colors.value("lighter_background", "#24283b"),
        colors.value("accent", "#7aa2f7"),
        colors.value("bright_cyan", "#0db9d7"),
    };
    const QStringList saved = QSettings().value("customColors").toStringList();
    if (saved.size() == 4) {
        m_customColors = saved;
    }
}

Backend::~Backend() {
    if (m_generate && m_generate->state() != QProcess::NotRunning) {
        m_generate->kill();
        m_generate->waitForFinished(1000);
    }
    if (m_action && m_action->state() != QProcess::NotRunning) {
        m_action->kill();
        m_action->waitForFinished(1000);
    }
}

QStringList Backend::styles() const {
    return {"any", "linear", "glow", "aurora", "mesh", "beam", "orb", "horizon", "split"};
}

QStringList Backend::paletteModes() const {
    return {"theme", "cool", "warm", "mono", "custom", "surprise"};
}

QStringList Backend::sizes() const {
    // Smallest to largest by pixel count, so the dropdown reads as one scale.
    return {"hd", "wuxga", "framework-13", "qhd", "laptop-1610", "uw-1440",
            "suw", "macbook-pro-16", "4k", "16x10-4k", "5k"};
}

QStringList Backend::paletteColors() const {
    const auto palettes = batchPalettes();
    if (palettes.isEmpty())
        return {};

    // Three base tones plus the first and last accent, so the header shows the
    // sweep the set will walk rather than a single frozen colour.
    QStringList colors = palettes.first().split(',');
    QStringList result = colors.mid(0, 3);
    result << colors.last();
    if (palettes.size() > 1) {
        const QString last = palettes.last().split(',').last();
        if (last != colors.last())
            result << last;
    }
    return result;
}

void Backend::setStyle(const QString &style) {
    if (style == m_style || !styles().contains(style))
        return;
    m_style = style;
    emit styleChanged();
}

void Backend::setGrain(double grain) {
    grain = qBound(0.0, grain, 0.1);
    if (qFuzzyCompare(grain, m_grain))
        return;
    m_grain = grain;
    emit grainChanged();
}

void Backend::setSize(const QString &size) {
    if (size == m_size || !sizes().contains(size))
        return;
    m_size = size;
    emit sizeChanged();
}

void Backend::setCount(int count) {
    count = qBound(1, count, 24);
    if (count == m_count)
        return;
    m_count = count;
    emit countChanged();
    emit paletteChanged();
}

void Backend::setPaletteMode(const QString &mode) {
    if (mode == m_paletteMode || !paletteModes().contains(mode))
        return;
    m_paletteMode = mode;
    emit paletteModeChanged();
    emit paletteChanged();
}

void Backend::setSeed(int seed) {
    if (seed == m_seed)
        return;
    m_seed = seed;
    emit seedChanged();
    emit paletteChanged();
}

void Backend::reroll() {
    setSeed(QRandomGenerator::global()->bounded(1, 1000000));
}

void Backend::generateNew() {
    reroll();
    generate();
}

void Backend::generate() {
    if (m_busy)
        return;

    refreshTheme();
    const QString script = scriptPath();
    if (script.isEmpty()) {
        fail(tr("Could not find gradgen.py. Reinstall Ombre or set GRADGEN_SCRIPT."));
        return;
    }

    QString python = qEnvironmentVariable("GRADGEN_PYTHON");
    if (python.isEmpty())
        python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty()) {
        fail(tr("python3 is not installed."));
        return;
    }

    QDir(m_previewDirectory).removeRecursively();
    if (!QDir().mkpath(m_previewDirectory)) {
        fail(tr("Could not create %1").arg(m_previewDirectory));
        return;
    }

    m_lastSeed = m_seed;
    m_usedUv = false;
    m_previews.clear();
    emit previewsChanged();
    setBusy(true);
    setStatus(tr("Generating %n wallpapers…", "", m_count));

    QStringList arguments{
        script,
        "-n", QString::number(m_count),
        "-s", m_size,
        "-f", "webp",
        "-q", "92",
        "--style", m_style,
        "--grain", QString::number(m_grain, 'f', 3),
        "--seed", QString::number(m_seed),
        "-o", m_previewDirectory,
    };
    if (m_paletteMode == "surprise") {
        arguments << "--palette-seed" << QString::number(m_seed)
                  << "--palette-vary" << "--soft" << "--dim";
    } else {
        arguments << "--palette" << paletteString();
    }

    startGeneration(python, arguments);
}

void Backend::cancel() {
    if (!m_busy || !m_generate || m_generate->state() == QProcess::NotRunning)
        return;
    m_generate->kill();
    setBusy(false);
    setStatus(tr("Cancelled"));
}

void Backend::startGeneration(const QString &program, const QStringList &arguments) {
    if (!m_generate) {
        m_generate = new QProcess(this);
        connect(m_generate, &QProcess::finished, this, &Backend::generationFinished);
        connect(m_generate, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
                fail(tr("Could not start the renderer: %1").arg(m_generate->errorString()));
        });
    }
    m_generate->start(program, arguments);
}

void Backend::generationFinished(int exitCode, QProcess::ExitStatus status) {
    const QByteArray error = m_generate->readAllStandardError();

    if (status == QProcess::NormalExit && exitCode == 0) {
        const auto files = QDir(m_previewDirectory)
                               .entryInfoList({"*.webp"}, QDir::Files, QDir::Name);
        if (files.isEmpty()) {
            fail(tr("The renderer returned no images."));
            return;
        }
        for (const auto &file : files)
            m_previews << file.absoluteFilePath();
        emit previewsChanged();
        emit lastSeedChanged();
        setBusy(false);
        setStatus(tr("%n wallpapers · seed %1", "", static_cast<int>(m_previews.size()))
                      .arg(m_lastSeed));
        return;
    }

    // Arch has python-numpy and python-pillow; other systems and this dev
    // machine may not, and uv can fetch them into a throwaway environment.
    if (!m_usedUv
        && (error.contains("No module named 'numpy'") || error.contains("No module named 'PIL'"))) {
        const QString uv = QStandardPaths::findExecutable("uv");
        if (!uv.isEmpty()) {
            m_usedUv = true;
            setStatus(tr("Generating %n wallpapers…", "", m_count));
            QStringList arguments{"run", "--quiet", "--with", "numpy", "--with", "pillow", "python"};
            arguments += m_generate->arguments();
            startGeneration(uv, arguments);
            return;
        }
    }

    const QString message = lastErrorLine(error);
    fail(message.isEmpty() ? tr("Generation failed (exit %1)").arg(exitCode)
                           : tr("Generation failed: %1").arg(message));
}

void Backend::setCustomColor(int index, const QString &color) {
    if (index < 0 || index >= 4)
        return;
    const QColor chosen(color);
    if (!chosen.isValid())
        return;
    if (m_customColors.size() != 4)
        m_customColors = {"#13141c", "#24283b", "#7aa2f7", "#0db9d7"};
    m_customColors[index] = chosen.name();
    QSettings().setValue("customColors", m_customColors);
    emit customColorsChanged();
    emit paletteChanged();
}

void Backend::apply(const QString &path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        fail(tr("That wallpaper is gone; generate again."));
        return;
    }

    const QString command = QStandardPaths::findExecutable("omarchy-theme-bg-set");
    if (command.isEmpty()) {
        fail(tr("Omarchy's background command is not installed."));
        return;
    }

    // The preview directory is wiped whenever a new set is generated, so keep
    // the chosen image somewhere stable before pointing the desktop at it.
    const QString directory = QDir::homePath() + "/.local/share/ombre/applied";
    if (!QDir().mkpath(directory)) {
        fail(tr("Could not create %1").arg(directory));
        return;
    }
    QString stable = path;
    if (QFileInfo(path).absolutePath() != directory) {
        stable = directory + "/" + QFileInfo(path).fileName();
        QFile::remove(stable);
        if (!QFile::copy(path, stable)) {
            fail(tr("Could not keep a copy of %1").arg(QFileInfo(path).fileName()));
            return;
        }
    }

    if (!m_action) {
        m_action = new QProcess(this);
        connect(m_action, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
            if (code == 0 && status == QProcess::NormalExit) {
                setStatus(tr("Background applied"));
                return;
            }
            const QString message = lastErrorLine(m_action->readAllStandardError());
            fail(message.isEmpty() ? tr("Could not apply the background.") : message);
        });
    }
    if (m_action->state() != QProcess::NotRunning)
        m_action->kill();
    m_action->start(command, {stable});
}

void Backend::saveSet() {
    if (m_previews.isEmpty()) {
        fail(tr("Generate a set first."));
        return;
    }

    refreshTheme();
    const QString destination = QDir::homePath() + "/.config/omarchy/backgrounds/" + themeSlug();
    if (!QDir().mkpath(destination)) {
        fail(tr("Could not create %1").arg(destination));
        return;
    }

    int saved = 0;
    for (int i = 0; i < static_cast<int>(m_previews.size()); ++i) {
        const QString name = QString("ombre-%1-%2-%3-%4.webp")
                                 .arg(m_paletteMode, m_style, QString::number(m_lastSeed))
                                 .arg(i + 1, 2, 10, QChar('0'));
        const QString target = destination + "/" + name;
        QFile::remove(target);
        if (QFile::copy(m_previews[i], target))
            ++saved;
    }

    if (saved == 0) {
        fail(tr("Could not copy the wallpapers into %1").arg(destination));
        return;
    }

    if (const QString cache = QStandardPaths::findExecutable("omarchy-theme-bg-cache"); !cache.isEmpty())
        QProcess::startDetached(cache, {});

    setStatus(tr("Saved %n wallpapers to %1", "", saved).arg(destination));
}

QStringList Backend::batchPalettes() const {
    if (m_paletteMode == "surprise")
        return {};

    if (m_paletteMode == "custom") {
        const QString dark = m_customColors.value(0, "#13141c");
        const QString light = m_customColors.value(1, "#24283b");
        const QString accentA = m_customColors.value(2, "#7aa2f7");
        const QString accentB = m_customColors.value(3, "#0db9d7");
        const QString mid = mixedColor(dark, light, 0.5);
        const int count = qMax(1, m_count);
        QStringList palettes;
        for (int i = 0; i < count; ++i) {
            const double t = count == 1 ? 0.0 : double(i) / double(count - 1);
            palettes << QStringList{dark, mid, light, mixedColor(accentA, accentB, t)}.join(',');
        }
        return palettes;
    }

    const auto colors = readColors();
    const auto get = [&colors](const char *key, const QString &fallback) {
        return colors.value(QLatin1String(key), fallback);
    };

    const QString dark = get("darker_background", "#13141c");
    const QString background = get("background", "#1a1b26");
    const QString lighter = get("lighter_background", "#24283b");

    QString first, last;
    if (m_paletteMode == "mono") {
        first = get("muted", "#414868");
        last = get("foreground", "#a9b1d6");
    } else if (m_paletteMode == "cool") {
        first = get("blue", "#7aa2f7");
        last = get("bright_cyan", "#0db9d7");
    } else if (m_paletteMode == "warm") {
        first = get("red", "#f7768e");
        last = get("bright_yellow", "#ff9e64");
    } else {
        // Theme mode cycles the theme's own accent families by seed, so every
        // Space is a different hue while the dark base stays recognisable.
        struct Family { const char *first; const char *last; };
        static const Family families[] = {
            {"blue", "bright_cyan"},
            {"bright_magenta", "blue"},
            {"bright_cyan", "blue"},
            {"red", "bright_yellow"},
            {"yellow", "red"},
            {"green", "bright_cyan"},
            {"orange", "bright_magenta"},
        };
        const QString accent = get("accent", "#7aa2f7");
        const int index = m_seed % int(sizeof(families) / sizeof(families[0]));
        first = get(families[index].first, accent);
        last = get(families[index].last, accent);
    }

    // Wallpapers sit behind everything, so the accent is pulled toward the
    // theme's lighter background: still the same hue, lower contrast.
    first = mixedColor(first, lighter, 0.35);
    last = mixedColor(last, lighter, 0.35);

    // The set sweeps from the first accent to the second, so the six images
    // belong together without being the same colour.
    const int count = qMax(1, m_count);
    QStringList palettes;
    for (int i = 0; i < count; ++i) {
        const double t = count == 1 ? 0.0 : double(i) / double(count - 1);
        palettes << QStringList{dark, background, lighter, mixedColor(first, last, t)}.join(',');
    }
    return palettes;
}

QString Backend::paletteString() const {
    return batchPalettes().join(';');
}

QHash<QString, QString> Backend::readColors() const {
    QHash<QString, QString> colors;
    QFile file(m_currentDirectory + "/theme/colors.toml");
    if (!file.open(QIODevice::ReadOnly))
        return colors;

    const QRegularExpression entry(R"re(^\s*([A-Za-z_]+)\s*=\s*["'](#[0-9a-fA-F]{3,8})["'])re",
                                   QRegularExpression::MultilineOption);
    const QString text = QString::fromUtf8(file.readAll());
    auto it = entry.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        colors.insert(match.captured(1), match.captured(2));
    }
    return colors;
}

void Backend::refreshTheme() {
    QFile file(m_currentDirectory + "/theme.name");
    QString name = "Omarchy";
    if (file.open(QIODevice::ReadOnly)) {
        const QString value = QString::fromUtf8(file.readAll()).trimmed();
        if (!value.isEmpty())
            name = value;
    }
    if (name != m_themeName) {
        m_themeName = name;
        emit themeNameChanged();
        emit paletteChanged();
    }
}

QString Backend::themeSlug() const {
    QString slug = m_themeName.toLower();
    slug.replace(QRegularExpression("[^a-z0-9._-]+"), "-");
    slug.remove(QRegularExpression("^-+|-+$"));
    return slug.isEmpty() ? QStringLiteral("custom") : slug;
}

QString Backend::scriptPath() const {
    const QString override = qEnvironmentVariable("GRADGEN_SCRIPT");
    if (!override.isEmpty() && QFileInfo::exists(override))
        return override;

    const QString installed =
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, "ombre/gradgen.py");
    if (!installed.isEmpty())
        return installed;

    const QString besideBuild =
        QFileInfo(QCoreApplication::applicationDirPath() + "/../backend/gradgen.py").absoluteFilePath();
    if (QFileInfo::exists(besideBuild))
        return besideBuild;

#ifdef OMBRE_SOURCE_DIR
    const QString source = QStringLiteral(OMBRE_SOURCE_DIR) + "/backend/gradgen.py";
    if (QFileInfo::exists(source))
        return source;
#endif

    return {};
}

void Backend::fail(const QString &message) {
    setBusy(false);
    setStatus(message);
}

void Backend::setStatus(const QString &status) {
    if (status == m_status)
        return;
    m_status = status;
    emit statusChanged();
}

void Backend::setBusy(bool busy) {
    if (busy == m_busy)
        return;
    m_busy = busy;
    emit busyChanged();
}
