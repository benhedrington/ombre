#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QStringList>

// The app's one job: ask backend/gradgen.py for a set of candidates, show them,
// and hand the chosen one to Omarchy. Everything that touches the disk or a
// process lives here; QML only lays it out.
class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QStringList previews READ previews NOTIFY previewsChanged)
    Q_PROPERTY(QStringList paletteColors READ paletteColors NOTIFY paletteChanged)
    Q_PROPERTY(QStringList customColors READ customColors NOTIFY customColorsChanged)
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeNameChanged)
    Q_PROPERTY(int lastSeed READ lastSeed NOTIFY lastSeedChanged)

    Q_PROPERTY(QStringList styles READ styles CONSTANT)
    Q_PROPERTY(QStringList paletteModes READ paletteModes CONSTANT)
    Q_PROPERTY(QStringList sizes READ sizes CONSTANT)

    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(double grain READ grain WRITE setGrain NOTIFY grainChanged)
    Q_PROPERTY(QString size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(QString paletteMode READ paletteMode WRITE setPaletteMode NOTIFY paletteModeChanged)
    Q_PROPERTY(int seed READ seed NOTIFY seedChanged)

public:
    explicit Backend(const QString &currentDirectory = {}, QObject *parent = nullptr);
    ~Backend() override;

    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    QStringList previews() const { return m_previews; }
    QStringList paletteColors() const;
    QStringList customColors() const { return m_customColors; }
    QString themeName() const { return m_themeName; }
    int lastSeed() const { return m_lastSeed; }

    QStringList styles() const;
    QStringList paletteModes() const;
    QStringList sizes() const;

    QString style() const { return m_style; }
    double grain() const { return m_grain; }
    QString size() const { return m_size; }
    int count() const { return m_count; }
    QString paletteMode() const { return m_paletteMode; }
    int seed() const { return m_seed; }

    void setStyle(const QString &style);
    void setGrain(double grain);
    void setSize(const QString &size);
    void setCount(int count);
    void setPaletteMode(const QString &mode);
    void setSeed(int seed);

    // The palette as it will be passed to the renderer, e.g.
    // "#0e0e14,#1a1b26,#24283b,#7aa2f7". Exposed for tests.
    Q_INVOKABLE QString paletteString() const;

    Q_INVOKABLE void generate();
    Q_INVOKABLE void generateNew();
    Q_INVOKABLE void reroll();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void setCustomColor(int index, const QString &color);
    Q_INVOKABLE void apply(const QString &path);
    Q_INVOKABLE void saveSet();

signals:
    void statusChanged();
    void busyChanged();
    void previewsChanged();
    void paletteChanged();
    void customColorsChanged();
    void themeNameChanged();
    void lastSeedChanged();
    void styleChanged();
    void grainChanged();
    void sizeChanged();
    void countChanged();
    void paletteModeChanged();
    void seedChanged();

private:
    void refreshTheme();
    QHash<QString, QString> readColors() const;
    QStringList batchPalettes() const;
    QString scriptPath() const;
    void startGeneration(const QString &program, const QStringList &arguments);
    void generationFinished(int exitCode, QProcess::ExitStatus status);
    void fail(const QString &message);
    void setStatus(const QString &status);
    void setBusy(bool busy);
    QString themeSlug() const;
    QString savedName(int index) const;

    QString m_currentDirectory;
    QString m_status;
    bool m_busy = false;
    QStringList m_previews;
    QString m_previewDirectory;

    QString m_style = "any";
    double m_grain = 0.01;
    QString m_size = "wuxga";
    int m_count = 6;
    QString m_paletteMode = "theme";
    QStringList m_customColors;
    int m_seed = 1;
    int m_lastSeed = 0;
    int m_batchSeed = 1;
    QString m_batchPaletteMode = "theme";
    QString m_batchStyle = "any";
    QString m_themeName = "Omarchy";

    QProcess *m_generate = nullptr;
    QProcess *m_action = nullptr;
    bool m_usedUv = false;
    QString m_actionPath;
};
