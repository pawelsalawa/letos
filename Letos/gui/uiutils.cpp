#include "uiutils.h"
#include "services/codeformatter.h"
#include "services/config.h"
#include "common/widgetstateindicator.h"
#include "uiconfig.h"
#include "iconmanager.h"
#include "sqleditor.h"
#include <QObject>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QDateTimeEdit>
#include <QFileDialog>
#include <QStringList>
#include <QSet>
#include <QDebug>
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QScreen>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QPlainTextEdit>

const QList<QPageSize::PageSizeId> pageSizeIds = {
    QPageSize::A4, QPageSize::B5, QPageSize::Letter, QPageSize::Legal, QPageSize::Executive, QPageSize::A0, QPageSize::A1,
    QPageSize::A2, QPageSize::A3, QPageSize::A5, QPageSize::A6, QPageSize::A7, QPageSize::A8, QPageSize::A9, QPageSize::B0,
    QPageSize::B1, QPageSize::B10, QPageSize::B2, QPageSize::B3, QPageSize::B4, QPageSize::B6, QPageSize::B7, QPageSize::B8,
    QPageSize::B9, QPageSize::C5E, QPageSize::Comm10E, QPageSize::DLE, QPageSize::Folio, QPageSize::Ledger, QPageSize::Tabloid,
    QPageSize::Custom
};

const QStringList pageSizes = pageSizeIds | MAP_NO_CAP(id, {
    return QPageSize::name(id);
});

const QStringList pageSizesWithDimensions;

QString getDbPath(bool newFileMode, const QString &startWith, const QStringList& filters, const QString& dialogTitle)
{
    QString dir = startWith;
    if (dir.isNull())
        dir = CFG->get("dialogCache", "lastDbDir").toString();

    QFileDialog dialog(nullptr, dialogTitle, dir, QString());
    dialog.setAcceptMode(newFileMode ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);

    /* As we don't actually overwrite a selected existing database file, switch off the
     * overwrite warning.
     * FIXME: QFileDialog::DontConfirmOverwrite does not work on MacOS native dialogs.
     * Probably some better UX is needed.
     */
    dialog.setOption(QFileDialog::DontConfirmOverwrite, true);

    dialog.setLabelText(QFileDialog::Accept, QObject::tr("Select"));
    dialog.setLabelText(QFileDialog::FileType, QObject::tr("File type"));
    dialog.setNameFilters(filters);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().empty())
        return QString();

    return dialog.selectedFiles().constFirst();
}

QString getDbPath(bool newFileMode, const QString &startWith)
{
    OpenFileFilters::Filters filters = OpenFileFilters::get();
    return getDbPath(newFileMode, startWith, {
                         filters.sqlite,
                         filters.all
                     },
                     newFileMode ? QObject::tr("Select new database file") : QObject::tr("Select database file"));
}

QString getDbOrSqlPath(bool newFileMode, const QString &startWith)
{
    OpenFileFilters::Filters filters = OpenFileFilters::get();
    return getDbPath(newFileMode, startWith, {
                         filters.sqliteOrSql,
                         filters.sqlite,
                         filters.sql,
                         filters.all
                     }, QObject::tr("Select a file to open"));
}

QString getOpenFilePath(bool newFileMode, const QString &startWith)
{
    OpenFileFilters::Filters filters = OpenFileFilters::get();
    return getDbPath(newFileMode, startWith, {
                         filters.sqliteOrSql,
                         filters.sqlite,
                         filters.sql,
                         filters.csv,
                         filters.text,
                         filters.all
                     }, QObject::tr("Select a file to open"));
}

void setValidState(QWidget *widget, bool valid, const QString& message)
{
    INDICATOR(widget)->setMode(WidgetStateIndicator::Mode::ERROR);
    INDICATOR(widget)->setVisible(!valid, valid ? QString() : message);
}

void setValidStateWihtTooltip(QWidget* widget, const QString& tooltip, bool valid, const QString& message)
{
    if (!valid)
    {
        INDICATOR(widget)->setMode(WidgetStateIndicator::Mode::ERROR);
        INDICATOR(widget)->setVisible(true, message);
    }
    else
    {
        INDICATOR(widget)->setMode(WidgetStateIndicator::Mode::HINT);
        INDICATOR(widget)->setVisible(widget->isEnabled(), tooltip);
    }
}

void setValidStateWarning(QWidget* widget, const QString& warning)
{
    INDICATOR(widget)->setMode(WidgetStateIndicator::Mode::WARNING);
    INDICATOR(widget)->setVisible(widget->isEnabled(), warning);
}

void setValidStateInfo(QWidget* widget, const QString& info)
{
    INDICATOR(widget)->setMode(WidgetStateIndicator::Mode::INFO);
    INDICATOR(widget)->setVisible(widget->isEnabled(), info);
}

void setValidStateTooltip(QWidget* widget, const QString& tip)
{
    INDICATOR(widget)->setMode(WidgetStateIndicator::Mode::HINT);
    INDICATOR(widget)->setVisible(widget->isEnabled(), tip);
}

bool isValidStateIndicatorVisible(QWidget* widget)
{
    return EXISTS_INDICATOR(widget) && INDICATOR(widget)->isVisible();
}

QString convertPageSize(QPageSize::PageSizeId size)
{
    return QPageSize::name(size);
}

QPageSize convertPageSize(const QString& size)
{
    return QPageSize(static_cast<QPageSize::PageSizeId>(pageSizes | INDEX_OF_STR(size, Qt::CaseInsensitive)));
}

const QStringList& getAllPageSizes()
{
    return pageSizes;
}

