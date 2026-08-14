// SPDX-License-Identifier: BSD-3-Clause
#include "settings.hpp"

#include <QColor>
#include <QDBusMetaType>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

QDBusArgument &operator<<(QDBusArgument &argument, const PortalSettings &settings)
{
    argument.beginMap(QMetaType::fromType<QString>(), QMetaType::fromType<QVariantMap>());
    for (auto it = settings.cbegin(); it != settings.cend(); ++it) {
        argument.beginMapEntry();
        argument << it.key() << it.value();
        argument.endMapEntry();
    }
    argument.endMap();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalSettings &settings)
{
    settings.clear();
    argument.beginMap();
    while (!argument.atEnd()) {
        QString nameSpace;
        QVariantMap values;
        argument.beginMapEntry();
        argument >> nameSpace >> values;
        argument.endMapEntry();
        settings.insert(nameSpace, values);
    }
    argument.endMap();
    return argument;
}

namespace
{
constexpr auto Appearance = "org.freedesktop.appearance";
constexpr auto GnomeInterface = "org.gnome.desktop.interface";
constexpr auto GnomeFontconfig = "org.gnome.fontconfig";

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2
        && ((value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))
            || (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"'))))) {
        value = value.sliced(1, value.size() - 2);
    }
    return value;
}

uint schemeFromString(const QString &value)
{
    const auto folded = value.toCaseFolded();
    if (folded.contains(QStringLiteral("dark"))) {
        return 1U;
    }
    if (folded.contains(QStringLiteral("light"))) {
        return 2U;
    }
    return 0U;
}

QString kdeFontName(const QVariant &value)
{
    const auto description = value.toString();
    if (description.isEmpty()) {
        return {};
    }

    QFont font;
    if (font.fromString(description)) {
        return QStringLiteral("%1 %2").arg(font.family()).arg(font.pointSize());
    }
    return description;
}

QString fontHintingFromGtk(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("hint"))) {
        value.remove(0, 4);
    }
    if (value == QStringLiteral("none") || value == QStringLiteral("slight")
        || value == QStringLiteral("medium") || value == QStringLiteral("full")) {
        return value;
    }
    return {};
}

QString rgbaOrder(QString value)
{
    value = value.trimmed().toLower();
    if (value == QStringLiteral("rgba") || value == QStringLiteral("rgb")
        || value == QStringLiteral("bgr") || value == QStringLiteral("vrgb")
        || value == QStringLiteral("vbgr")) {
        return value;
    }
    return {};
}
}

Settings::Settings(QObject &parent)
    : QDBusAbstractAdaptor(&parent)
{
    setAutoRelaySignals(true);
    qDBusRegisterMetaType<PortalSettings>();

    const auto config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    m_configFiles = {
        QDir(config).filePath(QStringLiteral("kdeglobals")),
        QDir(config).filePath(QStringLiteral("gtk-3.0/settings.ini")),
        QDir(config).filePath(QStringLiteral("gtk-4.0/settings.ini")),
        QDir(config).filePath(QStringLiteral("dconf/user"))};
    m_configDirectories = {
        config,
        QDir(config).filePath(QStringLiteral("gtk-3.0")),
        QDir(config).filePath(QStringLiteral("gtk-4.0")),
        QDir(config).filePath(QStringLiteral("dconf"))};

    const auto genericCache =
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    const auto genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    m_fontconfigFiles = {
        QDir(config).filePath(QStringLiteral("fontconfig/fonts.conf")),
        QDir::home().filePath(QStringLiteral(".fonts.conf"))};
    m_fontconfigDirectories = {
        QDir(config).filePath(QStringLiteral("fontconfig")),
        QDir(genericCache).filePath(QStringLiteral("fontconfig")),
        QDir(genericData).filePath(QStringLiteral("fonts")),
        QDir::home().filePath(QStringLiteral(".fonts"))};

    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(150);
    connect(&m_reloadTimer, &QTimer::timeout, this, &Settings::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &Settings::scheduleReload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Settings::scheduleReload);
    m_values = readHostSettings();
    refreshWatches();
}

QDBusVariant Settings::Read(const QString &nameSpace, const QString &key)
{
    const auto namespaceIt = m_values.constFind(nameSpace);
    if (namespaceIt == m_values.cend() || !namespaceIt->contains(key)) {
        sendErrorReply(QStringLiteral("org.freedesktop.portal.Error.NotFound"),
                       QStringLiteral("Unknown setting %1.%2").arg(nameSpace, key));
        return {};
    }
    return QDBusVariant(namespaceIt->value(key));
}

