#ifndef UIUTILS_H
#define UIUTILS_H

#include "gui_global.h"
#include <QVariant>
#include <QPagedPaintDevice>

class Db;
class QWidget;
class QToolBar;
class QPlainTextEdit;
class QMenu;

GUI_API_EXPORT QString getDbPath(bool newFileMode, const QString& startWith = QString());
GUI_API_EXPORT QString getDbOrSqlPath(bool newFileMode, const QString &startWith = QString());
GUI_API_EXPORT QString getOpenFilePath(bool newFileMode, const QString &startWith = QString());
GUI_API_EXPORT void setValidState(QWidget* widget, bool valid, const QString& message = QString());
GUI_API_EXPORT void setValidStateWihtTooltip(QWidget* widget, const QString& tooltip, bool valid, const QString& message = QString());
GUI_API_EXPORT void setValidStateWarning(QWidget* widget, const QString& warning);
GUI_API_EXPORT void setValidStateInfo(QWidget* widget, const QString& info);
GUI_API_EXPORT void setValidStateTooltip(QWidget* widget, const QString& tip);
GUI_API_EXPORT bool isValidStateIndicatorVisible(QWidget* widget);
GUI_API_EXPORT const QStringList& getAllPageSizes();
GUI_API_EXPORT QString convertPageSize(QPageSize::PageSizeId size);
GUI_API_EXPORT QPageSize convertPageSize(const QString& size);
GUI_API_EXPORT QPixmap addOpacity(const QPixmap& input, float opacity);
GUI_API_EXPORT void limitDialogWidth(QDialog* dialog);
GUI_API_EXPORT void fixTextCursorSelectedText(QString& text);
GUI_API_EXPORT QColor styleSyntaxStringColor();
GUI_API_EXPORT QBrush styleEditorLineColor();
GUI_API_EXPORT void fixToolbarTooltips(QToolBar* toolbar);
GUI_API_EXPORT QColor findContrastingColor(const QColor& input);
GUI_API_EXPORT void enrichTextEditContextMenu(QPlainTextEdit* editor, std::function<void(QPlainTextEdit*,QMenu*)> enrichFunc);
GUI_API_EXPORT void addFormatSqlToContextMenu(QPlainTextEdit* editor, std::function<bool(QPlainTextEdit*)> actionCondition = nullptr);
GUI_API_EXPORT void formatSqlInTextEdit(QPlainTextEdit* editor, Db* db = nullptr);

/**
 * @brief Interface for classes that require explicit initialization after construction.
 *
 * Used by AbstractTableWindow to indicate required call during session restoring. Maybe by other classes in future.
 */
class RequiresExplicitInit
{
    public:
        virtual void init() = 0;
};

// This is a hack. For example we want to display "Ctrl+W" shortcut to the user in this menu, but assigning that shortcut
// permanently to the action makes it ambigous to Qt, because it's already a standard shortcut,
// thus making Qt confused and this shortcut working only every second time.
// Here we assign the shortcut only for the time of displaying the menu. Rest of the time it's not assigned.
#define CONFLICTING_MENU_HOTKEY_WORKAROUND(Menu, KeySeq, Action) \
    connect(Menu, &QMenu::aboutToShow, this, [this]() \
    { \
        QList<QKeySequence> bindings = QKeySequence::keyBindings(KeySeq); \
        if (bindings.size() > 0) \
            Action->setShortcut(bindings.first()); \
    }); \
    connect(Menu, &QMenu::aboutToHide, this, [this]() \
    { \
        Action->setShortcut(QKeySequence()); \
    });


#define UI_PROP_COLUMN "column_name"

#endif // UIUTILS_H