QPixmap addOpacity(const QPixmap& input, float opacity)
{
    QPixmap output(input.size());
    output.fill(Qt::transparent);
    QPainter p(&output);
    p.setOpacity(opacity);
    p.drawPixmap(0, 0, input);
    p.end();
    return output;
}

void limitDialogWidth(QDialog* dialog)
{
    dialog->setMaximumWidth(QGuiApplication::primaryScreen()->availableGeometry().width());
}

void fixTextCursorSelectedText(QString& text)
{
    text.replace("\u2029", "\n");
}

QColor styleSyntaxStringColor()
{
    static const QColor stdAltColor = QColor(Qt::green);
    if (QApplication::style()->standardPalette().text().color().lightness() >= 128)
        return stdAltColor.lighter();
    else
        return stdAltColor.darker();
}

QBrush styleEditorLineColor()
{
    QPalette palette = QApplication::style()->standardPalette();
    if (CFG_UI.General.Style.get().toLower() != "macintosh")
        return palette.alternateBase();

    if (palette.base().color().lightness() < 128)
        return QBrush(palette.alternateBase().color().darker(300));

    return palette.alternateBase();
}

void fixToolbarTooltips(QToolBar* toolbar)
{
    for (QAction*& action : toolbar->actions())
    {
        QToolButton* button = dynamic_cast<QToolButton*>(toolbar->widgetForAction(action));
        if (!button)
            continue;

        QString text = action->text();
        text = text.replace(QRegularExpression("\\s?\\(&.\\)$"),""); // issue #4261, for Chinese
        text = text.replace("&", ""); // issue #4218
        if (!action->shortcut().isEmpty())
            text += QString(" (%1)").arg(action->shortcut().toString());

        button->setToolTip(text);
    }
}

QColor findContrastingColor(const QColor& input)
{
    if (!input.isValid())
        return input;

    auto channel = [](int v)
    {
        double c = v / 255.0;
        return (c <= 0.03928) ? (c / 12.92)
                              : std::pow((c + 0.055) / 1.055, 2.4);
    };

    double inputLum = (0.2126 * channel(input.red())
                       + 0.7152 * channel(input.green())
                       + 0.0722 * channel(input.blue()));

    bool isDark = inputLum < 0.15;
    return isDark ? QColor(240,240,240) : QColor(30,30,30);
}

void formatSqlInTextEdit(QPlainTextEdit* editor, Db* db)
{
    QTextCursor cur = editor->textCursor();
    QString sql = cur.hasSelection() ? cur.selectedText() : editor->toPlainText();
    fixTextCursorSelectedText(sql);

    sql = LETOS->getCodeFormatter()->format("sql", sql, db);

    if (!cur.hasSelection())
        editor->selectAll();

    // After potential change in selection, we need to replace contents based on the new cursor
    editor->textCursor().insertText(sql);
}

void enrichTextEditContextMenu(QPlainTextEdit* editor, std::function<void(QPlainTextEdit*,QMenu*)> enrichFunc)
{
    editor->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(editor, &QWidget::customContextMenuRequested, editor, [editor, enrichFunc](const QPoint &pos)
    {
        QMenu *menu = editor->createStandardContextMenu();
        menu->setParent(editor);
        enrichFunc(editor, menu);
        menu->exec(editor->mapToGlobal(pos));
        delete menu;
    });
}

void addFormatSqlToContextMenu(QPlainTextEdit* editor, std::function<bool(QPlainTextEdit*)> actionCondition)
{
    QAction* formatSqlAction = new QAction(ICONS.FORMAT_SQL, QObject::tr("Format SQL"), editor);
    editor->addAction(formatSqlAction);

    formatSqlAction->setShortcutContext(Qt::WidgetShortcut);
    formatSqlAction->setShortcut(QKeySequence(CFG_KEYS_INSTANCE(SqlEditor).FORMAT_SQL.get()));

    enrichTextEditContextMenu(editor, [formatSqlAction, actionCondition](QPlainTextEdit* editor, QMenu* menu)
    {
        if (actionCondition && !actionCondition(editor))
            return;

        auto actions = menu->actions();
        if (actions.isEmpty())
        {
            menu->addAction(formatSqlAction);
        }
        else
        {
            auto sep = menu->insertSeparator(actions.first());
            menu->insertAction(sep, formatSqlAction);
        }
    });

    QObject::connect(formatSqlAction, &QAction::triggered, [editor, actionCondition]()
    {
        if (actionCondition && !actionCondition(editor))
            return;

        formatSqlInTextEdit(editor);
    });
}

void scanForCustomFonts()
{
    QStringList fontDirs;
    if (getDistributionType() != DistributionType::OS_MANAGED)
        fontDirs += qApp->applicationDirPath() + "/fonts";

    fontDirs += QDir(CFG->getConfigDir()).absoluteFilePath("fonts");

    for (const QString& varName : {"LETOS_FONTS"})
    {
        QString envDirs = LETOS->getEnv(varName);
        if (!envDirs.isNull())
            fontDirs += envDirs.split(PATH_LIST_SEPARATOR);
    }

    for (const QString& fontDirPath : fontDirs)
    {
        QDir fontDir(fontDirPath);
        if (!fontDir.exists())
            continue;

        for (QFileInfo& fileInfo : fontDir.entryInfoList({"*.ttf", "*.otf"}, QDir::Files))
        {
            QString fileName = fileInfo.absoluteFilePath();
            int id = QFontDatabase::addApplicationFont(fileName);
            if (id == -1)
                qWarning() << "Failed to load custom font" << fileName;
            else
                qDebug() << "Loaded custom font" << fileName;
        }
    }
}