PortalSettings Settings::ReadAll(const QStringList &namespaces) const
{
    PortalSettings result;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
        if (namespaceMatches(it.key(), namespaces)) {
            result.insert(it.key(), it.value());
        }
    }
    return result;
}

void Settings::scheduleReload(const QString &path)
{
    if (m_fontconfigFiles.contains(path) || m_fontconfigDirectories.contains(path)) {
        m_fontconfigChanged = true;
    }
    m_reloadTimer.start();
}

void Settings::reload()
{
    if (m_fontconfigChanged) {
        ++m_fontconfigSerial;
        m_fontconfigChanged = false;
    }
    const auto updated = readHostSettings();
    for (auto namespaceIt = updated.cbegin(); namespaceIt != updated.cend(); ++namespaceIt) {
        const auto oldNamespace = m_values.value(namespaceIt.key());
        for (auto keyIt = namespaceIt->cbegin(); keyIt != namespaceIt->cend(); ++keyIt) {
            if (!oldNamespace.contains(keyIt.key()) || oldNamespace.value(keyIt.key()) != keyIt.value()) {
                emit SettingChanged(namespaceIt.key(), keyIt.key(), QDBusVariant(keyIt.value()));
            }
        }
    }
    m_values = updated;
    refreshWatches();
}

QString Settings::gsettings(const QString &schema, const QString &key)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), {QStringLiteral("get"), schema, key});
    if (!process.waitForStarted(250) || !process.waitForFinished(750)) {
        process.kill();
        process.waitForFinished(100);
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return {};
    }
    return unquote(QString::fromUtf8(process.readAllStandardOutput()));
}

bool Settings::namespaceMatches(const QString &nameSpace, const QStringList &patterns)
{
    if (patterns.isEmpty() || patterns.contains(QString{})) {
        return true;
    }
    for (const auto &pattern : patterns) {
        if (pattern.endsWith(QLatin1Char('*'))) {
            if (nameSpace.startsWith(pattern.first(pattern.size() - 1))) {
                return true;
            }
        } else if (pattern == nameSpace) {
            return true;
        }
    }
    return false;
}

PortalSettings Settings::readHostSettings() const
{
    QString iconTheme;
    QString fontName;
    QString gtkTheme;
    QString fontAntialiasing;
    QString fontHinting;
    QString fontRgbaOrder;
    uint colorScheme = 0U;

    const QFileInfo kdeFile(m_configFiles.at(0));
    if (kdeFile.isFile()) {
        QSettings kde(kdeFile.filePath(), QSettings::IniFormat);
        kde.beginGroup(QStringLiteral("General"));
        fontName = kdeFontName(kde.value(QStringLiteral("font")));
        const auto kdeScheme = kde.value(QStringLiteral("ColorScheme")).toString();
        if (kde.contains(QStringLiteral("XftHintStyle"))) {
            fontHinting =
                fontHintingFromGtk(kde.value(QStringLiteral("XftHintStyle")).toString());
        }
        if (kde.contains(QStringLiteral("XftSubPixel"))) {
            fontRgbaOrder = rgbaOrder(kde.value(QStringLiteral("XftSubPixel")).toString());
        }
        if (kde.contains(QStringLiteral("XftAntialias"))) {
            if (!kde.value(QStringLiteral("XftAntialias")).toBool()) {
                fontAntialiasing = QStringLiteral("none");
            } else if (!fontRgbaOrder.isEmpty()) {
                fontAntialiasing = QStringLiteral("rgba");
            } else {
                fontAntialiasing = QStringLiteral("grayscale");
            }
        }
        kde.endGroup();
        kde.beginGroup(QStringLiteral("Icons"));
        iconTheme = kde.value(QStringLiteral("Theme")).toString();
        kde.endGroup();
        colorScheme = schemeFromString(kdeScheme);

        if (colorScheme == 0U) {
            kde.beginGroup(QStringLiteral("Colors:Window"));
            const QColor background(kde.value(QStringLiteral("BackgroundNormal")).toString());
            kde.endGroup();
            if (background.isValid()) {
                colorScheme = background.lightnessF() < 0.5F ? 1U : 2U;
            }
        }
    }

    const auto readGtkSettings = [&](const QString &path) {
        const QFileInfo gtkFile(path);
        if (!gtkFile.isFile()) {
            return;
        }

        QSettings gtk(path, QSettings::IniFormat);
        gtk.beginGroup(QStringLiteral("Settings"));
        if (iconTheme.isEmpty()) {
            iconTheme = gtk.value(QStringLiteral("gtk-icon-theme-name")).toString();
        }
        if (fontName.isEmpty()) {
            fontName = gtk.value(QStringLiteral("gtk-font-name")).toString();
        }
        if (gtkTheme.isEmpty()) {
            gtkTheme = gtk.value(QStringLiteral("gtk-theme-name")).toString();
        }
        if (fontHinting.isEmpty()) {
            fontHinting = fontHintingFromGtk(
                gtk.value(QStringLiteral("gtk-xft-hintstyle")).toString());
        }
        if (fontRgbaOrder.isEmpty()) {
            fontRgbaOrder = rgbaOrder(gtk.value(QStringLiteral("gtk-xft-rgba")).toString());
        }
        if (fontAntialiasing.isEmpty()
            && gtk.contains(QStringLiteral("gtk-xft-antialias"))) {
            if (!gtk.value(QStringLiteral("gtk-xft-antialias")).toBool()) {
                fontAntialiasing = QStringLiteral("none");
            } else if (!fontRgbaOrder.isEmpty()) {
                fontAntialiasing = QStringLiteral("rgba");
            } else {
                fontAntialiasing = QStringLiteral("grayscale");
            }
        }
        if (colorScheme == 0U
            && gtk.value(QStringLiteral("gtk-application-prefer-dark-theme")).toBool()) {
            colorScheme = 1U;
        }
        gtk.endGroup();
    };

    readGtkSettings(m_configFiles.at(1));
    readGtkSettings(m_configFiles.at(2));

    if (colorScheme == 0U) {
        colorScheme = schemeFromString(gsettings(QString::fromLatin1(GnomeInterface),
                                                  QStringLiteral("color-scheme")));
    }
    if (iconTheme.isEmpty()) {
        iconTheme = gsettings(QString::fromLatin1(GnomeInterface), QStringLiteral("icon-theme"));
    }
    if (fontName.isEmpty()) {
        fontName = gsettings(QString::fromLatin1(GnomeInterface), QStringLiteral("font-name"));
    }
    if (gtkTheme.isEmpty()) {
        gtkTheme = gsettings(QString::fromLatin1(GnomeInterface), QStringLiteral("gtk-theme"));
    }
    if (colorScheme == 0U) {
        colorScheme = schemeFromString(gtkTheme);
    }
    if (fontAntialiasing.isEmpty()) {
        fontAntialiasing = gsettings(QString::fromLatin1(GnomeInterface),
                                     QStringLiteral("font-antialiasing"));
    }
    if (fontHinting.isEmpty()) {
        fontHinting = gsettings(QString::fromLatin1(GnomeInterface),
                                QStringLiteral("font-hinting"));
    }
    if (fontRgbaOrder.isEmpty()) {
        fontRgbaOrder = gsettings(QString::fromLatin1(GnomeInterface),
                                  QStringLiteral("font-rgba-order"));
    }

    return {
        {QString::fromLatin1(Appearance),
         {{QStringLiteral("color-scheme"), colorScheme}}},
        {QString::fromLatin1(GnomeFontconfig),
         {{QStringLiteral("serial"), m_fontconfigSerial}}},
        {QString::fromLatin1(GnomeInterface),
         {{QStringLiteral("color-scheme"), colorScheme},
          {QStringLiteral("font-antialiasing"), fontAntialiasing},
          {QStringLiteral("font-hinting"), fontHinting},
          {QStringLiteral("icon-theme"), iconTheme},
          {QStringLiteral("font-name"), fontName},
          {QStringLiteral("font-rgba-order"), fontRgbaOrder},
          {QStringLiteral("gtk-theme"), gtkTheme}}}};
}

void Settings::refreshWatches()
{
    const auto watchedFiles = m_watcher.files();
    auto files = m_configFiles;
    files.append(m_fontconfigFiles);
    for (const auto &file : files) {
        if (QFileInfo::exists(file) && !watchedFiles.contains(file)) {
            m_watcher.addPath(file);
        }
    }

    const auto watchedDirectories = m_watcher.directories();
    auto directories = m_configDirectories;
    directories.append(m_fontconfigDirectories);
    for (const auto &directory : directories) {
        if (QFileInfo(directory).isDir() && !watchedDirectories.contains(directory)) {
            m_watcher.addPath(directory);
        }
    }
}
